""" GLSL shader generation """

from utils import Stages, getHeader, getShader, getMacro, genFnCall, fsl_assert, get_whitespace
from utils import isArray, getArrayLen, getArrayBaseName, getMacroName, DescriptorSets, is_groupshared_decl
import os, sys, importlib, re
from shutil import copyfile

def pssl(fsl, dst, rootSignature=None):
    return d3d(fsl, dst, pssl=True, d3d12=False, rootSignature=rootSignature)

def prospero(fsl, dst):
    return d3d(fsl, dst, pssl=True, prospero=True)

def xbox(fsl, dst, rootSignature=None):
    return d3d(fsl, dst, xbox=True, d3d12=True, rootSignature=rootSignature)

def d3d12(fsl, dst):
    return d3d(fsl, dst, d3d12=True)

def scarlett(fsl, dst, rootSignature=None):
    return xbox(fsl, dst, rootSignature)

def d3d(fsl, dst, pssl=False, prospero=False, xbox=False, rootSignature=None, d3d12=False):

    shader = getShader(fsl, dst)

    shader_src = getHeader(fsl)
    if not (d3d12 or pssl or xbox):
        shader_src += ['#define DIRECT3D11\n']

    if prospero:
        import prospero
        pssl = prospero
        shader_src += ['#define PROSPERO\n']
        shader_src += prospero.preamble()


    elif pssl:
        import orbis
        pssl = orbis
        shader_src += ['#define ORBIS\n']
        shader_src += orbis.preamble()

    if xbox:
        import xbox
        shader_src += ['#define XBOX\n']
        shader_src += xbox.preamble()

    if d3d12:
        shader_src += ['#define DIRECT3D12\n']

        
    shader_src += ['#define STAGE_', shader.stage.name, '\n']
    if shader.enable_waveops:
        shader_src += ['#define ENABLE_WAVEOPS()\n']

    # directly embed d3d header in shader
    header_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'includes', 'd3d.h')
    header_lines = open(header_path).readlines()
    shader_src += header_lines + ['\n']

    nonuniformresourceindex = None

    # tesselation
    pcf_returnType = None

    # for SV_PrimitiveID usage in pixel shaders, generate a pass-through gs
    passthrough_gs = False
    if pssl and shader.stage == Stages.FRAG:
        for dtype, dvar in shader.flat_args:
            if getMacroName(dtype).upper() == 'SV_PRIMITIVEID':
                passthrough_gs = True
                if prospero:
                    prospero.gen_passthrough_gs(shader, dst.replace('frag', 'geom'))
                else:
                    orbis.gen_passthrough_gs(shader, dst.replace('frag', 'geom'))

    last_res_decl = 0
    explicit_res_decl = None
    srt_resources = { descriptor_set.name: [] for descriptor_set in DescriptorSets }
    srt_free_resources = []
    srt_references = []

    defineLoc = len(shader_src)

    parsing_struct = None
    skip_semantics = False
    struct_elements = []
    srt_redirections = set()
    for line in shader.lines:

        def get_uid(name):
            return name + '_' + str(len(shader_src))

        # dont process commented lines
        if line.strip().startswith('//'):
            shader_src += [line]
            continue

        if is_groupshared_decl(line):
            dtype, dname = getMacro(line)
            basename = getArrayBaseName(dname)
            shader_src += ['#define srt_'+basename+' '+basename+'\n']
            if not pssl:
                line = 'groupshared '+dtype+' '+dname+';\n'
            else:
                line = 'thread_group_memory '+dtype+' '+dname+';\n'

        if 'DECLARE_RESOURCES' in line:
            explicit_res_decl = len(shader_src) + 1
            line = '//' + line

        if line.strip().startswith('STRUCT(') or line.strip().startswith('CBUFFER(') or line.strip().startswith('PUSH_CONSTANT('):
            parsing_struct = getMacro(line)

            struct_name = parsing_struct[0]

            struct_elements = []

            if pssl and 'PUSH_CONSTANT' in line:
                skip_semantics = True
                macro = get_uid(struct_name)
                shader_src += ['#define ', macro, '\n']
                srt_free_resources += [(macro, pssl.declare_rootconstant(struct_name))]

            if pssl and 'CBUFFER' in line:
                skip_semantics = True
                res_freq = parsing_struct[1]
                macro = get_uid(struct_name)
                shader_src += ['#define ', macro, '\n']
                if 'rootcbv' in struct_name:
                    srt_free_resources += [(macro, pssl.declare_cbuffer(struct_name))]
                else:
                    srt_resources[res_freq] += [(macro, pssl.declare_cbuffer(struct_name))]

        if parsing_struct and line.strip().startswith('DATA('):
            data_decl = getMacro(line)
            if skip_semantics or data_decl[-1] == 'None':
                line = get_whitespace(line) + data_decl[0] + ' ' + data_decl[1] + ';\n'

            if pssl and type(parsing_struct) is not str:
                basename = getArrayBaseName(data_decl[1])
                macro = 'REF_' + get_uid(basename)
                shader_src += ['#define ', macro, '\n']
                init, ref = pssl.declare_element_reference(shader, parsing_struct, data_decl)
                shader_src += [*init, '\n']
                srt_redirections.add(basename)
                struct_elements += [(macro, ref)]
                srt_references += [(macro, (init, ref))]
                shader_src += [line]
                continue

        if parsing_struct and '};' in line:

            # if this shader is the receiving end of a passthrough_gs, insert the necessary inputs
            if passthrough_gs and shader.struct_args[0][0] == parsing_struct:
                shader_src += ['\tDATA(FLAT(uint), PrimitiveID, TEXCOORD8);\n']

            shader_src += [line]

            skip_semantics = False
            if type(parsing_struct) is not str:
                last_res_decl = len(shader_src)+1
            parsing_struct = None
            continue

        resource_decl = None
        if line.strip().startswith('RES('):
            resource_decl = getMacro(line)
            last_res_decl = len(shader_src)+1

        if pssl and resource_decl:
            # shader_src += ['// ', line.strip(), '\n']
            _, res_name, res_freq, _, _ = resource_decl
            basename = getArrayBaseName(res_name)
            macro = get_uid(basename)

            # shader_src += ['#define ', macro, ' //', line.strip(), '\n']
            shader_src += ['#define ', macro, '\n']
            srt_resources[res_freq] += [(macro, pssl.declare_resource(resource_decl))]

            # macro = 'REF_' + macro
            # shader_src += ['#define ', macro, '\n']
            init, ref = pssl.declare_reference(shader, resource_decl)
            shader_src += [*init, '\n']
            srt_references += [(macro, (init, ref))]
            srt_redirections.add(basename)

            last_res_decl = len(shader_src)+1
            # continue

        if 'TESS_VS_SHADER(' in line and prospero:
            vs_filename = getMacro(line).strip('"')
            vs_fsl_path = os.path.join(os.path.dirname(fsl), vs_filename)
            ls_vs_filename = 'ls_'+vs_filename.replace('.fsl', '')
            vs_pssl = os.path.join(os.path.dirname(dst), ls_vs_filename)
            d3d(vs_fsl_path, vs_pssl, pssl=True, prospero=True)
            shader_src += [
                '#undef VS_MAIN\n',
                '#define VS_MAIN vs_main\n',
                '#include "', ls_vs_filename, '"\n'
                ]
            continue

        if '_MAIN(' in line and shader.stage == Stages.TESC and prospero:
            shader_src += pssl.insert_tesc('vs_main')

        if '_MAIN(' in line and shader.returnType:
            if shader.returnType not in shader.structs:
                if shader.stage == Stages.FRAG:
                    if not 'SV_DEPTH' in shader.returnType.upper():
                        line = line[:-1] + ': SV_TARGET\n'
                    else:
                        line = line[:-1] + ': SV_DEPTH\n'
                if shader.stage == Stages.VERT:
                    line = line[:-1] + ': SV_POSITION\n'

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
                for dtype, darg in shader.flat_args:
                    if 'SV_INSTANCEID' in dtype.upper():
                        shader_src += pssl.set_indirect_draw()

        if '_MAIN(' in line and (pssl or xbox) and rootSignature:

            l0 = rootSignature.find('SrtSignature')
            l1 = rootSignature.find('{', l0)
            srt_name = rootSignature[l0: l1].split()[-1]

            res_sig = 'RootSignature' if xbox else 'SrtSignature'
            shader_src += ['[', res_sig, '(', srt_name, ')]\n', line]
            continue

        # if 'INIT_MAIN' in line:
        #     if pssl:
        #         shader_src += ['\tinit_global_references();\n']

        if 'INIT_MAIN' in line and shader.returnType:
            # mName = getMacroName(shader.returnType)
            # mArg = getMacro(shader.returnType)
            # line = line.replace('INIT_MAIN', '{} {}'.format(mName, mArg))
            line = get_whitespace(line)+'//'+line.strip()+'\n'


            # if this shader is the receiving end of a passthrough_gs, copy the PrimitiveID from GS output
            if passthrough_gs:
                for dtype, dvar in shader.flat_args:
                    if 'SV_PRIMITIVEID' in dtype.upper():
                        shader_src += ['uint ', dvar, ' = ', shader.struct_args[0][1], '.PrimitiveID;\n']

        if 'BeginNonUniformResourceIndex(' in line:
            index, max_index = getMacro(line), None
            assert index != [], 'No index provided for {}'.format(line)
            if type(index) == list:
                max_index = index[1]
                index = index[0]
            nonuniformresourceindex = index
            if pssl:
                shader_src += pssl.begin_nonuniformresourceindex(nonuniformresourceindex, max_index)
                continue
            else:
                line = '#define {0} NonUniformResourceIndex({0})\n'.format(nonuniformresourceindex)
        if 'EndNonUniformResourceIndex()' in line:
            assert nonuniformresourceindex, 'EndNonUniformResourceIndex: BeginNonUniformResourceIndex not called/found'
            if pssl:
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

        # tesselation
        if shader.pcf and shader.pcf in line and not pcf_returnType:
            loc = line.find(shader.pcf)
            pcf_returnType = line[:loc].strip()
            # line = getMacroName(pcf_returnType) + ' ' + line[loc:]
            for dtype, dvar in shader.pcf_arguments:
                if not 'INPUT_PATCH' in dtype and not 'OUTPUT_PATCH' in dtype:
                    line = line.replace(dtype, getMacro(dtype))
                    line = line.replace(dvar, dvar+': '+getMacroName(dtype))

        if pcf_returnType and re.match('\s*PCF_INIT', line):
            # line = line.replace('PCF_INIT', getMacroName(pcf_returnType) + ' ' + getMacro(pcf_returnType))
            line = line.replace('PCF_INIT', '')

        if pcf_returnType and 'PCF_RETURN' in line:
            line = line.replace('PCF_RETURN', 'return ')
            # line = line.replace('PCF_RETURN', '{ return ' + getMacro(pcf_returnType) + ';}')

        if 'INDIRECT_DRAW(' in line:
            if pssl:
                shader_src += pssl.set_indirect_draw()
            line = '//' + line

        if 'SET_OUTPUT_FORMAT(' in line:
            if pssl:
                shader_src += pssl.set_output_format(getMacro(line))
            line = '//' + line

        if 'PS_ZORDER_EARLYZ(' in line:
            if xbox:
                shader_src += xbox.set_ps_zorder_earlyz()
            line = '//' + line

        shader_src += [line]

    if pssl:
        if explicit_res_decl:
            last_res_decl = explicit_res_decl
        if last_res_decl > 0: # skip srt altogether if no declared resourced or not requested
            srt = pssl.gen_srt(srt_resources, srt_free_resources, srt_references)
            open(dst + '.srt.h', 'w').write(srt)
            shader_src.insert(last_res_decl, '\n#include \"' + os.path.basename(dst) + '.srt.h\"\n')

    # insert root signature at the end (not sure whether that will work for xbox)
    if rootSignature and pssl:
        shader_src += [_line+'\n' for _line in rootSignature.splitlines()]# + shader.lines
    if rootSignature and xbox:
        shader_src += rootSignature + ['\n']# + shader.lines

    open(dst, 'w').writelines(shader_src)

    return 0