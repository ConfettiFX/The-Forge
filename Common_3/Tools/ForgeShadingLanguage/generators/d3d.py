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

""" HLSL shader generation """

from utils import Stages, DescriptorSets, WaveopsFlags, ShaderBinary, Platforms, iter_lines
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
    srt_resources = { descriptor_set.name: [] for descriptor_set in DescriptorSets }
    srt_free_resources = []

    parsing_struct = None
    skip_semantics = False

    for fi, line_index, line in iter_lines(shader.lines):

        shader_src_len = len(shader_src)

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

            if 'RayQueryEndForEachCandidate' in line:
                    hit_name = getMacro(line)
                    for awaiting_call in awaiting_ray_query_calls:
                        if awaiting_call[0] == hit_name:
                            line = ';'
                            line += awaiting_call[1]
                            awaiting_ray_query_calls.remove(awaiting_call)
                            print(line)
                            break

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
        if line.strip().startswith('RES('):
            resource_decl = getMacro(line)
            if 'ROOT_CONSTANT' in resource_decl[0]:
                srt_loc = len(shader_src) + 1

        if d3d11 and resource_decl:
            dtype, name, freq, reg, _ = resource_decl
            freq = DescriptorSets[freq].value
            shader_src += [f'#define {name} _fslF{freq}_{name}\n']
            if 'CBUFFER' in dtype or 'ROOT_CONSTANT' in dtype:
                line = line[:-2] + f' {{{getMacro(dtype)} {name};}};\n'

        if pssl and resource_decl:
            resType, res_name, res_freq, _, _ = resource_decl
            if 'CBUFFER' in resType or 'ROOT_CONSTANT' in resType:
                line = f'struct {res_name} {{ {getMacro(resType)} m; }};\n'
            if 'rootcbv' in res_name:
                srt_free_resources += [pssl.declare_resource(resource_decl)]
            else:
                srt_resources[res_freq] += [pssl.declare_resource(resource_decl)]
            replacements += [ pssl.declare_reference(shader, resource_decl) ]

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

        shader_src += [line]

    if pssl:
        for awaiting_call in awaiting_ray_query_calls:
            lastCharacter = awaiting_call[1].find(f', {awaiting_call[0]}Callback', )
            shader_src[awaiting_call[2]] = awaiting_call[1][0:lastCharacter] + ");}\n"
        if srt_loc > 0: # skip srt altogether if no declared resourced or not requested
            srt = pssl.gen_srt(srt_resources, srt_free_resources)
            shader_src0, shader_src1 = shader_src[:srt_loc], shader_src[srt_loc:]
            shader_src0 += [srt]
            shader_src1 = ''.join(shader_src1)
            for s, d in replacements:
                rr = re.compile(rf'\b{s}\b\s*(.)', re.DOTALL)
                shader_src1 = rr.sub(lambda x: x.group() if x.group(1) == '{' else d+x.group(1), shader_src1)
            shader_src = shader_src0 + [shader_src1]

    open(dst, 'w').writelines(shader_src)

    return 0, dependencies
