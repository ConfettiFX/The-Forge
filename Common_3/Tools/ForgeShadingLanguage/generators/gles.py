""" GLSL shader generation """

from utils import Platforms, Stages, getHeader, getMacro, genFnCall, getShader, getMacroName, get_whitespace
from utils import isArray, getArrayLen, resolveName, getArrayBaseName, fsl_assert, ShaderBinary
from utils import is_input_struct, get_input_struct_var, getArrayLenFlat, is_groupshared_decl
import os, sys, re, math
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

def is_sampler(fsl_declaration):
    return 'SamplerState' in fsl_declaration[0]

def getFloatCount(var_type):
    _map = {
        'bool'      : 1,
        'int'       : 1,
        'int2'      : 2, 
        'int3'      : 3,
        'int4'      : 4,
        'uint'      : 1,
        'uint2'     : 2,
        'uint3'     : 3,
        'uint4'     : 4,
        'float'     : 1,
        'float2'    : 2,
        'float3'    : 3,
        'float4'    : 4,
        'float2x2'  : 4,
        'float3x3'  : 9,
        'float4x4'  : 16,
        'double2'   : 2,
        'double3'   : 3,
        'double4'   : 4,
        'double2x2' : 4,
        'double3x3' : 9,
        'double4x4' : 16,
    }
    assert var_type in _map, 'Cannot map {} to format float count'.format(var_type)
    return _map[var_type]

def getFloatSize(var_type, struct_map):
    _map = {
        'bool'      : 1,
        'int'       : 1,
        'int2'      : 2, 
        'int3'      : 4,
        'int4'      : 4,
        'uint'      : 1,
        'uint2'     : 2,
        'uint3'     : 4,
        'uint4'     : 4,
        'float'     : 1,
        'float2'    : 2,
        'float3'    : 4,
        'float4'    : 4,
        'float2x2'  : 4,
        'float3x3'  : 12,
        'float4x4'  : 16,
        'double2'   : 2,
        'double3'   : 4,
        'double4'   : 4,
        'double2x2' : 4,
        'double3x3' : 12,
        'double4x4' : 16,
    }

    if var_type in struct_map:
        return struct_map[var_type]['floatSize']

    assert var_type in _map, 'Cannot map {} to format float size'.format(var_type)
    return _map[var_type]

def is_buffer(fsl_declaration):
    # TODO: endswith better here?
    return 'Buffer' in fsl_declaration[0]


# writeable textures get translated to glsl images
def is_rw_texture(fsl_declaration):
    dtype = fsl_declaration[0]
    writeable_types = [
        'RasterizerOrderedTex2D',
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

def getSemanticName(sem):
    if 'POSITION' == sem: return 'Position'
    elif 'NORMAL' == sem: return 'Normal'
    elif 'COLOR' == sem: return 'Color'
    elif 'COLOR0' == sem: return 'Color'
    elif 'TANGENT' == sem: return 'Tangent'
    elif 'BINORMAL' == sem: return 'Binormal'
    elif 'JOINTS' == sem: return 'Joints'
    elif 'WEIGHTS' == sem: return 'Weights'
    elif 'TEXCOORD' == sem: return 'UV'
    elif 'TEXCOORD0' == sem: return 'UV'
    elif 'TEXCOORD1' == sem: return 'UV1'
    elif 'TEXCOORD2' == sem: return 'UV2'
    elif 'TEXCOORD3' == sem: return 'UV3'
    elif 'TEXCOORD4' == sem: return 'UV4'
    elif 'TEXCOORD5' == sem: return 'UV5'
    elif 'TEXCOORD6' == sem: return 'UV6'
    elif 'TEXCOORD7' == sem: return 'UV7'
    elif 'TEXCOORD8' == sem: return 'UV8'
    elif 'TEXCOORD9' == sem: return 'UV9'
    else: return None

def setUBOConversion(elem_dtype, ubo_name, float_offset, float_stride, is_array, struct_map):
    assert not elem_dtype in struct_map, 'Expecting native type during UBO conversion'
    
    out = elem_dtype + '('

    extractionCount = getFloatCount(elem_dtype)
    float4_stride = math.ceil(float_stride/4)

    # Check if we can extract in parts of 4 floats
    extractFullVec4 = False
    if extractionCount % 4 == 0:
        extractFullVec4 = True
        extractionCount = int(extractionCount/4)

    for j in range(extractionCount):
        xyzw = (j + float_offset) % 4
        strExt = '.x'
        offset = math.floor((float_offset + j) / 4)
        if extractFullVec4:
            strExt = ''
            offset = math.ceil(float_offset / 4) + j
        elif xyzw == 1:
            strExt = '.y'
        elif xyzw == 2:
            strExt = '.z'
        elif xyzw == 3:
            strExt = '.w'

        out += ubo_name + '[' + str(offset) + (' + # * ' + str(float4_stride) if is_array else '') + ']' + strExt

        if not j == extractionCount -1:
            out += ', '

    out += ')'
    return out

def gles(debug, binary: ShaderBinary, dst):

    fsl = binary.preprocessed_srcs[Platforms.GLES]

    shader = getShader(Platforms.GLES, binary.fsl_filepath, fsl, dst, line_directives=False)

    #Only vertex and fragment shaders are valid for OpenGL ES 2.0
    if shader.stage != Stages.VERT and shader.stage != Stages.FRAG:
        print("Invalid OpenGL ES 2.0 shader given, only .vert and .frag shaders are valid.")
        return 1

    shader_src = getHeader(fsl)
    shader_src += ['#version 100\n', '\nprecision highp float;\nprecision highp int;\n\n']
    shader_src += ['#define STAGE_', shader.stage.name, '\n']
    shader_src += ['#define GLES\n']

    header_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'includes', 'gles.h')
    gles_includes = {header_path: 0}

    shader_src += [f'#line 0 {gles_includes[header_path]} //{header_path}\n']
    header_lines = open(header_path).readlines()
    shader_src += header_lines + ['\n']

    if True or not os.path.exists(dst):
        os.makedirs(os.path.dirname(dst), exist_ok=True)

    # retrieve output patch size
    patch_size = 0
    for line in shader.lines:
        if 'OUTPUT_CONTROL_POINTS' in line:
            patch_size = int(getMacro(line))

    arrBuffs = ['Get('+getArrayBaseName(res[1])+')' for res in shader.resources if 'Buffer' in res[0] and (isArray(res[1]))]
    returnType = None if not shader.returnType else (getMacroName(shader.returnType), getMacro(shader.returnType))

    skip_line = False
    parsing_struct = None
    
    parsing_comments = False
    
    parsing_ubuffer = None
    parsing_ubuffer_float_count = 0

    nonuniformresourceindex = None
    parsed_entry = False

    struct_construction = {}
    get_declarations = {}
    struct_declarations = []
    input_assignments = []
    return_assignments = []

    def is_struct(var_type):
        return var_type in struct_construction

    def set_ubo(ubo_name, basename, elemname, elem_type, getname, float_stride, fromStruct, isArray, float_offset, result):
            if(is_struct(elem_type)):
                structItem = struct_construction[elem_type]
                for uniformIndex in range(structItem['uniformCount']):
                    elem_dtype, elem_name, _ = getMacro(structItem[uniformIndex])
                    struct_get_name = getname + '_' + elem_name.upper()
                    float_offset, result = set_ubo(ubo_name, basename, elemname + ('\.' if isArray else '.') + elem_name, elem_dtype, struct_get_name, float_stride, True, isArray, float_offset, result) #recurse
            else:
                if isArray:
                    if fromStruct:
                        get_declarations['Get\(' + basename + '\)\[.+\]' + elemname] = (getname+'(#)', True)
                    else:
                        get_declarations['Get\(' + basename + '\)\[.+\]'] = (getname+'(#)', True)
                else:
                    if fromStruct:
                        get_declarations['Get(' + basename + ')' + elemname] = (getname, False)
                    else:
                        get_declarations['Get(' + basename + ')'] = (getname, False)

                elem_float_size = getFloatSize(elem_type, struct_construction)
                element_path = setUBOConversion(elem_type, ubo_name, float_offset, float_stride, isArray, struct_construction)
                if isArray:
                    replaced_value = element_path.replace('#', 'X')
                    result += '#define ' + getname + '(X) ' + replaced_value + '\n'
                else:
                    result += '#define ' + getname + ' ' + element_path + '\n'

                float_offset += elem_float_size

            return float_offset, result

    def declare_buffer(fsl_declaration):
        #Set a default max buffer size, GLSL v100 does not allow unknown array sizes
        #TODO: We might want to use a texture as buffer instead to allow unknown large buffer sizes (requires engine changes)
        default_max_buffer_size = 256 
        buffer_type, name, _, _, _ = fsl_declaration
        data_type  = getMacro(buffer_type)

        assert not isArray(name), 'Cannot use array of buffers in glsl v100'

        result = []
        stride = getFloatSize(data_type, struct_construction)      
        get_name = 'Get_' + name.upper()

        _, result = set_ubo(name, name, '', data_type, get_name, stride, False, True, 0, result)

        result += 'uniform float4 {}[{}];\n'.format(name, str(default_max_buffer_size))
        return result

    line_index = 0
    current_src = binary.fsl_filepath

    for line in shader.lines:

        line_index += 1
        shader_src_len = len(shader_src)

        if line.startswith('#line'):
            l, s = line.split(maxsplit=2)[1:]
            if s not in gles_includes:
                gles_includes[s] = len(gles_includes)
            include_index = gles_includes[s]
            shader_src += [f'#line {l} {include_index} //{s}']
            line_index = int(l) - 1
            current_src = s
            continue

        def get_uid(name):
            return '_' + name + '_' + str(len(shader_src))

        # dont process commented lines
        if line.strip().startswith('//'):
            shader_src += [line]
            continue

        if line.strip().startswith('/*'):
            parsing_comments = True
            continue

        if line.strip().startswith('*/'):
            parsing_comments = False
            continue
        if parsing_comments:
            continue

        if 'INDIRECT_DRAW()' in line:
            continue

        # TODO: handle this differently
        if '#ifdef NO_FSL_DEFINITIONS' in line:
            skip_line = True
        if skip_line and '#endif' in line:
            skip_line = False
            continue
        if skip_line:
            continue

        if line.strip().startswith('STRUCT('):
            parsing_struct = getMacro(line)
            struct_construction[parsing_struct] = {'floatSize': 0, 'uniformCount': 0}
        if parsing_struct and '};' in line:
            if not line.endswith('\n'): line += '\n'
            shader_src += [line]
            print("{} struct size = {}".format(parsing_struct, str(struct_construction[parsing_struct]['floatSize'])))
            for macro, struct_declaration in struct_declarations:
                shader_src += ['#ifdef ', macro, '\n']
                shader_src += [*struct_declaration, '\n']
                shader_src += ['#endif\n']
            shader_src += [f'#line {line_index} {gles_includes[current_src]} //{current_src}\n']
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

                basename = getArrayBaseName(elem_name)
                macro = get_uid(basename)
                shader_src += ['#define ', macro, '\n']
                output_datapath = elem_name
                reference = None

                if sem == 'SV_POSITION':
                    output_datapath = 'gl_Position'
                elif sem == 'SV_POINTSIZE': output_datapath = 'gl_PointSize'
                elif sem == 'SV_DEPTH': output_datapath = 'gl_FragDepth'
                else:
                    reference = ['RES_OUT(', elem_dtype, ', ', output_datapath,');']
                
                # unroll attribute arrays
                if isArray(elem_name):
                    assignment = [var, '_', basename, ' = ', var, '.', basename, '']
                    
                else:
                    assignment = [output_datapath, ' = ', var, '.', elem_name]

                if reference:
                    struct_declarations += [(macro, reference)]
                return_assignments += [(macro, assignment)]

            # elif shader.entry_arg and parsing_struct in shader.entry_arg:
            elif is_input_struct(parsing_struct, shader):
                var = get_input_struct_var(parsing_struct, shader)
                elem_dtype, elem_name, sem = getMacro(line)
                sem = sem.upper()
                flat_modifier = 'FLAT(' in line
                if flat_modifier:
                    elem_dtype = getMacro(elem_dtype)
                    line = get_whitespace(line) + getMacro(elem_dtype) + ' '+ elem_name +';\n'

                noperspective_modifier = 'noperspective(' in line
                if noperspective_modifier:
                    elem_dtype = getMacro(elem_dtype)
                    line = get_whitespace(line) + getMacro(elem_dtype) + ' '+ elem_name +';\n'

                is_array = isArray(elem_name)
                
                basename = getArrayBaseName(elem_name)
                macro = get_uid(basename)
                shader_src += ['#define ', macro, '\n']
                input_datapath = var + '_' + elem_name
                
                if sem == 'SV_POINTSIZE': continue

                input_value = None
                if shader.stage == Stages.VERT:
                    # for vertex shaders, use the semantic as attribute name (for name-based reflection)
                    input_value = getSemanticName(sem)
                    if not input_value:
                        input_value = elem_name
                    reference = ['RES_IN(', elem_dtype, ', ', input_value, ');']
                elif shader.stage == Stages.FRAG:
                    # for fragment shaders, set the input as RES_OUT to be a varying
                    input_value = elem_name
                    reference = ['RES_OUT(', elem_dtype, ', ', input_value, ');']

                if flat_modifier:
                    reference.insert(0, 'flat ')
                if noperspective_modifier:
                    reference.insert(0, 'noperspective ')

                # input semantics
                if sem == 'SV_POSITION' and shader.stage == Stages.FRAG:
                    input_value = elem_dtype+'(float4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w))'
                    reference = []

                assignment = []

                if sem == 'SV_VERTEXID': input_value = 'gl_VertexIndex'
                # unroll attribute arrays
                # if is_array:
                #     assignment = ['for(int i=0; i<', str(getArrayLenFlat(elem_name)), ';i++) ']
                #     assignment += [var, '.', basename, '[i] = ', var, '_', basename, '[i]']
                # else:
                assignment = [var, '.', basename, ' = ',  input_value]
                    
                if reference:
                    struct_declarations += [(macro, reference)]
                input_assignments += [(macro, assignment)]

            else:       
                # Store struct information for later usage in declaring UBOs
                elem_dtype, elem_name, sem = getMacro(line)
                struct_construction[parsing_struct]['floatSize'] += getFloatSize(elem_dtype, struct_construction)
                uniformCount = struct_construction[parsing_struct]['uniformCount']
                struct_construction[parsing_struct][uniformCount] = line
                struct_construction[parsing_struct]['uniformCount'] += 1

        # Handle uniform buffers
        if parsing_ubuffer and line.strip().startswith('DATA('):
            elem_dtype, name, sem = getMacro(line)
            element_basename = getArrayBaseName(name)
            result = []
            float_stride = getFloatSize(elem_dtype, struct_construction) 

            array_length = 1
            if isArray(name):
                array_length = getArrayLen(shader.defines, name)

            get_name = 'Get_' + element_basename.upper()

            _, result = set_ubo(parsing_ubuffer, element_basename, '', elem_dtype, get_name, float_stride, False, isArray(name), parsing_ubuffer_float_count, result)
            parsing_ubuffer_float_count += float_stride * (array_length)

            shader_src += result
            continue

        if 'CBUFFER' in line:
            fsl_assert(parsing_ubuffer == None, message='Inconsistent cbuffer declaration: \"' + line + '\"')
            parsing_ubuffer, _, _, _= getMacro(line)
            continue
        if 'PUSH_CONSTANT' in line:
            fsl_assert(parsing_ubuffer == None, message='Inconsistent push_constant declaration: \"' + line + '\"')
            parsing_ubuffer, _ = getMacro(line)
            continue
        if '{' in line and parsing_ubuffer:
            continue
        if '};' in line and parsing_ubuffer:
            parsing_ubuffer_float_count = math.ceil(parsing_ubuffer_float_count / 4)
            shader_src += ['uniform float4 ', parsing_ubuffer, '[', str(parsing_ubuffer_float_count),'];\n']
            parsing_ubuffer_float_count = 0
            parsing_ubuffer = None
            continue


        resource_decl = None
        if line.strip().startswith('RES('):
            resource_decl = getMacro(line)
            print(resource_decl)
            fsl_assert(len(resource_decl) == 5, fsl, message='invalid Res declaration: \''+line+'\'')

            basename = getArrayBaseName(resource_decl[1])           
            get_name = 'Get_' + basename.upper()

            # No samplers available in GLSL v100, define NO_SAMPLER
            if resource_decl and is_sampler(resource_decl):
                get_declarations['Get(' + basename + ')'] = (get_name, False)
                shader_src += ['#define ' + get_name + ' NO_SAMPLER' '\n']
                continue

            # Handle buffer resource declarations for GLES
            if is_buffer(resource_decl):
                shader_src += declare_buffer(resource_decl)
                continue

            basename = getArrayBaseName(resource_decl[1])           

            get_name = 'Get_' + basename.upper()
            get_declarations['Get(' + basename + ')'] = (get_name, False)
            shader_src += ['#define ' + get_name + ' ' + basename + '\n']

        #TODO handle references to arrays of structured buffers for GLES
        #for buffer_array in arrBuffs:
        #    line = insert_buffer_array_indirections(line, buffer_array)

        #TODO specify format qualified image resource for GLES
        if resource_decl and is_rw_texture(resource_decl):
            #shader_src += declare_rw_texture(resource_decl)
            continue
        
        #TODO Check if usage is needed within GLES
        if 'BeginNonUniformResourceIndex(' in line:
            #index, max_index = getMacro(line), None
            #assert index != [], 'No index provided for {}'.format(line)
            #if type(index) == list:
            #    max_index = index[1]
            #    index = index[0]
            #nonuniformresourceindex = index
            ## if type(max_index) is str:
            #if max_index and not max_index.isnumeric():
            #    max_index = ''.join(c for c in shader.defines[max_index] if c.isdigit())
            #shader_src += BeginNonUniformResourceIndex(nonuniformresourceindex, max_index)
            continue
        if 'EndNonUniformResourceIndex()' in line:
            #assert nonuniformresourceindex, 'EndNonUniformResourceIndex: BeginNonUniformResourceIndex not called/found'
            #shader_src += EndNonUniformResourceIndex(nonuniformresourceindex)
            #nonuniformresourceindex = None
            continue
        if nonuniformresourceindex :
            #shader_src += [line[:-1], ' \\\n']
            continue

        if '_MAIN(' in line:

            for dtype, dvar in shader.flat_args:
                sem = getMacroName(dtype).upper()
                if sem == 'SV_INSTANCEID':
                    shader_src += ['uniform int ', sem, ';\n\n']

            if shader.returnType and shader.stage == Stages.VERT and shader.returnType not in shader.structs:
                shader_src += ['RES_OUT(', shader.returnType, ', ', 'out_', shader.returnType, ');\n']

            #TODO Check if this is needed somewere
            #if shader.input_patch_arg:
                #patch_size = shader.input_patch_arg[1]
                #shader_src += ['layout(vertices = ', patch_size, ') out;\n']

            shader_src += ['void main()\n']
            shader_src += [f'#line {line_index} {gles_includes[current_src]} //{current_src}\n']
            parsed_entry = True
            continue

        if parsed_entry and re.search('(^|\s+)RETURN', line):
            ws = get_whitespace(line)
            output_statement = [ws+'{\n']
            if shader.returnType:
                output_value = getMacro(line)
                if shader.returnType not in shader.structs:
                    if shader.stage == Stages.FRAG:
                        output_statement += [ws+'\tgl_FragColor = '+output_value+';\n']
                    else:
                        output_statement += [ws+'\tout_'+shader.returnType+' = '+output_value+';\n']
                else:
                    output_statement += [ws+'\t'+shader.returnType+' out_'+shader.returnType+' = '+output_value+';\n']
            for macro, assignment in return_assignments:
                output_statement += ['#ifdef ', macro, '\n']
                output_statement += [ws+'\t', *assignment, ';\n']
                output_statement += ['#endif\n']
                
            output_statement += [ws+'}\n']
            shader_src += output_statement
            shader_src += [f'#line {line_index} {gles_includes[current_src]} //{current_src}\n']
            continue

        if 'INIT_MAIN' in line:

            # assemble input
            for dtype, var in shader.struct_args:
                if shader.input_patch_arg and dtype in shader.input_patch_arg[0]:
                    dtype, dim, var = shader.input_patch_arg
                    shader_src += ['\t', dtype, ' ', var, '[', dim, '];\n']
                    continue
                if shader.output_patch_arg and dtype in shader.output_patch_arg[0]:
                    dtype, dim, var = shader.output_patch_arg
                    shader_src += ['\t', dtype, ' ', var, '[', dim, '];\n']
                    continue
                shader_src += ['\t', dtype, ' ', var, ';\n']
                
            for macro, assignment in input_assignments:
                shader_src += ['#ifdef ', macro, '\n']
                shader_src += ['\t', *assignment, ';\n']
                shader_src += ['#endif\n']
            
            ''' additional inputs '''
            for dtype, dvar in shader.flat_args:
                innertype = getMacro(dtype)
                semtype = getMacroName(dtype)
                shader_src += ['\t'+innertype+' '+dvar+' = '+innertype+'('+semtype.upper()+');\n']
            shader_src += [f'#line {line_index} {gles_includes[current_src]} //{current_src}\n']

            ''' generate a statement for each vertex attribute
                this should not be necessary, but it influences
                the attribute order for the SpirVTools shader reflection
            '''
            # if shader.stage == Stages.VERT:# and shader.entry_arg:
            #     for dtype, var in shader.struct_args:
            #         for _, n, s in shader.structs[dtype]:
            #             if s.upper() == 'SV_VERTEXID': continue
            #             shader_src += ['\t', s ,';\n']
            continue

        updatedline = line

        for key, value in get_declarations.items():
            if value[1] and isArray(updatedline):
                # Regex match and replace
                replace_value = value[0].replace('#', getArrayLenFlat(updatedline))
                updatedline = re.sub(key, replace_value, updatedline)
            elif key in updatedline:
                updatedline = updatedline.replace(key, value[0])

        #Strip .f -> .0 | 0f -> 0 float value definitions, those are not supported on GLES
        def replacef(matchReg):
            if len(matchReg.group(0)) > 2:
                return matchReg.group(0)[:2] + '0'
            return matchReg.group(0)[0]

        if shader_src_len != len(shader_src):
            shader_src += [f'#line {line_index} {gles_includes[current_src]} //{current_src}\n']

        shader_src += [re.sub('\d\.?f', replacef, updatedline)]

    open(dst, 'w').writelines(shader_src)
    return 0, []