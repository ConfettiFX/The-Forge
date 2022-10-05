""" GLSL shader generation """

from utils import Stages, getHeader, getMacro, Platforms, getShader, getMacroName, get_whitespace
from utils import isArray, getArrayLen, StageFlags, getArrayBaseName, fsl_assert, ShaderBinary
from utils import is_input_struct, get_input_struct_var, getArrayLenFlat, is_groupshared_decl
import os, sys, re
from shutil import copyfile

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
        # print(n_100, n_10, n_1)
        # sys.exit(1)
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
    elem_type = getMacro(name)
    _map = {
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
    id_beg = line.find(buffer_name+'[')
    while id_beg > -1:
        # either the line starts with the identifier, or it is preceded
        # by a symbol that cannot be contained in an indentifier
        if id_beg == 0 or line[id_beg-1] in '(){}[]|&^, +-/*%:;,<>~!?=\t\n':
            # walk the string and count angle bracket occurences to handle
            # nested expressions: buffer_array[buffer_array[0][0]][0]
            num_br, id_end = 1, id_beg+len(buffer_name)+1 # offset by +4 due to 'Get'
            while id_end < len(line):
                num_br += 1 if line[id_end] == '[' else  -1 if line[id_end] == ']' else 0
                id_end += 1
                if num_br == 0: 
                    line = line[:id_end] + '._data' + line[id_end:]
                    break
        id_beg = line.find(buffer_name+'[', id_beg + len(buffer_name) + 1)
    return line

def is_buffer(fsl_declaration):
    # TODO: endswith better here?
    return 'Buffer' in fsl_declaration[0]

def declare_buffer(fsl_declaration):
    
    buffer_type, name, freq, _, binding = fsl_declaration
    data_type  = getMacro(buffer_type)
    if 'ByteBuffer' in buffer_type:
        data_type = 'uint'
        
    access = 'readonly'
    if buffer_type[:2] == 'W': access = 'writeonly'
    if buffer_type[:2] == 'RW': access = ''

    if 'Coherent' in buffer_type:
        access += ' coherent'

    if isArray(name):
        # for arrays of buffers, expressions get transformed by the script
        array_name = getArrayBaseName(name)
        return ['layout (std430, {}, {}) {} buffer {}Block\n'.format(freq, binding, access, array_name),
                '{{\n\t{} _data[];\n}} {};\n'.format(data_type, name)]
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

def quest(*args):
    return glsl(Platforms.QUEST, *args)

def vulkan(*args):
    return glsl(Platforms.VULKAN, *args)

def switch(*args):
    return glsl(Platforms.SWITCH, *args)

def android(*args):
    return glsl(Platforms.ANDROID, *args)

def glsl(platform: Platforms, debug, binary: ShaderBinary, dst):

    binary.derivatives[platform] = [['VK_EXT_DESCRIPTOR_INDEXING_ENABLED=0', 'VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED=0']]

    shader = getShader(platform, binary.fsl_filepath, binary.preprocessed_srcs[platform], dst)

    shader_src = getHeader(binary.preprocessed_srcs[platform])
    shader_src += [ '#version 450 core\n', '#extension GL_GOOGLE_include_directive : require\nprecision highp float;\nprecision highp int;\n\n']
    shader_src += ['#define STAGE_', shader.stage.name, '\n']
    if Platforms.QUEST == platform:
        shader_src += ['#define TARGET_QUEST\n']
        if StageFlags.VR_MULTIVIEW in binary.flags:
            shader_src += ['#define VR_MULTIVIEW_ENABLED 1\n']

    if shader.enable_waveops:
        shader_src += ['#define ENABLE_WAVEOPS()\n']

    in_location = 0
    
    # shader_src += ['#include "includes/vulkan.h"\n\n']
    # incPath = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'includes', 'vulkan.h')
    # dstInc = os.path.join(os.path.dirname(dst), "includes/vulkan.h")
    # if True or not os.path.exists(dstInc):
    #     os.makedirs(os.path.dirname(dstInc), exist_ok=True)
    #     copyfile(incPath, dstInc)

    # directly embed vk header in shader
    header_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'includes', 'vulkan.h')
    header_lines = open(header_path).readlines()
    shader_src += header_lines + ['\n']

    # pcf output defines (inputs are identical to main)
    pcf_return_assignments = []
    # if shader.stage == Stages.TESC and shader.pcf:
    #     t = getMacroName(shader.pcf_returnType)
    #     if t in shader.structs:
    #         for t, n, s in shader.structs[t]:
    #             pcf_return_assignments += ['ASSIGN_PCF_OUT_' + getArrayBaseName(n)]

    # retrieve output patch size
    patch_size = 0
    for line in shader.lines:
        if 'OUTPUT_CONTROL_POINTS' in line:
            patch_size = int(getMacro(line))

    out_location = 0

    arrBuffs = ['Get('+getArrayBaseName(res[1])+')' for res in shader.resources if 'Buffer' in res[0] and (isArray(res[1]))]
    returnType = None if not shader.returnType else (getMacroName(shader.returnType), getMacro(shader.returnType))

    push_constant = None
    skip_line = False
    parsing_struct = None
    
    parsing_cbuffer = None
    parsing_pushconstant = None

    nonuniformresourceindex = None
    parsed_entry = False
    
    # tesselation
    pcf_returnType = None
    pcf_arguments = []

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

    line_index = 0

    for line in shader.lines:

        line_index += 1
        shader_src_len = len(shader_src)
        if line.startswith('#line'):
            line_index = int(line.split()[1]) - 1

        def get_uid(name):
            return '_' + name + '_' + str(len(shader_src))

        # dont process commented lines
        if line.strip().startswith('//'):
            shader_src += [line]
            continue
        
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

            for macro, struct_declaration in struct_declarations:
                shader_src += ['#ifdef ' + macro + '\n']
                shader_src += [''.join(struct_declaration) + '\n']
                shader_src += ['#endif\n']
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
                flat_modifier = 'FLAT(' in line
                if flat_modifier:
                    elem_dtype = getMacro(elem_dtype)
                    line = get_whitespace(line) + getMacro(elem_dtype)+' '+elem_name+';\n'

                basename = getArrayBaseName(elem_name)
                macro = get_uid(basename)
                shader_src += ['#define ', macro, '\n']
                output_datapath = var + '_' + elem_name
                reference = None

                if sem == 'SV_POSITION':
                    output_datapath = 'gl_Position'
                    if shader.stage == Stages.TESC:
                        output_datapath = 'gl_out[gl_InvocationID].' + output_datapath
                elif sem == 'SV_POINTSIZE': output_datapath = 'gl_PointSize'
                elif sem == 'SV_DEPTH': output_datapath = 'gl_FragDepth'
                elif sem == 'SV_RENDERTARGETARRAYINDEX' : output_datapath = 'gl_Layer'
                else:
                    output_prefix, out_postfix = '', ''
                    if shader.stage == Stages.TESC:
                        output_prefix = 'patch '
                        out_postfix = '[]'
                        if shader.input_patch_arg:
                            out_postfix = '['+shader.input_patch_arg[1]+']'
                    if flat_modifier:
                        output_prefix = 'flat '+output_prefix
                    reference = ['layout(location = ', str(out_location),') ', output_prefix, 'out(', elem_dtype, ') ', output_datapath, out_postfix, ';']
                

                if shader.stage == Stages.TESC and sem != 'SV_POSITION':
                    output_datapath += '[gl_InvocationID]'

                # unroll attribute arrays
                if isArray(elem_name):
                    # assignment = ['for(int i=0; i<', str(getArrayLenFlat(elem_name)), ';i++) ']
                    # assignment += [var, '_', basename, '[i] = ', var, '.', basename, '[i]']
                    assignment = [var, '_', basename, ' = ', var, '.', basename, '']
                    
                    out_location += getArrayLen(shader.defines, elem_name)
                else:
                    if sem != 'SV_POSITION' and sem !='SV_POINTSIZE' and sem !='SV_DEPTH' and sem != 'SV_RENDERTARGETARRAYINDEX':
                        out_location += 1
                    if output_datapath == 'gl_Layer':
                        assignment = [output_datapath, ' = int(', var, '.', elem_name, ')']
                    else:
                        assignment = [output_datapath, ' = ', var, '.', elem_name]

                if reference:
                    struct_declarations += [(macro, reference)]
                return_assignments += [(macro, assignment)]

            # tesselation pcf output
            elif shader.pcf_returnType and parsing_struct in shader.pcf_returnType:
                # var = getMacro(shader.pcf_returnType)
                var = 'out_'+shader.pcf_returnType
                _, elem_name, sem = getMacro(line)
                sem = sem.upper()

                basename = getArrayBaseName(elem_name)
                macro = get_uid(basename)
                shader_src += ['#define ', macro, '\n']
                tess_var = ''
                if sem == 'SV_TESSFACTOR':
                    tess_var = 'gl_TessLevelOuter'
                if sem == 'SV_INSIDETESSFACTOR':
                    tess_var = 'gl_TessLevelInner'
                pcf_return_assignments += [(macro, [tess_var, ' = ', var, '.', basename, ';'])]

            # elif shader.entry_arg and parsing_struct in shader.entry_arg:
            elif is_input_struct(parsing_struct, shader):
                var = get_input_struct_var(parsing_struct, shader)
                elem_dtype, elem_name, sem = getMacro(line)
                sem = sem.upper()
                flat_modifier = 'FLAT(' in line
                if flat_modifier:
                    elem_dtype = getMacro(elem_dtype)
                    line = get_whitespace(line) + getMacro(elem_dtype)+' '+elem_name+';\n'
                is_array = isArray(elem_name)
                
                basename = getArrayBaseName(elem_name)
                macro = get_uid(basename)
                shader_src += ['#define ', macro, '\n']
                input_datapath = var + '_' + elem_name
                
                if sem == 'SV_POINTSIZE' or sem == 'SV_RENDERTARGETARRAYINDEX': continue
                # for vertex shaders, use the semantic as attribute name (for name-based reflection)
                input_value = sem if shader.stage == Stages.VERT else var + '_' + elem_name

                # tessellation
                in_postfix, in_prefix = '', ''
                if shader.stage == Stages.TESC: in_postfix = '[]'
                if shader.stage == Stages.TESE:
                    in_prefix = ' patch'
                    in_postfix = '[]'
                    if shader.output_patch_arg:
                        in_postfix = '['+shader.output_patch_arg[1]+']'

                if flat_modifier:
                    in_prefix = 'flat '+in_prefix

                reference = ['layout(location = ', str(in_location),')', in_prefix, ' in(', elem_dtype, ') ', input_value, in_postfix, ';']

                # input semantics
                if sem == 'SV_POSITION' and shader.stage == Stages.FRAG:
                    input_value = elem_dtype+'(float4(gl_FragCoord.xyz, 1.0f / gl_FragCoord.w))'
                    reference = []

                var_postfix = ''
                if shader.stage == Stages.TESC:
                    if sem == 'SV_POSITION':
                        input_value = 'gl_in[gl_InvocationID].gl_Position'
                        reference = []
                    else:
                        input_value = input_value + '[gl_InvocationID]'
                    var_postfix = '[gl_InvocationID]'

                if shader.stage == Stages.TESE:
                    if sem == 'SV_POSITION':
                        input_value = 'gl_in[0].gl_Position'
                        reference = []
                    elif shader.output_patch_arg and shader.output_patch_arg[0] in parsing_struct:
                        input_value = input_value + '[0]'
                    var_postfix = '[0]'

                if sem == 'SV_TESSFACTOR':
                    input_value = 'gl_TessLevelOuter'
                    var_postfix = ''
                    reference = []
                if sem == 'SV_INSIDETESSFACTOR':
                    input_value = 'gl_TessLevelInner'
                    var_postfix = ''
                    reference = []

                assignment = []

                if sem == 'SV_VERTEXID': input_value = 'gl_VertexIndex'
                # unroll attribute arrays
                # if is_array:
                #     assignment = ['for(int i=0; i<', str(getArrayLenFlat(elem_name)), ';i++) ']
                #     assignment += [var, '.', basename, '[i] = ', var, '_', basename, '[i]']
                # else:
                assignment = [var, var_postfix, '.', basename, ' = ',  input_value]

                if shader.stage == Stages.VERT and sem != 'SV_VERTEXID':
                    assignment += [';\n\t', sem]

                if sem != 'SV_POSITION':
                    in_location += 1
                if reference:
                    struct_declarations += [(macro, reference)]
                input_assignments += [(macro, assignment)]

        if (parsing_cbuffer or parsing_pushconstant) and line.strip().startswith('DATA('):
            dt, name, sem = getMacro(line)
            element_basename = getArrayBaseName(name)
            element_path = None
            if parsing_cbuffer:
                element_path = element_basename
            if parsing_pushconstant:
                element_path = parsing_pushconstant[0] + '.' + element_basename
            shader_src += ['#define _Get', element_basename, ' ', element_path, '\n']

        if 'CBUFFER' in line:
            fsl_assert(parsing_cbuffer == None, message='Inconsistent cbuffer declaration: \"' + line + '\"')
            parsing_cbuffer = tuple(getMacro(line))
        if '};' in line and parsing_cbuffer:
            parsing_cbuffer = None

        if 'PUSH_CONSTANT' in line:
            parsing_pushconstant = tuple(getMacro(line))
        if '};' in line and parsing_pushconstant:
            parsing_pushconstant = None

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
        if line.strip().startswith('PUSH_CONSTANT'):
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
            fsl_assert(len(resource_decl) == 5, binary.fsl_filepath, message='invalid Res declaration: \''+line+'\'')
            basename = getArrayBaseName(resource_decl[1])
            shader_src += ['#define _Get' + basename + ' ' + basename + '\n']
        
        # handle buffer resource declarations
        if resource_decl and is_buffer(resource_decl):
            shader_src += declare_buffer(resource_decl)
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
            shader_src += [line[:-1], ' \\\n']
            continue

        if '_MAIN(' in line:

            if shader.returnType and shader.returnType not in shader.structs:
                shader_src += ['layout(location = 0) out(', shader.returnType, ') out_', shader.returnType, ';\n']

            if shader.input_patch_arg:
                patch_size = shader.input_patch_arg[1]
                shader_src += ['layout(vertices = ', patch_size, ') out;\n']

            shader_src += ['void main()\n']
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            parsed_entry = True
            continue

        if parsed_entry and re.search('(^|\s+)RETURN', line):
            ws = get_whitespace(line)
            output_statement = [ws+'{\n']
            if shader.returnType:
                output_value = getMacro(line)
                if shader.returnType not in shader.structs:
                    output_statement += [ws+'\tout_'+shader.returnType+' = '+output_value+';\n']
                else:
                    output_statement += [ws+'\t'+shader.returnType+' out_'+shader.returnType+' = '+output_value+';\n']
            for macro, assignment in return_assignments:
                output_statement += ['#ifdef ' + macro + '\n']
                output_statement += [ws+'\t' + ''.join(assignment) + ';\n']
                output_statement += ['#endif\n']

            if shader.stage == Stages.TESC:
                output_statement += ['\t\t' + shader.pcf + '();\n']

            output_statement += [ws+'\treturn;\n'+ws+'}\n']
            shader_src += output_statement
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            continue

        if 'INIT_MAIN' in line:

            # assemble input
            for dtype, var in shader.struct_args:
                if shader.input_patch_arg and dtype in shader.input_patch_arg[0]:
                    dtype, dim, var = shader.input_patch_arg
                    shader_src += ['\t' + dtype + ' ' + var + '[' + dim + '];\n']
                    continue
                if shader.output_patch_arg and dtype in shader.output_patch_arg[0]:
                    dtype, dim, var = shader.output_patch_arg
                    shader_src += ['\t' + dtype + ' ' + var + '[' + dim + '];\n']
                    continue
                shader_src += ['\t' + dtype + ' ' + var + ';\n']
                
            for macro, assignment in input_assignments:
                shader_src += ['#ifdef ' + macro + '\n']
                shader_src += ['\t' + ''.join(assignment) + ';\n']
                shader_src += ['#endif\n']
            
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

        # tesselation
        if shader.pcf and shader.pcf in line and not pcf_returnType:
            loc = line.find(shader.pcf)
            pcf_returnType = line[:loc].strip()
            pcf_arguments = getMacro(line[loc:])
            _pcf_arguments = [arg for arg in pcf_arguments if 'INPUT_PATCH' in arg]
            ws = line[:len(line)-len(line.lstrip())]
            shader_src += [ws + 'void ' + shader.pcf + '()\n']
            continue

        if pcf_returnType and 'PCF_INIT' in line:
            ws = line[:len(line)-len(line.lstrip())]
            for dtype, dvar in shader.pcf_arguments:

                sem = getMacroName(dtype)
                innertype = getMacro(dtype)
                print(innertype, sem, dvar)
                if 'INPUT_PATCH' in sem:
                    shader_src += [ws + dtype + ' ' + dvar + ';\n']
                else:
                    shader_src += [ws + innertype + ' ' + dvar + ' = ' + sem.upper() + ';\n']

            for macro, assignment in input_assignments:
                shader_src += ['#ifdef ' + macro + '\n']
                shader_src += ['\t' + ''.join(assignment) + ';\n']
                shader_src += ['#endif\n']
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            continue

        if pcf_returnType and 'PCF_RETURN' in line:
            ws = get_whitespace(line)

            output_statement = [ws+'{\n']
            output_value = getMacro(line)
            
            output_statement += [ws+'\t'+pcf_returnType+' out_'+pcf_returnType+' = '+output_value+';\n']
            for macro, assignment in pcf_return_assignments:
                output_statement += ['#ifdef ' + macro + '\n']
                output_statement += [ws+'\t' + ''.join(assignment) + '\n']
                output_statement += ['#endif\n']
            output_statement += [
                ws+'\treturn;\n',
                ws+'}\n'
            ]
            shader_src += output_statement
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            continue

        if shader_src_len != len(shader_src):
            shader_src += ['#line {}\n'.format(line_index)]

        shader_src += [line]

    open(dst, 'w').writelines(shader_src)
    return 0, []