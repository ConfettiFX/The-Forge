""" metal shader generation """

from utils import Stages, getHeader, getShader, getMacro, genFnCall, getMacroName, isBaseType
from utils import isArray, getArrayLen , getArrayBaseName, resolveName, DescriptorSets, dictAppendList, ShaderTarget
from utils import is_input_struct, get_input_struct_var, fsl_assert, get_whitespace, get_array_dims, Platforms, ShaderBinary
from utils import get_array_decl, visibility_from_stage, get_fn_table, is_groupshared_decl, collect_shader_decl
import os, sys, re
from shutil import copyfile

targetToMslEntry = {
    Stages.VERT: 'vertex',
    Stages.FRAG: 'fragment',
    Stages.COMP: 'kernel',
    Stages.TESC: 'kernel',
    Stages.TESE: 'vertex',
}

def typeToMember(name):
    return 'm_'+name

# expand inline cbuffer declaration to match other resources
def resource_from_cbuffer_decl(cbuffer_decl):
    return ('CBUFFER', *cbuffer_decl)

def get_mtl_patch_type(tessellation_layout):
    mtl_patch_types = {
        'quad': 'quad',
        'triangle': 'triangle',
    }
    domain = tessellation_layout[0].strip('"')
    fsl_assert(domain in mtl_patch_types, message='Cannot map domain to mtl patch type: ' + domain)
    return mtl_patch_types[domain]

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

    shader_src = getHeader(fsl)
    shader_src += ['#define METAL\n']
    shader_src += [f'#define TARGET_{platform.name}\n']    

    if shader.enable_waveops:
        shader_src += ['#define ENABLE_WAVEOPS()\n']

    # shader_src += ['#include "includes/metal.h"\n']
    # incPath = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'includes', 'metal.h')
    # dstInc = os.path.join(os.path.dirname(dst), "includes/metal.h")
    # if True or not os.path.exists(dstInc):
    #     os.makedirs(os.path.dirname(dstInc), exist_ok=True)
    #     copyfile(incPath, dstInc)
        
    # directly embed metal header in shader
    shader_src += ['#include "includes/metal.h"\n']

    def getMetalResourceID(dtype: str, reg: str) -> str:
        S_OFFSET, U_OFFSET, B_OFFSET = 1024, 1024*2, 1024*3
        if 'FSL_REG' in reg:
            reg = getMacro(reg)[-1]
        register = int(reg[1:])
        if 't' == reg[0]: # SRV
            return register
        if 's' == reg[0]: # samplers
            return S_OFFSET + register
        if 'u' == reg[0]: # UAV
            return U_OFFSET + register
        if 'b' == reg[0]: # CBV
            return B_OFFSET + register
        assert False, '{}'.format(reg)

    # for metal only consider resource declared using the following frequencies
    metal_ab_frequencies = [
        'UPDATE_FREQ_NONE',
        'UPDATE_FREQ_PER_FRAME',
        'UPDATE_FREQ_PER_BATCH',
        'UPDATE_FREQ_PER_DRAW',
    ]

    # collect array resources
    ab_elements = {}
    for resource_decl in shader.resources:
        name, freq = resource_decl[1:3]
        if isArray(name):
            ab_elements[freq] = []

    # tessellation layout, used for mtl patch type declaration
    tessellation_layout = None

    # consume any structured inputs, since an intermediate struct will be used and manually fed to DS
    if shader.stage == Stages.TESE:
        shader.struct_args = []

    
    global_references = {}
    global_reference_paths = {}
    global_reference_args = {}
    global_fn_table = get_fn_table(shader.lines)

    # transform VS entry into callable fn
    if shader.stage == Stages.TESC:
        vsb = ShaderBinary()
        vsb.stage = Stages.VERT
        vsb.filename = shader.vs_reference
        o = Opts(debug)
        vertex_shader_decl, _ = collect_shader_decl(o, shader.vs_reference, [platform], None, None, [ vsb ])
        vertex_shader_source = vertex_shader_decl[0].preprocessed_srcs[platform]
        vertex_shader = getShader(platform, shader.vs_reference, vertex_shader_source, dst)
        vs_main, vs_parsing_main = [], -2

        struct = None
        elem_index = 0

        # add vertex input as a shader resource to tesc
        for struct in vertex_shader.struct_args:
            res_decl = 'RES(Buffer(VSIn), vertexInput, UPDATE_FREQ_NONE, b0, binding = 0)'
            shader.lines.insert(0, res_decl + ';\n')
            shader.lines.insert(0, 'struct VSIn;\n')

        # add mtl tessellation buffer outputs
        r0_decl = 'RES(RWBuffer(HullOut), hullOutputBuffer, UPDATE_FREQ_NONE, b1, binding = 0)'
        r0 = getMacroName(shader.returnType)
        shader.lines.insert(0, r0_decl + ';\n')
        shader.lines.insert(0, 'struct ' +r0+ ';\n')

        r1_decl = 'RES(RWBuffer(PatchTess), tessellationFactorBuffer, UPDATE_FREQ_NONE, b2, binding = 0)'
        r1 = getMacroName(shader.pcf_returnType)
        shader.lines.insert(0, r1_decl + ';\n')
        shader.lines.insert(0, 'struct ' +r1+ ';\n')

        # TODO: this one is necessary to determine the total # of vertices to fetch from the buffer,
        # this should be handled generically
        di_decl = 'RES(Buffer(DrawIndirectInfo), drawInfo, UPDATE_FREQ_NONE, b3, binding = 0)'
        shader.lines.insert(0, di_decl + ';\n')
        shader.lines.insert(0, 'struct DrawIndirectInfo {uint vertexCount; uint _data[3];};\n')

        # collect VSMain lines, transform inputs into const refs which will get manually fed from buffer
        for line in vertex_shader.lines:
            if line.strip().startswith('//'): continue

            if line.strip().startswith('STRUCT('):
                struct = getMacro(line)

            if is_input_struct(struct, vertex_shader):
                if line.strip().startswith('DATA('):
                    dtype, dname, _ = getMacro(line)
                    vs_main += [get_whitespace(line), dtype, ' ', dname, ' [[attribute(', str(elem_index), ')]];\n']
                    elem_index += 1
                else:
                    vs_main += [line]

            if struct and '};' in line:
                struct = None

            if 'VS_MAIN(' in line:
                sig = line.strip().split(' ', 1)
                line = getMacroName(sig[0]) + ' VSMain('
                l0 = len(line)
                prefix = ''
                for dtype, dname in vertex_shader.struct_args:
                    line = prefix + line + 'constant ' + dtype + '& ' + dname
                    prefix = ', '
                vs_parsing_main = -1
                line = line+')\n'
                vs_main += [line, '{\n']
                global_fn_table['VSMain'] = (line, (999999, l0))
                continue

            if vs_parsing_main > -1:

                l_get = line.find('Get(')
                while l_get > 0:
                    resource = line[l_get:]
                    resource = resource[4:resource.find(')')]
                    fn = 'VSMain'
                    if fn not in global_references: global_references[fn] = set()
                    global_references[fn].add(resource)
                    l_get = line.find('Get(', l_get+1)

                if 'INIT_MAIN' in line and vertex_shader.returnType:
                    continue
                    # mName = getMacroName(vertex_shader.returnType)
                    # mArg = getMacro(vertex_shader.returnType)
                    # line = get_whitespace(line) + '{} {};\n'.format(mName, mArg)
                if re.search('(^|\s+)RETURN', line) and vertex_shader.returnType:
                    line = line.replace('RETURN', 'return ')
                vs_main += [line]

            if vs_parsing_main > -2 and '{' in line: vs_parsing_main += 1
            if vs_parsing_main > -2 and '}' in line: vs_parsing_main -= 1

    skip = False
    struct = None
    mainArgs = []

    # statements which need to be inserted into INIT_MAIN
    entry_declarations = []

    parsing_cbuffer = None
    struct_uid = None
    parsing_pushconstant = None

    # reserve the first 6 buffer locations for ABs(4) + rootcbv + rootconstant
    buffer_location  = 6
    texture_location = 0
    sampler_location = 0
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

    if shader.enable_waveops:
        global_reference_paths['simd_lane_id'] = 'simd_lane_id'
        global_reference_args['simd_lane_id'] = 'const uint simd_lane_id'

    def declare_argument_buffers(mainArgs):
        ab_decl = []
        # declare argument buffer structs
        for freq, elements in ab_elements.items():
            argBufType = 'AB_' + freq
            ab_decl += ['\n\t// Generated Metal Resource Declaration: ', argBufType, '\n' ]

            # skip empty update frequencies
            if not elements: continue

            # make AB declaration only active if any member is defined
            ab_macro = get_uid(argBufType)
            space = 'constant'
            mainArgs += [(ab_macro, [space, ' struct ', argBufType, '& ', argBufType, '[[buffer({})]]'.format(freq)])]

            ab_decl += ['\tstruct ', argBufType, '\n\t{\n']
            for macro, elem in elements:
                ab_decl += ['\t\t', *elem, ';\n']

            ab_decl += ['\t};\n']
        return ab_decl

    line_index = 0
    
    last_res_decl = 0
    explicit_res_decl = None
    for line in shader.lines:

        line_index += 1
        shader_src_len = len(shader_src)
        if line.startswith('#line'):
            line_index = int(line.split()[1]) - 1

        def get_uid(name):
            return name + '_' + str(len(shader_src))

        # dont process commented lines
        if line.strip().startswith('//'):
            shader_src += ['\t', line]
            continue

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

        if 'BeginNonUniformResourceIndex' in line:
            nuri = getMacro(line)
            if type(nuri) is str:
                line = line.replace(nuri, nuri + ', None')

        if 'TESS_LAYOUT(' in line:
            tessellation_layout = getMacro(line)

        #  Shader I/O
        if struct and line.strip().startswith('DATA('):
            if shader.returnType and struct in shader.returnType:
                var = getMacro(shader.returnType)
                dtype, name, sem = getMacro(line)
                sem = sem.upper()
                base_name = getArrayBaseName(name)
                macro = get_uid(base_name)

                output_semantic = ''
                if 'SV_POSITION' in sem:
                    output_semantic = '[[position]]'
                if 'SV_POINTSIZE' in sem:
                    output_semantic = '[[point_size]]'
                if 'SV_DEPTH' in sem:
                    output_semantic = '[[depth(any)]]'
                if 'SV_RENDERTARGETARRAYINDEX' in sem:
                    output_semantic = '[[render_target_array_index]]'

                color_location = sem.find('SV_TARGET')
                if color_location > -1:
                    color_location = sem[color_location+len('SV_TARGET'):]
                    if not color_location: color_location = '0'
                    output_semantic = '[[color('+color_location+')]]'

                shader_src += ['#line {}\n'.format(line_index)]
                shader_src += [get_whitespace(line), dtype, ' ', name, ' ', output_semantic, ';\n']
                continue

            elif is_input_struct(struct, shader):
                var = get_input_struct_var(struct, shader)
                dtype, name, sem = getMacro(line)
                sem = sem.upper()
                macro = get_uid(getArrayBaseName(name))
                # for vertex shaders, use the semantic as attribute name (for name-based reflection)
                n2 = sem if shader.stage == Stages.VERT else getArrayBaseName(name)
                if isArray(name):
                    base_name = getArrayBaseName(name)
                    array_length = int(getArrayLen(shader.defines, name))
                    assignment = []
                    for i in range(array_length): assignment += ['\t_',var, '.', base_name, '[', str(i), '] = ', var, '.', base_name, '_', str(i), '; \\\n']
                    # TODO: handle this case
                
                attribute = ''
                if shader.stage == Stages.VERT:
                    attribute = '[[attribute('+str(attribute_index)+')]]'
                    attribute_index += 1
                elif shader.stage == Stages.FRAG:
                    attribute = ''
                    if 'SV_POSITION' in sem:
                        attribute = '[[position]]'
                    elif 'SV_RENDERTARGETARRAYINDEX' in sem:
                        attribute = '[[render_target_array_index]]'
                shader_src += ['#line {}\n'.format(line_index)]
                shader_src += [get_whitespace(line), dtype, ' ', name, ' ', attribute, ';\n']
                continue

            # metal requires tessellation factors of dtype half
            if shader.stage == Stages.TESC or shader.stage == Stages.TESE:
                dtype, name, sem = getMacro(line)
                sem = sem.upper()
                if sem == 'SV_TESSFACTOR' or sem == 'SV_INSIDETESSFACTOR':
                    line = line.replace(dtype, 'half')

            # since we directly use the dtype of output patch for the mtl patch_control_point template,
            # the inner struct needs attributes
            if shader.stage == Stages.TESE:
                if struct == shader.output_patch_arg[0]:
                    dtype, name, sem = getMacro(line)
                    elem = tuple(getMacro(line))
                    attribute_index = str(shader.structs[struct].index(elem))
                    line = line.replace(name, name + ' [[attribute(' + attribute_index + ')]]')

        # handle cbuffer and pushconstant elements
        if (parsing_cbuffer or parsing_pushconstant) and line.strip().startswith('DATA('):
            dt, name, sem = getMacro(line)
            element_basename = getArrayBaseName(name)
            macro = get_uid(element_basename)

            if parsing_cbuffer:
                elemen_path = parsing_cbuffer[0] + '.' + element_basename
                is_embedded = parsing_cbuffer[1] in ab_elements
                # for non-embedded cbuffer, access directly using struct access
                global_reference_paths[element_basename] = parsing_cbuffer[0]
                global_reference_args[element_basename] = 'constant struct ' + parsing_cbuffer[0] + '& ' + parsing_cbuffer[0]
                if is_embedded:
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

            macro = get_uid(basename)
            entry_declarations += [(macro, ['threadgroup {} {};'.format(dtype, dname)])]

            array_decl = get_array_decl(dname)

            global_reference_paths[basename] = basename
            global_reference_args[basename] = 'threadgroup ' + dtype + ' (&' + basename + ')' + array_decl

            shader_src += ['#define _Get_', basename, ' ', basename, '\n']

            shader_src += ['#line {}\n'.format(line_index)]
            shader_src += ['\t// End of GroupShared Declaration: ', basename, '\n']
            continue

        if 'PUSH_CONSTANT' in line:
            parsing_pushconstant = tuple(getMacro(line))
            struct_uid = get_uid(parsing_pushconstant[0])

        if '};' in line and parsing_pushconstant:

            shader_src += [line]
            push_constant_decl = parsing_pushconstant
            pushconstant_name = push_constant_decl[0]

            push_constant_location = '[[buffer(UPDATE_FREQ_USER)]]'

            mainArgs += [(struct_uid, ['constant struct ', pushconstant_name, '& ', pushconstant_name, ' ', push_constant_location])]

            parsing_pushconstant = None
            struct_references = []
            struct_uid = None
            last_res_decl = len(shader_src)+1
            continue

        if 'CBUFFER' in line:
            fsl_assert(parsing_cbuffer == None, message='Inconsistent cbuffer declaration: \"' + line + '\"')
            parsing_cbuffer = tuple(getMacro(line))
            cbuffer_decl = parsing_cbuffer
            struct_uid = get_uid(cbuffer_decl[0])

        if '};' in line and parsing_cbuffer:
            shader_src += [line]
            cbuffer_decl = parsing_cbuffer
            cbuffer_name, cbuffer_freq, dxreg = cbuffer_decl[:3]
            is_rootcbv = 'rootcbv' in cbuffer_name

            if cbuffer_freq not in metal_ab_frequencies and not is_rootcbv:
                shader_src += ['#line {}\n'.format(line_index)]
                shader_src += ['\t// Ignored CBuffer Declaration: '+line+'\n']
                continue

            is_embedded = cbuffer_freq in ab_elements
            if not is_embedded:
                location = buffer_location
                if is_rootcbv:
                    location = 'UPDATE_FREQ_USER + 1'
                else:
                    buffer_location += 1
                mainArgs += [(struct_uid, ['constant struct ', cbuffer_name, '& ', cbuffer_name, ' [[buffer({})]]'.format(location)])]
            else:
                ab_elements[cbuffer_freq] += [(struct_uid, ['constant struct ', cbuffer_name, '& ', cbuffer_name])]

            parsing_cbuffer = None
            struct_references = []
            struct_uid = None
            last_res_decl = len(shader_src)+1

            continue

        # consume resources
        if 'RES(' in line:
            resource = tuple(getMacro(line))
            resType, resName, freq, dxreg = resource[:4]
            baseName = getArrayBaseName(resName)
            macro = get_uid(baseName)

            is_embedded = freq in ab_elements and freq != 'UPDATE_FREQ_USER'

            if freq not in metal_ab_frequencies:
                shader_src += ['#line {}\n'.format(line_index)]
                shader_src += ['\t// Ignored Resource Declaration: '+line+'\n']
                continue

            if not is_embedded: # regular resource
                shader_src += ['\n\t// Metal Resource Declaration: ', line, '\n']
                prefix = ''
                postfix = ' '
                ctor_arg = None
                if 'Buffer' in resType:
                    prefix = 'device ' if 'RW' in resType else 'constant ' 
                    postfix = '* '
                    ctor_arg = ['', prefix, resType, '* ', resName]
                else:
                    ctor_arg = ['thread ', resType, '& ', resName]

                if 'Sampler' in resType:
                    binding = ' [[sampler({})]]'.format(sampler_location)
                    sampler_location += 1
                elif 'Tex' in resType or 'Depth' in getMacroName(resType):
                    binding = ' [[texture({})]]'.format(texture_location)
                    texture_location += 1
                elif 'Buffer' in resType:
                    binding = ' [[buffer({})]]'.format(buffer_location)
                    buffer_location += 1
                else:
                    fsl_assert(False, message="Unknown Resource location")

                main_arg = [prefix , resType, postfix, baseName, binding, ' // main arg']
                mainArgs += [(macro, main_arg)]

                global_reference_paths[baseName] = baseName

                if not isArray(resName):
                    global_reference_args[baseName] = 'thread ' + resType + '& ' + resName
                    if 'Buffer' in resType:
                        space = 'constant'
                        if 'RW' in resType:
                            space = 'device'
                        global_reference_args[baseName] = space + ' ' + resType + '* ' + resName
                else:
                    array = resName[resName.find('['):]
                    global_reference_args[baseName] = 'thread ' + resType + '(&' + baseName+') ' + array

                shader_src += ['#define _Get_', baseName, ' ', baseName, '\n']

                shader_src += ['\t// End of Resource Declaration: ', resName, '\n']

            else: # resource is embedded in argbuf

                shader_src += ['\n\t// Metal Embedded Resource Declaration: ', line, '\n']
                argBufType = 'AB_' + freq
                basename = getArrayBaseName(resName)

                if 'Buffer' in resType:
                    space = ' device ' if resType.startswith('RW') else ' constant '
                    ab_element = [space, resType, '* ', resName]
                else:
                    ab_element = [resType, ' ', resName]

                ab_elements[freq] += [(macro, ab_element)]

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
            line = '// [numthreads({}, {}, {})]\n'.format(*elems)

        # convert shader entry to member function
        if '_MAIN(' in line:

            parsing_main = len(shader_src)
            ab_decl = declare_argument_buffers(mainArgs)
            ab_decl_location = last_res_decl if not explicit_res_decl else explicit_res_decl
            shader_src = shader_src[:ab_decl_location] + ab_decl + shader_src[ab_decl_location:]

            if shader.stage == Stages.TESE:
                num_control_points = shader.output_patch_arg[1]
                patch_type = get_mtl_patch_type(tessellation_layout)
                shader_src += ['[[patch(', patch_type, ', ', num_control_points, ')]]\n']

            # insert VSMain code
            if shader.stage == Stages.TESC:
                shader_src += vs_main
                shader_src += ['//[numthreads(32, 1, 1)]\n']

            mtl_returntype = 'void'
            if shader.returnType and shader.stage != Stages.TESC:
                mtl_returntype = getMacroName(shader.returnType)

            shader_src += [msl_target, ' ', mtl_returntype, ' stageMain(\n']
            
            prefix = '\t'
            if shader.stage == Stages.TESE:
                if shader.output_patch_arg:
                    dtype, _, dname = shader.output_patch_arg
                    shader_src += [prefix+'patch_control_point<'+dtype+'> '+dname+'\n']
                    prefix = '\t,'
            elif shader.stage == Stages.TESC:
                shader_src += [prefix+'uint threadId [[thread_position_in_grid]]\n']
                prefix = '\t,'
                if shader.input_patch_arg:
                    dtype, _, dname = shader.input_patch_arg
            else:
                for dtype, var in shader.struct_args:
                    shader_src += [ prefix+dtype+' '+var+'[[stage_in]]\n']
                    prefix = '\t,'
            for dtype, dvar in shader.flat_args:
                if 'SV_OUTPUTCONTROLPOINTID' in dtype.upper(): continue
                innertype = getMacro(dtype)
                semtype = getMacroName(dtype)
                shader_src += [prefix+innertype+' '+dvar+' '+semtype.upper()+'\n']
                prefix = '\t,'
                
            if shader.enable_waveops:
                shader_src += [prefix, 'uint simd_lane_id [[thread_index_in_simdgroup]]\n']
                prefix = '\t,'

            for macro, arg in mainArgs:
                shader_src += [prefix, *arg, '\n']
                prefix = '\t,'

            shader_src += [')\n']
            shader_src += ['#line {}\n'.format(line_index)]
            continue

        if 'INIT_MAIN' in line:

            for macro, entry_declaration in entry_declarations:
                shader_src += ['\t', *entry_declaration, '\n']

            if shader.stage == Stages.TESC:
                # fetch VS data, call VS main
                dtype, dim, dname = shader.input_patch_arg
                shader_src += [
                    '\n\tif (threadId > drawInfo->vertexCount) return;\n',
                    '\n\t// call VS main\n',
                    '\tconst '+dtype+' '+dname+'['+dim+'] = { VSMain(vertexInput[threadId]) };\n',
                ]
                for dtype, dvar in shader.flat_args:
                    if 'SV_OUTPUTCONTROLPOINTID' in dtype.upper():
                        shader_src += ['\t'+getMacro(dtype)+' '+getMacro(dvar)+' = 0u;\n'] # TODO: extend this for >1 CPs

            
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            continue

        if re.search('(^|\s+)RETURN', line):
            ws = get_whitespace(line)
            return_statement = [ ws+'{\n' ]

            # void entry, return nothing
            if not shader.returnType:
                return_statement += [ws+'\treturn;\n']

            else:
                return_value = getMacro(line)
                #  for tessellation, write tesc results to output buffer and call PCF
                if shader.stage == Stages.TESC:
                    return_statement += [ws+'\thullOutputBuffer[threadId] = '+return_value+';\n']
                    return_statement += [ws+'\ttessellationFactorBuffer[threadId] = '+shader.pcf+'(\n']
                    prefix = ws+'\t\t'
                    for dtype, dvar in shader.pcf_arguments:
                        if 'INPUT_PATCH' in dtype:
                            return_statement += [prefix+shader.input_patch_arg[-1]+'\n']
                            prefix = ws+'\t\t,'
                        if 'SV_PRIMITIVEID' in dtype.upper():
                            return_statement += [prefix+'0u'+'\n'] # TODO: extend this for >1 CPs
                            prefix = ws+'\t\t,'
                    return_statement += [ws+'\t);\n']

                # entry declared with returntype, return var
                else:
                    return_statement += [ws+'\treturn '+return_value+';\n']

            return_statement += [ ws+'}\n' ]
            shader_src += return_statement
            shader_src += ['#line {}\n'.format(line_index), '//'+line]
            continue

        # tessellation PCF
        if shader.pcf and ' '+shader.pcf in line:
            for dtype, dvar in shader.pcf_arguments:
                if 'INPUT_PATCH' in dtype: continue
                innertype = getMacro(dtype)
                line = line.replace(dtype, innertype)
            # since we modify the pcf signature, need to update the global_fn_table accordingly
            _, (line_no, _) = global_fn_table[shader.pcf]
            global_fn_table[shader.pcf] = (line, (line_no, line.find(shader.pcf)+len(shader.pcf)+1))
        if shader.pcf and 'PCF_INIT' in line:
            line = get_whitespace(line)+'//'+line.strip()+'\n'
        if shader.pcf and 'PCF_RETURN' in line:
            ws = get_whitespace(line)
            return_value = getMacro(line)
            line = ws+'return '+return_value+';\n'

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
                # print('modify signature:', fn, shader_src[i], line)
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
                # print('modify call:', shader_src[i], line)
                shader_src[i] = line

    open(dst, 'w').writelines(shader_src)
    return 0, []