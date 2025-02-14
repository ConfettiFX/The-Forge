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

""" HLSL shader generation """

from utils import Stages, WaveopsFlags, ShaderBinary, Platforms, iter_lines
from utils import isArray, getArrayLen, getArrayBaseName, getMacroName, is_groupshared_decl
from utils import getMacroFirstArg, getHeader, getShader, getMacro, platform_langs, get_whitespace
from utils import get_fn_table, Features
import os, re

def direct3d11(*args):
    return hlsl(Platforms.DIRECT3D11, *args)

def direct3d12(*args):
    return hlsl(Platforms.DIRECT3D12, *args)

def xbox(*args):
    return hlsl(Platforms.XBOX, *args)

def scarlett(*args):
    return hlsl(Platforms.SCARLETT, *args)

def orbis(*args):
    return hlsl(Platforms.ORBIS, *args)

def prospero(*args):
    return hlsl(Platforms.PROSPERO, *args)

def resources_declaration(resources_dict):
    text = ''
    for key, items in resources_dict.items():
        for resources in items:
            for line in resources:
                valid_case = False
                line_tokens = re.findall(r'\w+', line)
                tokens_len = len(line_tokens)
                #print( f'resource = ' + line + ' token count = ' + str(tokens_len) )
                if tokens_len == 2 and 'RWByteBuffer' in line_tokens[0]:
                    text += 'static ' + line_tokens[0] + ' '+ line_tokens[1] + ' = gSRT.p' + key + '->' + line_tokens[1] + ';\n'
                    valid_case = True
                
                if tokens_len == 3 and ( 'RWBuffer' in line_tokens[0] or 'RWCoherentBuffer' in line_tokens[0] ):
                    text += 'static ' + line_tokens[0] + '(' + line_tokens[1] + ') ' + line_tokens[2] +' = gSRT.p' + key + '->' + line_tokens[2] + ';\n'
                    valid_case = True
                if tokens_len == 3 and ( 'Tex2D' in line_tokens[0] or 'Tex3D' in line_tokens[0] ):
                    text += 'static ' + line + ' = gSRT.p' + key + '->' + line_tokens[2] + ';\n'
                    valid_case = True
                if tokens_len == 3 and 'Depth2D' in line_tokens[0]:
                    text += 'static ' + line + ' = gSRT.p' + key + '->' + line_tokens[2] + ';\n'
                    valid_case = True                    
                if tokens_len == 3 and ( line_tokens[0] == 'TexCube' or line_tokens[0] == 'TexCubeArray' ):
                    text += 'static ' + line + ' = gSRT.p' + key + '->' + line_tokens[2] + ';\n'
                    valid_case = True
                if tokens_len == 4 and ( 'Tex1D' in line_tokens[0] or 'Tex2D' in line_tokens[0] or 'Tex3D' in line_tokens[0] or 'Depth2D' in line_tokens[0] ):
                    # detect array
                    if '[' in line and ']' in line:
                        text += 'static ' + line + ' = gSRT.p' + key + '->' + line_tokens[2] + ';\n'
                    else:
                        text += 'static ' + line + ' = gSRT.p' + key + '->' + line_tokens[3] + ';\n'
                    valid_case = True
                if tokens_len == 2 and ( line_tokens[0] == 'SamplerState' or line_tokens[0] == 'SamplerComparisonState' ):
                    text += 'static ' + line + ' = gSRT.p' + key + '->' + line_tokens[1] + ';\n'
                    valid_case = True
                if tokens_len == 2 and line_tokens[0] == 'ByteBuffer':
                    text += 'static ' + line_tokens[0] + '  '+ line_tokens[1] + ' = gSRT.p' + key + '->' + line_tokens[1] + ';\n'
                    valid_case = True                    
                if tokens_len == 4 and line_tokens[1] == 'CBUFFER':
                    text += 'static ' + line_tokens[2] + '& ' + line_tokens[3] + ' = *gSRT.p' + key + '->' + line_tokens[3] + ';\n'
                    valid_case = True
                if tokens_len == 4 and line_tokens[1] == 'ROOT_CONSTANT':
                    text += 'static ' + line_tokens[1] + '(' + line_tokens[2] + ')& ' + line_tokens[3] + ' = gSRT.' + line_tokens[3] + ';\n'
                    valid_case = True
                if tokens_len == 4 and line_tokens[1] == 'ROOT_CBV':
                    text += 'static ' + line_tokens[2] + '& ' + line_tokens[3] + ' = *gSRT.' + line_tokens[3] + ';\n'
                    valid_case = True
                if tokens_len == 3 and line_tokens[0] == 'Buffer':
                    text += 'static ' + line_tokens[0] + '(' + line_tokens[1] + ') ' + line_tokens[2] + ' = gSRT.p' + key + '->' + line_tokens[2] + ';\n'
                    valid_case = True                    

                if tokens_len == 3 and line_tokens[0] == 'RWByteBuffer':
                    text += 'static ' + line_tokens[0] + ' ' + line_tokens[1] + '  [] = gSRT.p' + key + '->' + line_tokens[1] + ';\n'
                    valid_case = True    
                    
                if tokens_len == 4 and line_tokens[2] == 'TopLevelBvhDescriptor':
                    text += 'static ' + line_tokens[0] + '::' + line_tokens[1] + '::' + line_tokens[2] + ' ' + line_tokens[3] + ' = gSRT.p' + key + '->' + line_tokens[3] + ';\n'
                    valid_case = True


                if valid_case == False:
                    print('Error : resource case not found, check resources_declaration in d3d.py : ' + line + ' token count = ' + str(tokens_len))

    return text
    
def replace_d3d_resource_register(register_subtring, new_register):
    # find ( and find comma
    open_parenthesis_index = register_subtring.find('(')
    comma_index = register_subtring.find(',')
    if ( open_parenthesis_index is not -1) and (comma_index is not -1) :
        register_subtring = register_subtring[:open_parenthesis_index+1] + new_register + register_subtring[comma_index:]
    return register_subtring
    
def replace_d3d_register_declaration(line, new_register_string):
    register_index = line.find('register')
    if register_index is not -1:
        line = line[:register_index] + ' ' + new_register_string
    return line
    

def hlsl(platform, debug, binary: ShaderBinary, dst):

    fsl = binary.preprocessed_srcs[platform]

    shader = getShader(platform, binary, fsl, dst)
    binary.waveops_flags = shader.waveops_flags
    # check for function overloading.
    get_fn_table(shader.lines)

    shader_src = getHeader(fsl)

    pssl = None

    is_xbox = False
    d3d11 = Platforms.DIRECT3D11 == platform

    dependencies = []
    
    if Platforms.PROSPERO == platform:
        import prospero as prospero_utils
        pssl = prospero_utils
        shader_src += prospero_utils.preamble(binary)

    elif Platforms.ORBIS == platform:
        import orbis as orbis_utils
        pssl = orbis_utils
        shader_src += orbis_utils.preamble()

    if Platforms.XBOX == platform or Platforms.SCARLETT == platform:
        is_xbox = True
        import xbox as xbox_utils
        shader_src += xbox_utils.preamble(platform, binary, shader)

    shader_src += [f'#define {platform.name}\n']
    shader_src += [f'#define {platform_langs[platform]}\n']
        
    shader_src += ['#define STAGE_' + shader.stage.name + '\n']
    if shader.waveops_flags != WaveopsFlags.WAVE_OPS_NONE:
        shader_src += ['#define ENABLE_WAVEOPS(flags)\n']

    # directly embed d3d header in shader
    header_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'includes', 'd3d.h')
    header_lines = open(header_path).readlines()
    shader_src += header_lines + ['\n']
    
    #todo : do we need this? if not remove.
    # directly embed srt header in shader
    #if not pssl:
        #header_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'includes', 'd3d12_srt.h')
        #header_lines = open(header_path).readlines()
        #shader_src += header_lines + ['\n']

    nonuniformresourceindex = None

    # ray query
    awaiting_ray_query_calls = []

    replacements = []

    # for SV_PrimitiveID usage in pixel shaders, generate a pass-through gs
    passthrough_gs = False
    if pssl and shader.stage == Stages.FRAG:
        for dtype, dvar in shader.flat_args:
            if getMacroName(dtype).upper() == 'SV_PRIMITIVEID':
                passthrough_gs = True
                fn = dst.replace('frag', 'geom')
                if Platforms.PROSPERO == platform:
                    pssl.gen_passthrough_gs(shader, binary, fn)
                else:
                    pssl.gen_passthrough_gs(shader, fn)
                dependencies += [(Stages.GEOM, fn, os.path.basename(fn))]

    srt_loc = len(shader_src)+1
    srt_free_resources = []

    parsing_struct = None
    skip_semantics = False
    declarations_code = ''
    parsing_resources = False
    all_resources = {}
    curr_res_freq = ''
    resources_to_check = { 'Tex', 'SamplerState', 'SamplerComparisonState', 'struct CBUFFER', 'Buffer', 'Tex2DArray', 'RWTex2DArray', 'WTex2DArray', 'RWTex2D', 'RWTex3D', 'WTex2D', 'WTex3D', 'RTex2D', 'RTex3D', 'RWBuffer', 'RWCoherentBuffer', 'struct ROOT_CONSTANT', 'struct ROOT_CBV', 'ByteBuffer', 'Depth2D', 'RWByteBuffer', 'TopLevelBvhDescriptor' }
    resources_to_check_end = {  'TopLevelBvhDescriptor' }
    sets_to_check = []
    register_index = 0
    
    for fi, line_index, line in iter_lines(shader.lines):

        skip_line = False
        stripped_line = line.strip()
        shader_src_len = len(shader_src)
        line_tokens = re.findall(r'\w+', line)
        tokens_len = len(line_tokens)

        if pssl:
            if 'RayQueryCallBegin(' in line:
                l_index = line.find('(')
                r_index = line.find(')')
                if l_index > -1 and r_index > -1:
                    hit_name = line[l_index + 1 : r_index]
                    line = line.replace(f'RayQueryCallBegin({hit_name})', '')
                    line = line.replace('RayQueryCallEnd', '')
                    awaiting_ray_query_calls += [[hit_name, line, len(shader_src)]]
                    line = '{}\n'
                    
            if parsing_resources:
                if stripped_line.startswith('struct set'):
                    if stripped_line not in sets_to_check:
                        sets_to_check.append(stripped_line)
                
                if any( stripped_line.startswith(word) for word in sets_to_check):
                    curr_res_freq = line_tokens[1][3:]
                if any( stripped_line.startswith(word) for word in resources_to_check) or any( word in stripped_line for word in resources_to_check_end):
                    cleaned_line = stripped_line.replace(";", "").replace("\n", "")
                    if curr_res_freq not in all_resources:
                        all_resources[curr_res_freq] = []
                    all_resources[curr_res_freq].append([f"{cleaned_line}"])
                    if tokens_len == 3:
                        declarations_code += f"//static {cleaned_line} = gSRT.pNone->{line_tokens[2]};\n"
            
            if 'struct globalSRT' in line:
                parsing_resources = True
            if 'gSRT: S_SRT_DATA' in line:
                line += '//Generated resources declarations :\n' + resources_declaration(all_resources) 
                parsing_resources = False

            if 'RayQueryEndForEachCandidate' in line:
                    hit_name = getMacro(line)
                    for awaiting_call in awaiting_ray_query_calls:
                        if awaiting_call[0] == hit_name:
                            line = ';'
                            line += awaiting_call[1]
                            awaiting_ray_query_calls.remove(awaiting_call)
                            print(line)
                            break
        else:
            # those macros are skipped and will not be in generated shader
            # we use them to detect start and end of SRT and SRT sets
            if stripped_line.startswith('BEGIN_SRT(') or stripped_line.startswith('BEGIN_SRT_NO_AB('):
                parsing_resources = True
                skip_line = True
            elif stripped_line.startswith('END_SRT('):
                parsing_resources = False
                skip_line = True
            elif parsing_resources:
                if stripped_line.startswith('BEGIN_SRT_SET('):
                    register_index = 0
                    skip_line = True
                    if stripped_line not in sets_to_check:
                        sets_to_check.append(stripped_line)
                elif stripped_line.startswith('END_SRT_SET('):
                    skip_line = True
                else:
                    # skip empty lines and comments
                    if len(stripped_line) > 0 and stripped_line.startswith('#') is False:
                        #check if this is an array and get the count, otherwise its 1
                        array_count = 1
                        match = re.search(r'\[\s*(\d+)\s*U?\s*\]', stripped_line)
                        if match:
                            array_count = int(match.group(1))
                        # get the substring staring with 'register' and strip it from spaces
                        register_start_index = stripped_line.find("register")
                        if register_start_index != -1:
                            register_subtring = stripped_line[register_start_index:]
                            register_subtring = register_subtring.replace(' ','')
                            open_parenthesis_index = register_subtring.find('(')
                            # replace it with the accumulated register index, based on type
                            # this way the register bindings are automatically calculated 
                            if open_parenthesis_index != -1:
                                fix_line = False
                                fixed_register_substring = ''
                                resource_type_letter = register_subtring[open_parenthesis_index+1:open_parenthesis_index+2]
                                if resource_type_letter is 's':
                                    fixed_register_substring = replace_d3d_resource_register(register_subtring, 's'+str(register_index))
                                    register_index += array_count
                                    fix_line = True
                                elif resource_type_letter is 'b':
                                    fixed_register_substring = replace_d3d_resource_register(register_subtring, 'b'+str(register_index))
                                    register_index += array_count
                                    fix_line = True
                                elif resource_type_letter is 't':
                                    fixed_register_substring = replace_d3d_resource_register(register_subtring, 't'+str(register_index))
                                    register_index += array_count
                                    fix_line = True
                                elif resource_type_letter is 'u':
                                    fixed_register_substring = replace_d3d_resource_register(register_subtring, 'u'+str(register_index))
                                    register_index += array_count
                                    fix_line = True
                                    
                                if fix_line is True:
                                    fixed_line = replace_d3d_register_declaration(line, fixed_register_substring)
                                    line = fixed_line + '\n'
        if is_groupshared_decl(line):
            dtype, dname = getMacro(line)
            basename = getArrayBaseName(dname)
            shader_src += ['#define srt_'+basename+' '+basename+'\n']
            if not pssl:
                line = 'groupshared '+dtype+' '+dname+';\n'
            else:
                line = 'thread_group_memory '+dtype+' '+dname+';\n'

        if line.strip().startswith('STRUCT('):
            parsing_struct = getMacro(line)

        if parsing_struct and line.strip().startswith('DATA('):
            data_decl = getMacro(line)
            if skip_semantics or data_decl[-1] == 'None':
                line = get_whitespace(line) + data_decl[0] + ' ' + data_decl[1] + ';\n'

            if not pssl and Features.INVARIANT in binary.features and data_decl[2].upper() == 'SV_POSITION':
                    line = get_whitespace(line) + 'precise ' + line.strip() + '\n'

        if parsing_struct and '};' in line:

            # if this shader is the receiving end of a passthrough_gs, insert the necessary inputs
            if passthrough_gs and shader.struct_args[0][0] == parsing_struct:
                shader_src += ['\tDATA(FLAT(uint), PrimitiveID, TEXCOORD8);\n']

            shader_src += ['#line {}\n'.format(line_index), line]
            if list == type(parsing_struct) and 2 == len(parsing_struct):
                srt_loc = len(shader_src)

            skip_semantics = False
            parsing_struct = None
            continue

        resource_decl = None

        if '_MAIN(' in line and shader.returnType:
            if shader.returnType not in shader.structs:
                if shader.stage == Stages.FRAG:
                    if not 'SV_DEPTH' in shader.returnType.upper():
                        line = line.rstrip() + ': SV_TARGET\n'
                    else:
                        line = line.rstrip() + ': SV_DEPTH\n'
                if shader.stage == Stages.VERT:
                    line = line.rstrip() + ': SV_POSITION\n'
                    if not pssl and Features.INVARIANT in binary.features:
                        line = 'precise ' + line

        # manually transform Type(var) to Type var (necessary for DX11/fxc)
        if '_MAIN(' in line:

            for dtype, var in shader.struct_args:
                line = line.replace(dtype+'('+var+')', dtype + ' ' + var)

            for dtype, dvar in shader.flat_args:
                sem = getMacroName(dtype).upper()
                innertype = getMacro(dtype)
                ldtype = line.find(dtype)
                line = line[:ldtype]+innertype+line[ldtype+len(dtype):]
                l0 = line.find(' '+dvar, ldtype) + len(dvar)+1
                line = line[:l0]+' : '+sem+line[l0:]

            # if this shader is the receiving end of a passthrough_gs, get rid of the PrimitiveID input
            if passthrough_gs:
                for dtype, dvar in shader.flat_args:
                    if 'SV_PRIMITIVEID' in dtype.upper():
                        upper_line = line.upper()
                        l0 = upper_line.find('SV_PRIMITIVEID')
                        l1 = upper_line.rfind(',', 0, l0)
                        line = line.replace(line[l1: l0+len('SV_PRIMITIVEID')], '')

            if pssl:
                if Features.ICB in binary.features or Features.VDP in binary.features:
                    shader_src += pssl.set_indirect_draw()
                    shader_src += pssl.set_apply_index_instance_offset()

        if 'INIT_MAIN' in line:
            ws = get_whitespace(line)
            if shader.returnType:
                line = ws+'//'+line.strip()+'\n'

            # if this shader is the receiving end of a passthrough_gs, copy the PrimitiveID from GS output
            if passthrough_gs:
                for dtype, dvar in shader.flat_args:
                    if 'SV_PRIMITIVEID' in dtype.upper():
                        shader_src += [ws + 'uint ' + dvar + ' = ' + shader.struct_args[0][1] + '.PrimitiveID;\n']

        if 'BeginNonUniformResourceIndex(' in line:
            index, max_index = getMacro(line), None
            assert index != [], 'No index provided for {}'.format(line)
            if type(index) == list:
                max_index = index[1]
                index = index[0]
            nonuniformresourceindex = index
            if Platforms.ORBIS == platform:
                shader_src += pssl.begin_nonuniformresourceindex(nonuniformresourceindex, max_index)
                continue
            else:
                line = '#define {0} NonUniformResourceIndex({0})\n'.format(nonuniformresourceindex)
        if 'EndNonUniformResourceIndex()' in line:
            assert nonuniformresourceindex, 'EndNonUniformResourceIndex: BeginNonUniformResourceIndex not called/found'
            if Platforms.ORBIS == platform:
                shader_src += pssl.end_nonuniformresourceindex(nonuniformresourceindex)
                continue
            else:
                line = '#undef {}\n'.format(nonuniformresourceindex)
            nonuniformresourceindex = None

        elif re.match('\s*RETURN', line):
            if shader.returnType:
                line = line.replace('RETURN', 'return ')
            else:
                line = line.replace('RETURN()', 'return')

        if pssl and Platforms.PROSPERO == platform:
            if 'BeginPSInterlock();' in line:
                line = line.replace('BeginPSInterlock();', pssl.apply_begin_ps_interlock())

            if 'EndPSInterlock();' in line:
                line = line.replace('EndPSInterlock();', pssl.apply_end_ps_interlock())

        if 'SET_OUTPUT_FORMAT(' in line:
            if pssl:
                shader_src += pssl.set_output_format(getMacro(line))
            line = '//' + line

        if 'PS_ZORDER_EARLYZ(' in line:
            if is_xbox:
                shader_src += xbox_utils.set_ps_zorder_earlyz()
            line = '//' + line

        if shader_src_len != len(shader_src):
            shader_src += ['#line {}\n'.format(line_index)]

        if skip_line is False:
            shader_src += [line]

    if pssl:
        for awaiting_call in awaiting_ray_query_calls:
            lastCharacter = awaiting_call[1].find(f', {awaiting_call[0]}Callback', )
            shader_src[awaiting_call[2]] = awaiting_call[1][0:lastCharacter] + ");}\n"


    open(dst, 'w').writelines(shader_src)

    return 0, dependencies
