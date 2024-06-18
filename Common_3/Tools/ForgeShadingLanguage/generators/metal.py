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

""" metal shader generation """

from utils import WaveopsFlags, Stages, DescriptorSets, Features, Platforms, ShaderBinary
from utils import getHeader, getShader, getMacro, getMacroName, iter_lines
from utils import isArray, getArrayLen , getArrayBaseName, get_interpolation_modifier
from utils import is_input_struct, get_input_struct_var, fsl_assert, get_whitespace, getArrayLenFlat
from utils import get_array_decl, get_fn_table, is_groupshared_decl, collect_shader_decl
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

    shader = getShader(platform, binary.fsl_filepath, fsl, dst)
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


    ids = [4,0,0]

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
        if 'rootcbv' in parsing_cbuffer[0]:
            return False
        return Features.ICB in binary.features or Features.RAYTRACING in binary.features

    def cleaneval(exp: str):
        exp = re.sub(r"[A-Za-z]"," ",exp)
        return eval(exp)
        
    # map dx register to main fn metal resource location
    def dxRegisterToMSLBinding(res: str, reg: str) -> str:
        register = int(reg[1:])

        # [0,3] reserved for ABs
        buffer_CBV_offset = 4
        buffer_UAV_offset = buffer_CBV_offset + 8
        buffer_SRV_offset = buffer_UAV_offset + 8
        texture_SRV_offset = 16

        if 's' == reg[0]: # samplers 1:1
            return register

        if 'b' == reg[0]: # CBV -> msl buffer 1:1
            return register + buffer_CBV_offset

        if "BUFFER" in res.upper():
            if 'u' == reg[0]:
                return register + buffer_UAV_offset
            return register + buffer_SRV_offset
        if 'u' == reg[0]: # SRV texture 1:1
            return register
        return register + texture_SRV_offset

    # for metal only consider resource declared using the following frequencies
    metal_ab_frequencies = [
        'UPDATE_FREQ_NONE',
        'UPDATE_FREQ_PER_FRAME',
        'UPDATE_FREQ_PER_BATCH',
        'UPDATE_FREQ_PER_DRAW',
        'UPDATE_FREQ_USER',
    ]

    # collect array resources
    ab_elements = {}
    for freq in metal_ab_frequencies:
        ab_elements[freq] = []

    global_references = {}
    global_reference_paths = {}
    global_reference_args = {}
    global_fn_table = get_fn_table(shader.lines)

    skip = False
    struct = None
    mainArgs = []

    # statements which need to be inserted into INIT_MAIN
    entry_declarations = []

    parsing_cbuffer = None
    parsing_pushconstant = None
    attribute_index  = 0

    # reversed list of functions, used to determine where a resource is being accessed
    reversed_fns = list(reversed(list(global_fn_table.items())))

    parsing_main = False
    global_scope_count = 0
    for i, line in enumerate(shader.lines):
        if line.strip().startswith('//'): continue

        if '{' in line: global_scope_count += 1
        if '}' in line: global_scope_count -= 1

        l_get = line.find('Get(')
        while l_get > 0 and not parsing_main:
            resource = line[l_get:]
            resource = resource[4:resource.find(')')]
            br = resource.find('[')
            if -1 != br:
                resource = resource[:br]
            for fn, (_, fn_i) in reversed_fns:
                if fn_i[0] < i:
                    if fn not in global_references: global_references[fn] = set()
                    global_references[fn].add(resource)
                    break
            l_get = line.find('Get(', l_get+1)

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

    def declare_argument_buffers(mainArgs):
        ab_decl = []
        # declare argument buffer structs
        for freq, elements in ab_elements.items():

            # skip empty update frequencies
            if not elements: continue

            argBufType = 'AB_' + freq
            ab_decl += ['\n\t// Generated Metal Resource Declaration: ', argBufType, '\n' ]

            # make AB declaration only active if any member is defined
            space = 'constant'
            ifreq = metal_ab_frequencies.index(freq)
            mainArgs += [[space, ' struct ', argBufType, '& ', argBufType, f'[[buffer({ifreq})]]']]

            ab_decl += ['\tstruct ', argBufType, '\n\t{\n']
            sorted_elements = sorted(elements, key=lambda x: x[0], reverse=False)
            vkbinding = 0
            for elem in sorted_elements:
                binding = cleaneval(elem[0].split('=')[-1])
                arr = 1 if not isArray(elem[1][2]) else cleaneval(getArrayLenFlat(elem[1][2]))
                while vkbinding < binding:
                    ab_decl += [f'\t\tconstant void* _dummy_{ifreq}_{vkbinding};\n']
                    vkbinding += 1
                ab_decl += ['\t\t', *elem[1], ';\n']
                vkbinding += arr

            if len(sorted_elements) == 1:
                _, elem0 = sorted_elements[0]
                if 'SamplerState' in elem0[0]:
                    print('WARN: Sampler-only AB not supported!')

            ab_decl += ['\t};\n']
        return ab_decl
    
    last_res_decl = 0
    explicit_res_decl = None
    for fi, line_index, line in iter_lines(shader.lines):
        
        shader_src_len = len(shader_src)

        if 'DECLARE_RESOURCES' in line:
            explicit_res_decl = len(shader_src) + 1
            line = '//' + line

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

        # handle cbuffer and pushconstant elements
        if (parsing_cbuffer or parsing_pushconstant) and line.strip().startswith('DATA('):
            dt, name, sem = getMacro(line)
            element_basename = getArrayBaseName(name)

            if parsing_cbuffer:
                elemen_path = parsing_cbuffer[0] + '.' + element_basename

                # for non-embedded cbuffer, access directly using struct access
                global_reference_paths[element_basename] = parsing_cbuffer[0]
                global_reference_args[element_basename] = 'constant struct ' + parsing_cbuffer[0] + '& ' + parsing_cbuffer[0]
                if is_cbuffer_embedded(binary, parsing_cbuffer):
                    elemen_path = 'AB_' + parsing_cbuffer[1] + '.' + elemen_path
                    global_reference_paths[element_basename] = 'AB_' + parsing_cbuffer[1]
                    global_reference_args[element_basename] = 'constant struct AB_' + parsing_cbuffer[1] + '& ' + 'AB_' + parsing_cbuffer[1]

            if parsing_pushconstant:
                elemen_path = parsing_pushconstant[0] + '.' + element_basename
                global_reference_paths[element_basename] = parsing_pushconstant[0]
                global_reference_args[element_basename] = 'constant struct ' + parsing_pushconstant[0] + '& ' + parsing_pushconstant[0]

            shader_src += ['#define _Get_', element_basename, ' ', elemen_path, '\n']


        if is_groupshared_decl(line):
            dtype, dname = getMacro(line)
            basename = getArrayBaseName(dname)
            shader_src += ['\n\t// Metal GroupShared Declaration: ', basename, '\n']

            entry_declarations += [['threadgroup {} {};'.format(dtype, dname)]]

            array_decl = get_array_decl(dname)

            global_reference_paths[basename] = basename
            global_reference_args[basename] = 'threadgroup ' + dtype + ' (&' + basename + ')' + array_decl

            shader_src += ['#define _Get_', basename, ' ', basename, '\n']

            shader_src += ['#line {}\n'.format(line_index)]
            shader_src += ['\t// End of GroupShared Declaration: ', basename, '\n']
            continue

        if 'PUSH_CONSTANT' in line:
            parsing_pushconstant = tuple(getMacro(line))

        if '};' in line and parsing_pushconstant:

            shader_src += [line]
            push_constant_decl = parsing_pushconstant
            pushconstant_name = push_constant_decl[0]

            location = dxRegisterToMSLBinding('PUSH_CONSTANT', push_constant_decl[1])
            location = getBufferId(1)
            push_constant_location = f'[[buffer({location})]]'

            mainArgs += [ ['constant struct ', pushconstant_name, '& ', pushconstant_name, ' ', push_constant_location] ]

            parsing_pushconstant = None
            struct_references = []
            last_res_decl = len(shader_src)+1
            continue

        if 'CBUFFER' in line:
            fsl_assert(parsing_cbuffer == None, fi, line_index, message='Inconsistent cbuffer declaration')
            parsing_cbuffer = tuple(getMacro(line))

        if '};' in line and parsing_cbuffer:
            shader_src += [line]
            cbuffer_name, cbuffer_freq, dxreg, vkbinding = parsing_cbuffer[:4]
            is_embedded = is_cbuffer_embedded(binary, parsing_cbuffer)

            if cbuffer_freq not in metal_ab_frequencies and is_embedded:
                shader_src += ['#line {}\n'.format(line_index)]
                shader_src += ['\t// Ignored CBuffer Declaration: '+line+'\n']
                continue

            if not is_embedded:
                location = dxRegisterToMSLBinding('CBUFFER', dxreg)
                location = getBufferId(1)

                fr = metal_ab_frequencies.index(cbuffer_freq)
                nameWithFrequency = f'_fslF{fr}_{cbuffer_name}'

                mainArgs += [['constant struct ', cbuffer_name, '& ', nameWithFrequency, f' [[buffer({location})]] // {dxreg}']]
                entry_declarations += [f'constant auto& {cbuffer_name} = {nameWithFrequency};']
            else:
                ab_element = ['constant struct ', cbuffer_name, '& ', cbuffer_name]
                ab_elements[cbuffer_freq] += [( vkbinding, ab_element )]

            parsing_cbuffer = None
            struct_references = []
            last_res_decl = len(shader_src)+1

            continue

        # consume resources
        if 'RES(' in line:
            resource = tuple(getMacro(line))
            resType, resName, freq, dxreg, vkbinding = resource[:5]
            baseName = getArrayBaseName(resName)

            is_embedded = False
            if not Features.NO_AB in binary.features:
                if 'SamplerState' == resType:
                    is_embedded = False
                if isArray(resName):
                    is_embedded = True
                if 'RW' in resType:
                    is_embedded = False
                if 'COMMAND_BUFFER' == resType:
                    is_embedded = True
                if 'RasterizerOrderedTex2D' in resType:
                    is_embedded = True
            if 'USE_AB' == resource[-1]:
                is_embedded = True

            is_embedded = is_embedded or Features.ICB in binary.features or Features.RAYTRACING in binary.features

            if freq not in metal_ab_frequencies:
                shader_src += ['#line {}\n'.format(line_index)]
                shader_src += ['\t// Ignored Resource Declaration: '+line+'\n']
                continue

            if not is_embedded: # regular resource
                shader_src += ['\n\t// Metal Resource Declaration: ', line, '\n']
                is_array = isArray(resName)
                array_len = int(cleaneval(getArrayLenFlat(resName))) if is_array else 1
                prefix = ''
                postfix = ' '
                is_buffer = 'Buffer' in resType


                fr = metal_ab_frequencies.index(resource[2])
                nameWithFrequency = f'_fslF{fr}_{baseName}'

                if is_buffer:
                    prefix = 'device ' if 'RW' in resType else 'const device '
                    postfix = '* '
                if 'Sampler' in resType:
                    location = getSamplerId(array_len)
                    binding = ' [[sampler({})]]'.format(location)
                elif 'RasterizerOrderedTex2D' in resType:
                    location = getTextureId(array_len)
                    _, group_index = getMacro(resType)
                    binding = ' [[raster_order_group({0}), texture({1})]]'.format(group_index, location)
                elif 'Tex' in resType or 'Depth' in getMacroName(resType):
                    location = getTextureId(array_len)
                    binding = ' [[texture({})]]'.format(location)
                elif 'Buffer' in resType:
                    location = getBufferId(array_len)
                    binding = ' [[buffer({})]]'.format(location)
                else:
                    fsl_assert(False, fi, line_index, message=f"Unknown Resource location: {resource}")

                mainArgType = resType

                if is_array and is_buffer: # unroll buffer array
                    main_arg = [prefix , mainArgType, postfix, f'_fslA{fr}{array_len}_' + baseName, binding, ' // main arg ', resource[2]]
                    mainArgs += [main_arg]
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
                    entry_declarations += [f'auto {baseName} = {nameWithFrequency};']

                global_reference_paths[baseName] = baseName
                
                if is_buffer:
                    space = 'device' if 'RW' in resType else 'const device'
                    if is_array:
                        ref_arg = f'{space} {resType} *(&{baseName})[{array_len}] '
                    else:
                        ref_arg = f'{space} {resType} *{resName}'
                else:
                    if is_array:
                        ref_arg = f'thread array<{resType}, {array_len}> (&{baseName}) '
                    else:
                        ref_arg = f'thread {resType}& {resName}'
                global_reference_args[baseName] = ref_arg

                shader_src += ['#define _Get_', baseName, ' ', baseName, '\n']

                shader_src += ['\t// End of Resource Declaration: ', resName, '\n']

            else: # resource is embedded in argbuf

                shader_src += ['\n\t// Metal Embedded Resource Declaration: ', line, '\n']
                argBufType = 'AB_' + freq
                basename = getArrayBaseName(resName)

                if 'Buffer' in resType:
                    space = ' device ' if 'RW' in resType else ' const device '
                    ab_element = [space, resType, '* ', resName]
                elif 'RasterizerOrderedTex2D' in resType:
                    _, group_index = getMacro(resType)
                    ab_element = [resType, ' ', resName, ' ', f'[[raster_order_group({group_index})]]']
                else:
                    ab_element = [resType, ' ', resName]

                ab_elements[freq] += [( vkbinding, ab_element )]

                global_reference_paths[baseName] = argBufType
                if not isArray(resName):
                    global_reference_args[baseName] = 'constant ' + resType + '& ' + resName
                    if 'Buffer' in resType:
                        global_reference_args[baseName] = 'constant ' + resType + '* ' + resName
                else:
                    array = resName[resName.find('['):]
                    global_reference_args[baseName] = 'constant ' + resType + '(&' + baseName+') ' + array
                global_reference_args[baseName] = 'constant struct ' + argBufType + '& ' + argBufType
                shader_src += ['#define _Get_', baseName, ' ', argBufType, '.', baseName, '\n']

                shader_src += ['\t//End of Resource Declaration: ', baseName, '\n']

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
            ab_decl = declare_argument_buffers(mainArgs)
            ab_decl_location = last_res_decl if not explicit_res_decl else explicit_res_decl
            shader_src = shader_src[:ab_decl_location] + ab_decl + shader_src[ab_decl_location:]

            mtl_returntype = 'void'
            if shader.returnType:
                mtl_returntype = getMacroName(shader.returnType)

            shader_src += [msl_target, ' ', mtl_returntype, ' ', binary.filename.replace('.', '_'),  '(\n']
            
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
                prefix = '\t,'

            for arg in mainArgs:
                shader_src += [prefix, *arg, '\n']
                prefix = '\t,'

            shader_src += [')\n']
            shader_src += ['#line {}\n'.format(line_index)]
            continue

        if 'INIT_MAIN' in line:

            for entry_declaration in entry_declarations:
                shader_src += ['\t', *entry_declaration, '\n']

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

    open(dst, 'w').writelines(shader_src)
    return 0, []
