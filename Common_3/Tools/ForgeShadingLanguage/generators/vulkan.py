# Copyright (c) 2017-2024 The Forge Interactive Inc.
# 
# This file is part of The-Forge
# (see https://github.com/ConfettiFX/The-Forge).
# 
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# 
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

""" GLSL shader generation """

from utils import Stages, WaveopsFlags, Features, ShaderBinary, Platforms
from utils import isArray, getArrayLen, getArrayBaseName, fsl_assert, getHeader, getMacro
from utils import is_input_struct, get_input_struct_var, is_groupshared_decl, getShader, iter_lines
from utils import getMacroFirstArg, get_interpolation_modifier, getMacroName, get_whitespace, get_fn_table
import os, re

def BeginNonUniformResourceIndex(index, max_index=None):
    case_list = ['#define CASE_LIST ']
    if max_index:
        max_index = int(max_index)
        n_100 = max_index // 100
        n_10 = (max_index-n_100*100) // 10
        n_1 = (max_index-n_100*100-n_10*10) // 1
        for i in range(n_100):
            case_list += ['REPEAT_HUNDRED(',str(i*100), ') ']
        for i in range(n_10):
            case_list += ['REPEAT_TEN(',str(n_100*100 + i*10), ') ']
        for i in range(n_1):
            case_list += ['CASE(',str(n_100*100 + n_10*10 + i), ') ']
    else:
        case_list += ['CASE_LIST_256']
    return case_list + [
        '\n#define NonUniformResourceIndexBlock(', index,') \\\n'
    ]

def EndNonUniformResourceIndex(index):
    return  ['\n',
        '#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED\n',
        '\tNonUniformResourceIndexBlock(nonuniformEXT(', index, '))\n',
        '#elif VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED\n',
        '\tNonUniformResourceIndexBlock(', index, ')\n',
        '#else\n',
        '#define CASE(id) case id: ',
        'NonUniformResourceIndexBlock(id) break;\n',
        '\tswitch(', index, ') {CASE_LIST};\n',
        '#undef CASE\n#endif\n',
        '#undef NonUniformResourceIndexBlock\n',
        '#undef CASE_LIST\n',
    ]

def get_format_qualifier(name):
    elem_type = getMacroFirstArg(name)
    _map = {
        'uint64_t' : 'r64ui',
        'float4': 'rgba32f',
        'float2': 'rg32f',
        'float' : 'r32f',
        'uint4' : 'rgba32ui',
        'uint2' : 'rg32ui',
        'uint'  : 'r32ui',
        'int4'  : 'rgba32i',
        'int2'  : 'rg32i',
        'int'   : 'r32i',
        'half4' : 'rgba16f',
        'half2' : 'rg16f',
        'half'  : 'r16f',
        'float3'  : 'rgba8',
    }
    assert elem_type in _map, 'Cannot map {} to format qualifier'.format(elem_type)
    return _map[elem_type]

# helper to insert a level of indirection into buffer array expressions
# replacing: buffer_array[i][j] by buffer_array[i]._data[j]
def insert_buffer_array_indirections( line, buffer_name ):
    id_beg = line.find(buffer_name)        
    while id_beg > -1:
        # either the line starts with the identifier, or it is preceded
        # by a symbol that cannot be contained in an indentifier
        if id_beg == 0 or line[id_beg-1] in '(){}[]|&^, +-/*%:;,<>~!?=\t\n':
            id_beg += len(buffer_name)
            valid = True
            while id_beg < len(line):
                if line[id_beg] == '[': 
                    id_beg += 1
                    break
                if line[id_beg] not in ') \t\n':
                    valid = False
                    break
                id_beg += 1

            if not valid: continue

            # walk the string and count angle bracket occurences to handle
            # nested expressions: buffer_array[buffer_array[0][0]][0]
            num_br, id_end = 1, id_beg
            while id_end < len(line):
                num_br += 1 if line[id_end] == '[' else  -1 if line[id_end] == ']' else 0
                id_end += 1
                if num_br == 0: 
                    line = line[:id_end] + '._data' + line[id_end:]
                    break
        id_beg = line.find(buffer_name, id_beg + 1)
    return line

def is_buffer(fsl_declaration):
    # TODO: endswith better here?
    if 'CBUFFER' in fsl_declaration[0] or 'ROOT_CONSTANT' in fsl_declaration[0]:
        return True
    return 'Buffer' in fsl_declaration[0]

def declare_buffer(fsl_declaration):
    
    buffer_type, name, freq, _, binding = fsl_declaration
    data_type  = getMacro(buffer_type)
    if 'ByteBuffer' in buffer_type:
        data_type = 'uint'
        
    access = 'readonly'
    if buffer_type[:2] == 'W': access = 'writeonly'
    if buffer_type[:2] == 'RW': access = ''
    if 'CBUFFER' in buffer_type: access = 'uniform'

    if 'Coherent' in buffer_type:
        access += ' coherent'

    if isArray(name):
        # for arrays of buffers, expressions get transformed by the script
        array_name = getArrayBaseName(name)
        return ['layout (std430, {}, {}) {} buffer {}Block\n'.format(freq, binding, access, array_name),
                '{{\n\t{} _data[];\n}} {};\n'.format(data_type, name)]
    elif 'CBUFFER' in buffer_type:
        return ['layout (std140, {}, {}) uniform {}\n'.format(freq, binding, name),
                '{{\n\t{} {}_data;\n}};\n'.format(data_type, name),
                '#define {0} {0}_data\n'.format(name)]
    elif 'ROOT_CONSTANT' in buffer_type:
        return ['layout (push_constant) uniform {}Block '.format(name),
                '{{ {} data;}} {}; \n'.format(data_type, name, name),
                ]
    else:
        # for regular buffers, we redefine the access
        return ['layout (std430, {}, {}) {} buffer {}\n'.format(freq, binding, access, name),
                '{{\n\t{} {}_data[];\n}};\n'.format(data_type, name),
                '#define {0} {0}_data\n'.format(name)]

# writeable textures get translated to glsl images
def is_rw_texture(fsl_declaration):
    dtype = fsl_declaration[0]
    writeable_types = [
        'RasterizerOrderedTex2D',
        'RasterizerOrderedTex2DArray',
        'RTex1D',
        'RTex2D',
        'RTex3D',
        'RTex1DArray',
        'RTex2DArray',
        'WTex1D',
        'WTex2D',
        'WTex3D',
        'WTex1DArray',
        'WTex2DArray',
        'RWTex1D',
        'RWTex2D',
        'RWTex3D',
        'RWTex1DArray',
        'RWTex2DArray',
    ]
    return getMacroName(dtype) in writeable_types

def declare_rw_texture(fsl_declaration):
    tex_type, tex_name, freq, _, binding = fsl_declaration
    access = ''
    if tex_type.startswith('WT'):
        access = 'writeonly'
    if tex_type.startswith('RT'):
        access = 'readonly'
    return [
        'layout(', freq, ', ', binding, ', ', get_format_qualifier(tex_type), ') ',
        access, ' uniform ', tex_type, ' ', tex_name, ';\n'
    ]

def gen_passthrough_gs(shader, dst):
    gs = getHeader(dst)
    gs += ['// generated pass-through gs\n']
    gs += ['#version 450\n']
    gs += ['#extension GL_GOOGLE_include_directive : require\n']
    gs += ['#include "includes/vulkan.h"\n']
    gs += ['layout (triangles) in;\n']
    gs += ['layout (triangle_strip, max_vertices = 3) out;\n']

    ll = 0
    assignment = []
    for struct, _ in shader.struct_args:
        for e in shader.structs[struct]:
            sem = e[2].upper()
            if sem in {'SV_POSITION', 'SV_POINTSIZE', 'SV_DEPTH', 'SV_RENDERTARGETARRAYINDEX'}:
                continue
            gs += [f'layout(location = {ll}) in {e[0]} in_{e[1]}[];\n']
            gs += [f'layout(location = {ll}) out {e[0]} out_{e[1]};\n']
            assignment += [f'\t\tout_{e[1]} = in_{e[1]}[i];\n']
            ll += 1
    gs += ['\nvoid main()\n{\n\tfor(int i = 0; i < 3; i++)\n\t{\n']
    gs += ['\t\tgl_Position = gl_in[i].gl_Position;\n']
    gs += ['\t\tgl_PrimitiveID = gl_PrimitiveIDIn;\n']
    gs += assignment
    gs += ['\t\tEmitVertex();\n\t}\n}\n']
    open(dst, 'w').writelines(gs)

def quest(*args):
    return glsl(Platforms.QUEST, *args)

def vulkan(*args):
    return glsl(Platforms.VULKAN, *args)

def switch(*args):
    return glsl(Platforms.SWITCH, *args)

def android_vulkan(*args):
    return glsl(Platforms.ANDROID_VULKAN, *args)

def glsl(platform: Platforms, debug, binary: ShaderBinary, dst):

    binary.derivatives[platform] = [['VK_EXT_DESCRIPTOR_INDEXING_ENABLED=0', 'VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED=0']]

    shader = getShader(platform, binary, binary.preprocessed_srcs[platform], dst)
    # check for function overloading.
    get_fn_table(shader.lines)

    binary.waveops_flags = shader.waveops_flags

    dependencies = []

    shader_src = getHeader(binary.preprocessed_srcs[platform])
    version = 450
    extensions = ['GL_GOOGLE_include_directive']

    if Features.RAYTRACING in binary.features:
        version = max(version, 460)
        extensions += [
            'GL_EXT_ray_query',
            'GL_EXT_ray_flags_primitive_culling',
        ]

    if Platforms.QUEST == platform:
        if Features.MULTIVIEW in binary.features:
            extensions += [
                "GL_OVR_multiview2"
            ]

    shader_src += [f'#version {version} core\n']
    for ext in extensions:
        shader_src += [f'#extension {ext} : require\n']

    shader_src += [ 'precision highp float;\nprecision highp int;\n\n']
    shader_src += ['#define STAGE_', shader.stage.name, '\n']

    if Platforms.QUEST == platform:
        shader_src += ['#define TARGET_QUEST\n']

    if shader.waveops_flags != WaveopsFlags.WAVE_OPS_NONE:
        shader_src += ['#define ENABLE_WAVEOPS(flags)\n']
        for flag in list(WaveopsFlags):
            if flag not in shader.waveops_flags or flag == WaveopsFlags.WAVE_OPS_NONE or flag == WaveopsFlags.WAVE_OPS_ALL:
                continue
            shader_src += [ '#define {0}\n'.format(flag.name) ]

    if Features.INVARIANT in binary.features:
        shader_src += ['invariant gl_Position;\n']
    
    if Features.PRIM_ID in binary.features and binary.stage == Stages.FRAG:
        #  generate pass-through gs
        fn = dst.replace('frag', 'geom')

        gen_passthrough_gs(shader, fn)
        dependencies += [(Stages.GEOM, fn, os.path.basename(fn))]

    in_location = 0

    # directly embed vk header in shader
    header_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'includes', 'vulkan.h')
    header_lines = open(header_path).readlines()
    shader_src += header_lines + ['\n']

    out_location = 0

    arrBuffs = [getArrayBaseName(res[1]) for res in shader.resources if 'Buffer' in res[0] and (isArray(res[1]))]
    returnType = None if not shader.returnType else (getMacroName(shader.returnType), getMacro(shader.returnType))

    push_constant = None
    skip_line = False
    parsing_struct = None
    

    nonuniformresourceindex = None
    parsed_entry = False

    substitutions = {
         'min16float(' : 'float(',
        'min16float2(': 'float2(',
        'min16float3(': 'float3(',
        'min16float4(': 'float4(',
    }

    if shader.returnType and shader.returnType in shader.structs:
        for _, _, sem in shader.structs[shader.returnType]:
            sem = sem.upper()
            if sem == 'SV_RENDERTARGETARRAYINDEX':
                shader_src += ['#extension GL_ARB_shader_viewport_layer_array : enable\n\n']
                break

    struct_declarations = []
    input_assignments = []
    return_assignments = []

    subs = []

    for fi, line_index, line in iter_lines(shader.lines):

        shader_src_len = len(shader_src)
        
        if '@fsl_extension' in line:
            shader_src += [line.replace('@fsl_extension', '#extension')]
            continue

        # TODO: handle this differently
        if '#ifdef NO_FSL_DEFINITIONS' in line:
            skip_line = True
        if skip_line and '#endif' in line:
            skip_line = False
            continue
        if skip_line:
            continue

        for k, v in substitutions.items():
            l0 = line.find(k)
            while l0 > -1:
                line = line.replace(k, v)
                l0 = line.find(k)

        if line.strip().startswith('STRUCT('):
            parsing_struct = getMacro(line)
        if parsing_struct and '};' in line:
            if not line.endswith('\n'): line += '\n'
            shader_src += [line]

            for struct_declaration in struct_declarations:
                shader_src += [''.join(struct_declaration) + '\n']
            shader_src += ['#line {}\n'.format(line_index + 1)]

            struct_declarations = []

            parsing_struct = None
            continue

        # handle struct data declarations
        if parsing_struct and line.strip().startswith('DATA('):

            # handle entry return type
            if shader.returnType and parsing_struct in shader.returnType:
                var = 'out_'+shader.returnType
                elem_dtype, elem_name, sem = getMacro(line)

                sem = sem.upper()
                interpolation_modifier = get_interpolation_modifier(elem_dtype)
                if interpolation_modifier:
                    elem_dtype = getMacro(elem_dtype)
                    line = get_whitespace(line) + getMacro(elem_dtype)+' '+elem_name+';\n'

                basename = getArrayBaseName(elem_name)
                output_datapath = var + '_' + elem_name
                reference = None

                out_location_inc = 1

                if sem == 'SV_POSITION':
                    output_datapath = 'gl_Position'
                elif sem == 'SV_POINTSIZE': output_datapath = 'gl_PointSize'
                elif sem == 'SV_DEPTH': output_datapath = 'gl_FragDepth'
                elif sem == 'SV_RENDERTARGETARRAYINDEX' : output_datapath = 'gl_Layer'
                elif sem == 'SV_COVERAGE' : output_datapath = 'gl_SampleMask[0]'
                else:
                    output_prefix, out_postfix = '', ''
                    if interpolation_modifier:
                        output_prefix = f'{interpolation_modifier} '+output_prefix
                    reference = ['layout(location = ', str(out_location),') ', output_prefix, 'out(', elem_dtype, ') ', output_datapath, out_postfix, ';']

                # unroll attribute arrays
                if isArray(elem_name):
                    assignment = [var, '_', basename, ' = ', var, '.', basename, '']
                    out_location += getArrayLen(shader.defines, elem_name)
                else:
                    if sem != 'SV_POSITION' and sem !='SV_POINTSIZE' and sem !='SV_DEPTH' and sem != 'SV_RENDERTARGETARRAYINDEX':
                        out_location += out_location_inc
                    if output_datapath == 'gl_Layer':
                        assignment = [output_datapath, ' = int(', var, '.', elem_name, ')']
                    else:
                        assignment = [output_datapath, ' = ', var, '.', elem_name]

                if reference:
                    struct_declarations += [reference]
                return_assignments += [assignment]

            elif is_input_struct(parsing_struct, shader):
                var = get_input_struct_var(parsing_struct, shader)
                elem_dtype, elem_name, sem = getMacro(line)
                sem = sem.upper()
                interpolation_modifier = get_interpolation_modifier(elem_dtype)
                in_location_inc = 1
                if interpolation_modifier:
                    elem_dtype = getMacro(elem_dtype)
                    line = get_whitespace(line) + getMacro(elem_dtype)+' '+elem_name+';\n'
                is_array = isArray(elem_name)
                
                basename = getArrayBaseName(elem_name)
                input_datapath = var + '_' + elem_name
                
                if sem == 'SV_POINTSIZE' or sem == 'SV_RENDERTARGETARRAYINDEX': continue
                # for vertex shaders, use the semantic as attribute name (for name-based reflection)
                input_value = sem if shader.stage == Stages.VERT else var + '_' + elem_name

                in_prefix = ''
                if interpolation_modifier:
                    in_prefix = f' {interpolation_modifier} '+in_prefix

                reference = ['layout(location = ', str(in_location),')', in_prefix, ' in(', elem_dtype, ') ', input_value, ';']

                # input semantics
                if sem == 'SV_POSITION' and shader.stage == Stages.FRAG:
                    input_value = elem_dtype+'(float4(gl_FragCoord.xyz, 1.0f / gl_FragCoord.w))'
                    reference = []

                assignment = []

                if sem == 'SV_VERTEXID': input_value = 'gl_VertexIndex'
                # unroll attribute arrays
                # if is_array:
                #     assignment = ['for(int i=0; i<', str(getArrayLenFlat(elem_name)), ';i++) ']
                #     assignment += [var, '.', basename, '[i] = ', var, '_', basename, '[i]']
                # else:
                assignment = [var, '.', basename, ' = ',  input_value]

                if shader.stage == Stages.VERT and sem != 'SV_VERTEXID':
                    assignment += [';\n\t', sem]

                if sem != 'SV_POSITION':
                    in_location += in_location_inc
                if reference:
                    struct_declarations += [reference]
                input_assignments += [assignment]


        if is_groupshared_decl(line):
            dtype, dname = getMacro(line)
            basename = getArrayBaseName(dname)
            shader_src += ['#define _Get', basename, ' ', basename, '\n']
            line = 'shared '+dtype+' '+dname+';\n'

        if 'EARLY_FRAGMENT_TESTS' in line:
            line = 'layout(early_fragment_tests) in;\n'

        if 'EnablePSInterlock' in line:
            line = '#ifdef GL_ARB_fragment_shader_interlock\n' \
                'layout(pixel_interlock_ordered) in;\n' \
                '#endif\n'

        # handle push constants
        if line.strip().startswith('ROOT_CONSTANT'):
            # push_constant = getMacro(line)[0]
            push_constant = tuple(getMacro(line))
        if push_constant and '};' in line:
            shader_src += ['} ', push_constant[0], ';\n']
            for dt, dn, _ in shader.pushConstant[push_constant]:
                dn = getArrayBaseName(dn)
            push_constant = None
            continue

        resource_decl = None
        if line.strip().startswith('RES('):
            resource_decl = getMacro(line)
            fsl_assert(len(resource_decl) == 5, fi, line_index, message='invalid Res declaration: \''+line+'\'')
            basename = getArrayBaseName(resource_decl[1])
        
        # handle buffer resource declarations
        if resource_decl and is_buffer(resource_decl):
            shader_src += declare_buffer(resource_decl)
            if 'ROOT_CONSTANT' in resource_decl[0]:
                subs += [ (rf'[^}} ]\s*\b{resource_decl[1]}\b', '\g<0>.data') ]
            continue

        # handle references to arrays of structured buffers
        for buffer_array in arrBuffs:
            line = insert_buffer_array_indirections(line, buffer_array)

        # specify format qualified image resource
        if resource_decl and is_rw_texture(resource_decl):
            shader_src += declare_rw_texture(resource_decl)
            continue

        if 'BeginNonUniformResourceIndex(' in line:
            index, max_index = getMacro(line), None
            assert index != [], 'No index provided for {}'.format(line)
            if type(index) == list:
                max_index = ''.join( [c for c in index[1].strip() if c.isdigit()] )
                index = index[0]
            nonuniformresourceindex = index
            if max_index and not max_index.isnumeric():
                max_index = ''.join(c for c in shader.defines[max_index] if c.isdigit())
            shader_src += BeginNonUniformResourceIndex(nonuniformresourceindex, max_index)
            binary.derivatives[platform] += [
                ['VK_EXT_DESCRIPTOR_INDEXING_ENABLED=0', 'VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED=1'],
                ['VK_EXT_DESCRIPTOR_INDEXING_ENABLED=1', 'VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED=0'],
                ['VK_EXT_DESCRIPTOR_INDEXING_ENABLED=1', 'VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED=1'],
            ]
            continue
        if 'EndNonUniformResourceIndex()' in line:
            assert nonuniformresourceindex, 'EndNonUniformResourceIndex: BeginNonUniformResourceIndex not called/found'
            shader_src += EndNonUniformResourceIndex(nonuniformresourceindex)
            nonuniformresourceindex = None
            continue
        if nonuniformresourceindex :
            # skip preprocessor directive for unwrapped switch statement
            if not line.strip().startswith('#'):
                shader_src += [line[:-1], ' \\\n']
            continue

        if '_MAIN(' in line:

            if shader.returnType and shader.returnType not in shader.structs:
                shader_src += ['layout(location = 0) out(', shader.returnType, ') out_', shader.returnType, ';\n']

            shader_src += ['void main()\n']
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            parsed_entry = True
            continue

        if parsed_entry and re.search(r'(^|\s+)RETURN', line):
            ws = get_whitespace(line)
            output_statement = [ws+'{\n']
            if shader.returnType:
                output_value = getMacro(line)
                if shader.returnType not in shader.structs:
                    output_statement += [ws+'\tout_'+shader.returnType+' = '+output_value+';\n']
                else:
                    output_statement += [ws+'\t'+shader.returnType+' out_'+shader.returnType+' = '+output_value+';\n']
            for assignment in return_assignments:
                output_statement += [ws+'\t' + ''.join(assignment) + ';\n']

            output_statement += [ws+'\treturn;\n'+ws+'}\n']
            shader_src += output_statement
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            continue

        if 'INIT_MAIN' in line:
            post_init = None
            # assemble input
            for dtype, var in shader.struct_args:
                shader_src += ['\t' + dtype + ' ' + var + ';\n']
                
            for assignment in input_assignments:
                shader_src += ['\t' + ''.join(assignment) + ';\n']

            if post_init: shader_src += post_init
            
            ''' additional inputs '''
            for dtype, dvar in shader.flat_args:
                innertype = getMacro(dtype)
                semtype = getMacroName(dtype)
                shader_src += ['\tconst ' + innertype + ' ' + dvar + ' = ' + innertype + '(' + semtype.upper() + ');\n']

            ''' generate a statement for each vertex attribute
                this should not be necessary, but it influences
                the attribute order for the SpirVTools shader reflection
            '''
            # if shader.stage == Stages.VERT:# and shader.entry_arg:
            #     for dtype, var in shader.struct_args:
            #         for _, n, s in shader.structs[dtype]:
            #             if s.upper() == 'SV_VERTEXID': continue
            #             shader_src += ['\t', s ,';\n']
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            continue

        if shader_src_len != len(shader_src):
            shader_src += ['#line {}\n'.format(line_index)]

        shader_src += [line]

    shader_src = ''.join(shader_src)
    for pattern, repl in subs:
        shader_src = re.sub(pattern, repl, shader_src)

    open(dst, 'w').writelines(shader_src)
    return 0, dependencies
