# Copyright (c) 2017-2025 The Forge Interactive Inc.
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

""" metal shader generation """

from utils import WaveopsFlags, Stages, Features, Platforms, ShaderBinary
from utils import getHeader, getShader, getMacro, getMacroName, iter_lines, isBaseType
from utils import isArray, getArrayLen , getArrayBaseName, get_interpolation_modifier
from utils import is_input_struct, get_input_struct_var, fsl_assert, get_whitespace, getArrayLenFlat
from utils import get_array_decl, get_fn_table, is_groupshared_decl, collect_shader_decl, line_is_srt_declaration, get_srt_tuple
import os, sys, re
from shutil import copyfile

targetToMslEntry = {
    Stages.VERT: 'vertex',
    Stages.FRAG: 'fragment',
    Stages.COMP: 'kernel',
}

def typeToMember(name):
    return 'm_'+name

def ios(*args):
    return metal(Platforms.IOS, *args)

def macos(*args):
    return metal(Platforms.MACOS, *args)

class Opts:
    def __init__(self, debug):
        self.incremental = False
        self.debug = debug
        self.includes = None

def metal(platform: Platforms, debug, binary: ShaderBinary, dst):
    fsl = binary.preprocessed_srcs[platform]

    shader = getShader(platform, binary, fsl, dst)
    msl_target = targetToMslEntry[shader.stage]
    binary.waveops_flags = shader.waveops_flags

    shader_src = getHeader(fsl)
    shader_src += ['#define METAL\n']
    shader_src += [f'#define TARGET_{platform.name}\n']

    if shader.waveops_flags != WaveopsFlags.WAVE_OPS_NONE:
        shader_src += ['#define ENABLE_WAVEOPS(flags)\n']
    

    if Features.RAYTRACING in binary.features:
        shader_src += [
            '#include <metal_raytracing>\n',
            'using namespace metal::raytracing;\n',
            'using metal::raytracing::instance_acceleration_structure;\n',
        ]
        
    # directly embed metal header in shader
    shader_src += ['#include "includes/metal.h"\n']

    # since 0 and 1 are reserved for markers, 2 to 5 for argument buffers
    # other buffers begin from 6
    ids = [6,0,0]

    def consumeIdAt(i, inc):
        val = ids[i]
        ids[i] += inc
        return val

    def getBufferId(array_len):
        return consumeIdAt(0, array_len)
    def getTextureId(array_len):
        return consumeIdAt(1, array_len)
    def getSamplerId(array_len):
        return consumeIdAt(2, array_len)

    def is_cbuffer_embedded(binary, parsing_cbuffer):
        return Features.ICB in binary.features or Features.RAYTRACING in binary.features

    def cleaneval(exp: str):
        exp = re.sub(r"[A-Za-z]"," ",exp)
        return eval(exp)
    # shader resource table sets
    srt_sets = [
    ]

    # collect array resources
    ab_elements = {}

    global_references = {}
    global_reference_paths = {}
    global_reference_args = {}
    global_fn_table = get_fn_table(shader.lines)

    skip = False
    struct = None
    mainArgs = []
    mainArgsNames = []

    # statements which need to be inserted into INIT_MAIN
    entry_declarations = []
    attribute_index  = 0

    # reversed list of functions, used to determine where a resource is being accessed
    reversed_fns = list(reversed(list(global_fn_table.items())))
    res_type_index = 0
    res_name_index = 1
    parsing_main = False
    global_scope_count = 0
    global_resources = [res[res_name_index] for res in shader.resources]
    reGlobalResources = []

    reFn = re.compile(r'\b\w+\s+(\w+)\s*\(([^\{;]*)\)\s*?\{', re.DOTALL)

    res = {}
    for m in reFn.finditer(''.join(shader.lines)):
        a = m.group(2).split(',')
        if not m.group(2): continue
        a = [ a.split()[-1] for a in a ]
        if m.group(1) not in res: res[m.group(1)] = []
        res[m.group(1)] += a
    for i, line in enumerate(shader.lines):
        if line.strip().startswith('//'): continue

        if is_groupshared_decl(line):
            dtype, dname = getMacro(line)
            basename = getArrayBaseName(dname)
            if basename not in global_resources:
                global_resources += [basename]

        if '{' in line: global_scope_count += 1
        if '}' in line: global_scope_count -= 1

        if global_scope_count == 0: continue

        for global_resource in global_resources:
            resource_name = getArrayBaseName(global_resource)
            l_get = line.find(resource_name)
            while l_get > 0 and not parsing_main:
                resource = line[l_get:]
                resource = resource_name #resource[4:resource.find(')')]
                br = resource.find('[')
                if -1 != br:
                    resource = resource[:br]
                for fn, (_, fn_i) in reversed_fns:
                    if 'GetMetalIntersectionParams' == fn: continue
                    if fn_i[0] < i:
                        if fn not in global_references: global_references[fn] = set()
                        if not fn in res or resource not in res[fn]:
                            global_references[fn].add(resource)
                        break
                l_get = line.find(resource_name, l_get+1)

        l_get = line.find('WaveGetLaneIndex()')
        while l_get > 0 and not parsing_main:
            resource = 'simd_lane_id'
            for fn, (_, fn_i) in reversed_fns:
                if fn_i[0] < i:
                    if fn not in global_references: global_references[fn] = set()
                    global_references[fn].add(resource)
                    break
            l_get = line.find('Get(', l_get+1)

        if not parsing_main:
            global_references_tmp = list(global_references.items())
            for fn, resource in global_references_tmp:
                l_call = line.find(fn+'(')
                if l_call > 0:
                    for fn_caller, (_, fn_i) in reversed_fns:
                        if fn_i[0] < i:
                            if fn_caller not in global_references: global_references[fn_caller] = set()
                            global_references[fn_caller].update(resource)
                            break

        if '_MAIN(' in line:
            parsing_main = True

    if shader.waveops_flags != WaveopsFlags.WAVE_OPS_NONE:
        global_reference_paths['simd_lane_id'] = 'simd_lane_id'
        global_reference_args['simd_lane_id'] = 'const uint simd_lane_id'

    def declare_argument_buffers(mainArgs, mainArgsNames):
        ab_decl = []
        # declare argument buffer structs
        # 0 and 1 are reserved for markers, 2 to 5 for argument buffers
        # Keep in sync with DESCRIPTOR_SET_ARGUMENT_BUFFER_START_INDEX in Common_3/Graphics/Metal/MetalRenderer.mm
        DESCRIPTOR_SET_ARGUMENT_BUFFER_START_INDEX = 2
        buffer_index = DESCRIPTOR_SET_ARGUMENT_BUFFER_START_INDEX
        for freq, elements in ab_elements.items():

            # skip empty update frequencies
            if not elements:
                buffer_index = buffer_index + 1
                continue

            argBufType = 'SRTSet' + freq

            # make AB declaration only active if any member is defined
            space = 'constant'
            ifreq = srt_sets.index(freq)
            mainArgs.insert(buffer_index, [space, ' struct ', argBufType, '& ', argBufType, f'[[buffer({buffer_index})]]'])
            mainArgsNames.insert(buffer_index, argBufType)
            buffer_index = buffer_index + 1
            ab_decl += ['struct ', argBufType, '\n{\n']
            for elem in elements:
                binding = cleaneval(elem[0].split('=')[-1])
                arr = 1 if not isArray(elem[1][2]) else cleaneval(getArrayLenFlat(elem[1][2]))
                ab_decl += ['\t', *elem[1], ';\n']

            if len(elements) == 1:
                _, elem0 = elements[0]
                if 'SamplerState' in elem0[0]:
                    print('WARN: Sampler-only AB not supported!')

            ab_decl += ['};\n']
        return ab_decl
    
    last_res_decl = 0
    explicit_res_decl = None
    global_scope_count = 0
    parsing_srt = False
    
    is_no_ab = False

    main_entry_line_index = -1
    main_body_line_index = -1
    for fi, line_index, line in iter_lines(shader.lines):

        if '{' in line: global_scope_count += 1
        if '}' in line: global_scope_count -= 1
        
        shader_src_len = len(shader_src)

        # TODO: improve this
        if '#ifdef NO_FSL_DEFINITIONS' in line:
            skip = True
        if skip and '#endif' in line:
            skip = False
            continue
        if skip:
            continue

        if line.strip().startswith('STRUCT('):
            struct = getMacro(line)
        if struct and '};' in line:
            struct = None

        if 'EARLY_FRAGMENT_TESTS' in line:
            line = '[[early_fragment_tests]]\n'

        #  Shader I/O
        if struct and line.strip().startswith('DATA('):
            if shader.returnType and struct in shader.returnType:
                var = getMacro(shader.returnType)
                dtype, name, sem = getMacro(line)
                sem = sem.upper()
                base_name = getArrayBaseName(name)

                
                interpolation_modifier = get_interpolation_modifier(dtype)
                if interpolation_modifier: # consume modifier for metal vert
                    dtype = getMacro(dtype)

                output_semantic = ''
                if 'SV_POSITION' in sem:
                    output_semantic = '[[position]]'
                    if Features.INVARIANT in binary.features:
                        output_semantic = '[[position, invariant]]'
                if 'SV_POINTSIZE' in sem:
                    output_semantic = '[[point_size]]'
                if 'SV_DEPTH' in sem:
                    output_semantic = '[[depth(any)]]'
                if 'SV_RENDERTARGETARRAYINDEX' in sem:
                    output_semantic = '[[render_target_array_index]]'
                if 'SV_COVERAGE' in sem:
                    output_semantic = '[[sample_mask]]'

                color_location = sem.find('SV_TARGET')
                if color_location > -1:
                    color_location = sem[color_location+len('SV_TARGET'):]
                    if 'uint' in dtype:
                        binary.output_types_mask |= (1 << int(color_location))
                    if not color_location: color_location = '0'
                    output_semantic = '[[color('+color_location+')]]'

                shader_src += ['#line {}\n'.format(line_index)]
                shader_src += [get_whitespace(line), dtype, ' ', name, ' ', output_semantic, ';\n']
                continue

            elif is_input_struct(struct, shader):
                var = get_input_struct_var(struct, shader)
                dtype, name, sem = getMacro(line)
                sem = sem.upper()
                # for vertex shaders, use the semantic as attribute name (for name-based reflection)
                n2 = sem if shader.stage == Stages.VERT else getArrayBaseName(name)
                if isArray(name):
                    base_name = getArrayBaseName(name)
                    array_length = int(cleaneval(getArrayLen(shader.defines, name)))
                    assignment = []
                    for i in range(array_length): assignment += ['\t_',var, '.', base_name, '[', str(i), '] = ', var, '.', base_name, '_', str(i), '; \\\n']
                    # TODO: handle this case
                
                attribute = ''
                if shader.stage == Stages.VERT:
                    attribute = '[[attribute('+str(attribute_index)+')]]'
                    attribute_index += 1
                elif shader.stage == Stages.FRAG:
                    attribute = ''
                    interpolation_modifier = get_interpolation_modifier(dtype)
                    if 'SV_POSITION' in sem:
                        attribute = '[[position]]'
                        entry_declarations += [f'{var}.{name}.w = rcp({var}.{name}.w);\n']
                    elif 'SV_RENDERTARGETARRAYINDEX' in sem:
                        attribute = '[[render_target_array_index]]'
                    elif interpolation_modifier:
                        attribute = f'[[{interpolation_modifier}]]'
                        dtype = getMacro(dtype) # consume modifier
                shader_src += ['#line {}\n'.format(line_index)]
                shader_src += [get_whitespace(line), dtype, ' ', name, ' ', attribute, ';\n']
                continue

        if is_groupshared_decl(line):
            dtype, dname = getMacro(line)
            basename = getArrayBaseName(dname)
            shader_src += ['\n\t// Metal GroupShared Declaration: ', basename, '\n']

            entry_declarations += [['threadgroup {} {};'.format(dtype, dname)]]

            array_decl = get_array_decl(dname)

            global_reference_paths[basename] = basename
            global_reference_args[basename] = 'threadgroup ' + dtype + ' (&' + basename + ')' + array_decl

            shader_src += ['#line {}\n'.format(line_index)]
            shader_src += ['\t// End of GroupShared Declaration: ', basename, '\n']
            continue

        if 'BEGIN_SRT_NO_AB' in line and parsing_srt is False:
            is_no_ab = True
        if 'BEGIN_SRT' in line and parsing_srt is False:
            parsing_srt = True
        if 'END_SRT(' in line:
            parsing_srt = False
            explicit_res_decl = len(shader_src)+1
            
        # consume resources
        if line_is_srt_declaration(line):
            resource = get_srt_tuple(line)
            resType, resName, freq = resource[:3]
            baseName = getArrayBaseName(resName)
            is_embedded = True
            # RW resources remain out og any Arguemnt buffers
            if 'RW' in resType or resType.startswith('WTex') or resType.startswith('RTex3D'):
                is_embedded = False
            else:
                is_embedded = True
            
            if is_no_ab:
                is_embedded = False
                #if isArray(resName):
                #    is_embedded = True
                if 'Buffer' in resType and resType.startswith('CBUFFER') is False and is_no_ab is False:
                    is_embedded = True
                if 'RW' in resType or resType.startswith('WTex') or resType.startswith('RTex3D'):
                    is_embedded = False
                
            # some cases when more than 8 RW buffers are needed , we enforce AB
            is_embedded = is_embedded or Features.ICB in binary.features or Features.RAYTRACING in binary.features
            
            # but samplers remain out of AB, for low end devices support
            if 'SamplerState' in resType or 'SamplerComparisonState' in resType:
                is_embedded = False
                
            if freq not in srt_sets:
                srt_sets.append(freq)
                ab_elements[freq] = []

            if not is_embedded: # regular resource
                is_array = isArray(resName)
                array_len = int(cleaneval(getArrayLenFlat(resName))) if is_array else 1
                prefix = ''
                postfix = ' '
                is_buffer = 'Buffer' in resType or 'CBUFFER' in resType

                fr = srt_sets.index(resource[2])
                nameWithFrequency = f'_fslF{fr}_{baseName}'

                if is_buffer:
                    prefix = 'device ' if 'RW' in resType else 'const device '
                    postfix = '* '
                    if 'CBUFFER' in resType:
                        postfix = '& '
                        prefix = 'constant '
                if 'Sampler' in resType:
                    location = getSamplerId(array_len)
                    binding = ' [[sampler({})]]'.format(location)
                elif 'RasterizerOrderedTex2D' in resType:
                    location = getTextureId(array_len)
                    _, group_index = getMacro(resType)
                    binding = ' [[raster_order_group({0}), texture({1})]]'.format(group_index, location)
                elif 'Tex' in getMacroName(resType) or 'Depth' in getMacroName(resType):
                    location = getTextureId(array_len)
                    binding = ' [[texture({})]]'.format(location)
                elif is_buffer:
                    location = getBufferId(array_len)
                    binding = ' [[buffer({})]]'.format(location)
                else:
                    fsl_assert(False, fi, line_index, message=f"Unknown Resource location: {resource}")

                mainArgType = resType

                if is_array and is_buffer: # unroll buffer array
                    main_arg = [prefix , mainArgType, postfix, f'_fslA{fr}{array_len}_' + baseName, binding, ' // main arg ', resource[2]]
                    mainArgs += [main_arg]
                    mainArgsNames += [baseName]
                    declaration = [f'_fslA{fr}{array_len}_' + baseName]
                    for i in range(1, array_len):
                        binding = ' [[buffer({})]]'.format(location + i)
                        main_arg = [prefix , mainArgType, postfix, '_fslS_' + baseName + f'_{i}', binding, ' // main arg ', resource[2]]
                        mainArgs += [main_arg]
                        declaration += [ '_fslS_' + baseName + f'_{i}' ]
                    entry_declarations += [f'{prefix}{mainArgType}{postfix} {baseName}[] = {{ {", ".join(declaration)} }};']
                else:
                    if is_array: # sampler / texture arrays
                        mainArgType = f'array<{resType}, {array_len}>'
                    main_arg = [prefix , mainArgType, postfix, nameWithFrequency, binding, ' // main arg ', resource[2]]
                    mainArgs += [main_arg]
                    mainArgsNames += [resName]
                    if 'CBUFFER' in resType:
                        entry_declarations += [f'constant {mainArgType}& {baseName} = {nameWithFrequency};']
                    else:
                        entry_declarations += [f'{prefix}{mainArgType}{postfix} {baseName} = {nameWithFrequency};']

                global_reference_paths[baseName] = baseName
                
                if is_buffer:
                    space = 'device' if 'RW' in resType else 'const device'
                    if is_array:
                        ref_arg = f'{space} {resType} *(&{baseName})[{array_len}] '
                    elif 'CBUFFER' in resType:
                        ref_arg = f'constant {resType}& {resName}'
                    else:
                        ref_arg = f'{space} {resType} *{resName}'
                else:
                    if is_array:
                        ref_arg = f'thread array<{resType}, {array_len}> (&{baseName}) '
                    else:
                        ref_arg = f'thread {resType}& {resName}'
                global_reference_args[baseName] = ref_arg

            else: # resource is embedded in argbuf

                argBufType = 'SRTSet' + freq
                basename = getArrayBaseName(resName)

                if 'CBUFFER' in resType:
                    ab_element = ['constant ', resType, '& ', resName]
                elif 'Buffer' in resType:
                    space = ' device ' if 'RW' in resType else 'const device '
                    ab_element = [space, resType, '* ', resName]
                elif 'RasterizerOrderedTex2D' in resType:
                    _, group_index = getMacro(resType)
                    ab_element = [resType, ' ', resName, ' ', f'[[raster_order_group({group_index})]]']
                else:
                    ab_element = [resType, ' ', resName]

                ab_elements[freq] += [( '0', ab_element )]

                global_reference_paths[baseName] = argBufType
                if not isArray(resName):
                    global_reference_args[baseName] = 'constant ' + resType + '& ' + resName
                    if 'Buffer' in resType:
                        global_reference_args[baseName] = 'constant ' + resType + '* ' + resName
                else:
                    array = resName[resName.find('['):]
                    global_reference_args[baseName] = 'constant ' + resType + '(&' + baseName+') ' + array
                global_reference_args[baseName] = 'constant struct ' + argBufType + '& ' + argBufType

                
                reGlobalResources += [ (f'{argBufType}.{baseName}', re.compile(fr'\b{baseName}\b', re.DOTALL)) ]
            last_res_decl = len(shader_src)+1
            shader_src += ['#line {}\n'.format(line_index)]
            continue

        # create comment hint for shader reflection
        if 'NUM_THREADS(' in line:
            elems = getMacro(line)
            for i, elem in enumerate(elems):
                if not elem.isnumeric():
                    assert elem in shader.defines, "arg {} to NUM_THREADS needs to be defined!".format(elem)
                    elems[i] = shader.defines[elem]
            binary.num_threads = [ int(d) for d in elems ]
            line = '// [numthreads({}, {}, {})]\n'.format(*elems)

        # convert shader entry to member function
        if '_MAIN(' in line:

            parsing_main = len(shader_src)
            ab_decl = declare_argument_buffers(mainArgs, mainArgsNames)
            ab_decl_location = last_res_decl if not explicit_res_decl else explicit_res_decl
            shader_src = shader_src[:ab_decl_location] + ab_decl + shader_src[ab_decl_location:]

            mtl_returntype = 'void'
            if shader.returnType:
                mtl_returntype = getMacroName(shader.returnType)

            shader_src += [msl_target, ' ', mtl_returntype, ' ', binary.filename.replace('.', '_'),  '(\n']
            main_entry_line_index = len(shader_src)
            prefix = '\t'
            for dtype, var in shader.struct_args:
                shader_src += [ prefix+dtype+' '+var+'[[stage_in]]\n']
                prefix = '\t,'
            for dtype, dvar in shader.flat_args:
                if 'SV_OUTPUTCONTROLPOINTID' in dtype.upper(): continue
                innertype = getMacro(dtype)
                semtype = getMacroName(dtype)
                shader_src += [prefix+innertype+' '+dvar+' '+semtype.upper()+'\n']
                prefix = '\t,'
                
            if shader.waveops_flags != WaveopsFlags.WAVE_OPS_NONE:
                shader_src += [prefix, 'uint simd_lane_id [[thread_index_in_simdgroup]]\n']
                shader_src += [prefix, 'uint simdgroup_size [[threads_per_simdgroup]]\n']
                prefix = '\t,'

            
            for arg in mainArgs:
                shader_src += [ "".join([ prefix, *arg, '\n'])]
                prefix = '\t,'

            shader_src += [')\n']
            shader_src += ['#line {}\n'.format(line_index)]
            continue

        if global_scope_count > 0:
            for ab, reGr in reGlobalResources:
                line = reGr.sub(ab, line)
        if 'INIT_MAIN' in line:

            for entry_declaration in entry_declarations:
                shader_src += [ "".join([ '\t', *entry_declaration, '\n'] ) ]

            main_body_line_index = len(shader_src)
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            continue

        if re.search(r'(^|\s+)RETURN', line):
            ws = get_whitespace(line)
            return_statement = [ ws+'{\n' ]

            # void entry, return nothing
            if not shader.returnType:
                return_statement += [ws+'\treturn;\n']

            else:
                return_value = getMacro(line)
                # entry declared with returntype, return var
                return_statement += [ws+'\treturn '+return_value+';\n']

            return_statement += [ ws+'}\n' ]
            shader_src += return_statement
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            continue

        if shader_src_len != len(shader_src):
            shader_src += ['#line {}\n'.format(line_index)]
            
        if line.strip().startswith('BEGIN_SRT(') or line.strip().startswith('END_SRT(') or line.strip().startswith('BEGIN_SRT_NO_AB('):
            continue;

        shader_src += [line]

    shader_src += ['\n']

    # for each function, expand signature and calls to pass global resource references
    for fn, references in global_references.items():
        insert_line, (_, insert_loc) = global_fn_table[fn]

        call_additions = []
        signature_additions = []
        for reference in references:
            if global_reference_paths[reference] not in call_additions:
                call_additions += [global_reference_paths[reference]]
                signature_additions += [global_reference_args[reference]]

        modified_signature = False

        for i, line in enumerate(shader_src):
            if line.strip().startswith('//'): continue

            # modify signatures
            l_call = line.find(fn+'(')
            if insert_line in line:
                for parameter in signature_additions:
                    if line[insert_loc-1:insert_loc+1] == '()':
                        line = line[:insert_loc] + parameter +  line[insert_loc:]
                    else:
                        line = line[:insert_loc] + parameter + ', ' +  line[insert_loc:]
                shader_src[i] = line
                modified_signature = True

            # modify calls
            elif modified_signature and l_call > 0 and line[l_call-1] in ' =\t(!':
                l2 = line.find(');', l_call)
                l2 = 0
                counter = 0
                for j, c in enumerate(line[l_call+len(fn):]):
                    if c == '(':
                        counter+=1
                        if counter == 1:
                            l2 = j+l_call+len(fn)+1
                            break
                    if c == ')':
                        counter-=1
                for argument in call_additions:
                    if line[l2-1:l2+1] == '()':
                        line = line[:l2] + argument + line[l2:]
                    else:
                        line = line[:l2] + argument + ', ' + line[l2:]
                shader_src[i] = line
    
    # optimization pass for generated code
    # determine the unused and unreferenced main args, and remove
    # them from main Args and internal delcarations
    
    def tokenize_line(line):
        token_pattern = r'\s*(\w+|==|!=|<=|>=|->|\+\+|--|&&|\|\||[{}()\[\];,+*/%&|^~!=<>?:.-])\s*'
        tokens = re.findall(token_pattern, line)
        return tokens
        
    def line_contains_reference(line, argName):
        tokens = tokenize_line(line)
        return argName in tokens

    def line_contains_res_reference(line, argName):
        tokens = tokenize_line(line)
        for curr_token in tokens:
            if curr_token.endswith( argName ):
                return True
        return False


    unused_arguments = []
    for j, argName in enumerate(mainArgsNames):
        ref_count = 0
        for i, line in enumerate(shader_src):
            stripped_line = line.strip()

            if i < main_body_line_index: continue
            if stripped_line.startswith('//'): continue
            if stripped_line.startswith('#'): continue
            if ( len( stripped_line ) == 0 ): continue
            if ( line_contains_reference( stripped_line, argName)):
                ref_count = ref_count + 1
        if ref_count == 0:
            unused_arguments += [argName]
            

    line_index = 0;
    first_arg = True
    for line in shader_src:
        if line_index >= main_entry_line_index and line_index < main_body_line_index:
            stripped_line = line.strip()
            line_removed = False
            #remove line with unused reference
            for curr_arg in unused_arguments:
                if line_contains_res_reference(stripped_line, curr_arg) is True:
                    shader_src[line_index] = ''
                    line_removed = True
            # make sure first arg does not have a leading comma
            if first_arg is True and line_removed is False:
                if stripped_line.startswith(','):
                    shader_src[line_index] = shader_src[line_index].replace(",","")
                first_arg = False
        line_index += 1


    open(dst, 'w').writelines(shader_src)
    return 0, []
