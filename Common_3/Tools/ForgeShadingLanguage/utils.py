"""
"""
from enum import Enum
import datetime, os, sys
from shutil import copyfile
import subprocess, hashlib
from os.path import dirname, join
import tempfile

class Platforms(Enum):
    DIRECT3D11 = 0
    DIRECT3D12 = 1
    VULKAN =     2
    MACOS =      3
    IOS   =      4
    ORBIS =      5
    PROSPERO =   6
    XBOX =       7
    SCARLETT =   8
    GLES =       9
    SWITCH =    10
    ANDROID =   11
    QUEST =     12

platform_langs = {
    Platforms.DIRECT3D11:  'DIRECT3D11',
    Platforms.DIRECT3D12:  'DIRECT3D12',
    Platforms.VULKAN:      'VULKAN',
    Platforms.MACOS:       'METAL',
    Platforms.IOS:         'METAL',
    Platforms.ORBIS:       'ORBIS',
    Platforms.PROSPERO:    'PROSPERO',
    Platforms.XBOX :       'DIRECT3D12',
    Platforms.SCARLETT :   'DIRECT3D12',
    Platforms.GLES :       'GLES',
    Platforms.SWITCH :     'VULKAN',
    Platforms.ANDROID :    'VULKAN',
    Platforms.QUEST :      'VULKAN',
}

class Stages(Enum):
    VERT = 0
    FRAG = 1
    COMP = 2
    GEOM = 3
    TESC = 4
    TESE = 5
    NONE = 6

class ShaderTarget(Enum):
    ST_5_0 = 0,
    ST_5_1 = 1,
    ST_6_0 = 2,
    ST_6_1 = 3,
    ST_6_2 = 4,
    ST_6_3 = 5,
    ST_6_4 = 6,
    ST_END = 7,
    MSL_2_2 = 10,
    MSL_2_3 = 11,
    MSL_2_4 = 12,
    MSL_3_0 = 13,
    MSL_END = 14,

class StageFlags(Enum):
    NONE = 0,
    VR_MULTIVIEW = 1,
    ORBIS_TESC = 2,
    ORBIS_GEOM = 4

class DescriptorSets(Enum):
    UPDATE_FREQ_NONE = 0
    UPDATE_FREQ_PER_FRAME = 1
    UPDATE_FREQ_PER_BATCH = 2
    UPDATE_FREQ_PER_DRAW = 3
    UPDATE_FREQ_USER = 4
    space4 = 5
    space5 = 6
    space6 = 7

class ShaderBinary:
    def __init__(self):
        self.stage = Stages.NONE
        if os.name == "posix":
            self.target = ShaderTarget.MSL_2_2
        else:
            self.target = ShaderTarget.ST_5_1
        self.flags = []
        self.preprocessed_srcs = {}
        self.filename = None
        self.fsl_filepath = None
        self.derivatives = {}

def fsl_platform_assert(platform: Platforms, condition, filepath, message ):
    if condition: return

    if platform in [Platforms.ANDROID, Platforms.VULKAN]:
        for error in message.split('ERROR: '):
            fne = error.find(':', 2)
            if fne > 0:
                src = error[:fne]
                line, msg = error[fne+1:].split(':', 1)
                message = '{}({}): ERROR : {}\n'.format(src, line, msg )
                break

    if Platforms.GLES == platform:
        errors = message.strip().splitlines()[:-1]
        src = errors.pop(0)
        for error in errors:
            sl, desc = error.split(' ', maxsplit=2)[1:]
            source_index, line = sl.split(':')[:2]
            error_src = 'UNKNOWN'
            with open(src) as gen_src:
                for l in gen_src.readlines():
                    if l.startswith('#line'):
                        index, filepath = l.split()[2:]
                        if source_index == index:
                            error_src = filepath[3:].strip('"')
                            break
            message = f'{error_src}({line}): ERROR: {desc}'
            break

    print(message)
    sys.exit(1)

def fsl_assert(condition, filename=None, _line_no=None, message=''):
    if not condition:
        print(message)
        sys.exit(1)

def getHeader(fsl_source):
    header = [
        '//'+'-'*38+'\n',
        '// Generated from Forge Shading Language\n',
        # '// '+str(datetime.datetime.now())+'\n',
        # '// \"'+fsl_source+'\"\n'
        '//'+'-'*38+'\n',
        '\n'
    ]
    return header

def get_stage_from_entry(line):

    stages = {
        Stages.VERT: 'VS_MAIN',
        Stages.FRAG: 'PS_MAIN',
        Stages.COMP: 'CS_MAIN',
        # Stages.GEOM: 'GS_MAIN',
        Stages.TESC: 'TC_MAIN',
        Stages.TESE: 'TE_MAIN',
    }

    for stage, entry_name in stages.items():
        loc = line.find(entry_name)
        if loc > -1: return stage, loc

    return None, None

def genFnCall(params, prefix='', delimiter=', ', noBrackets=False):
    res = [] if noBrackets else ['(']
    for param in params:
        res += [prefix, param]
        res += [delimiter]
    if params:
        res.pop()
    res += [] if noBrackets else [')']
    return res

def get_unique_declaration_key(lineno, line):
    declaration = getMacro(line)
    if type(declaration) == list:
        return (lineno, *declaration)
    return (lineno, declaration)

def get_declarations(name, src):
    structs = {}
    struct = None
    for lineno, line in enumerate(src):
        if line.isspace() or line.strip().startswith('//') or line.strip().startswith('#'):
            continue
        if struct:
            if 'DATA('in line:
                fsl_assert(';' in line, message='Data decl, needs to end with semicolon: ' + line)
                if struct not in structs: structs[struct] = []
                element_decl = tuple(getMacro(line))
                fsl_assert(len(element_decl) == 3, message='Invalid Struct Element declaration: ' + line)
                structs[struct] += [element_decl]
            elif '};' in line:
                fsl_assert(structs[struct], message='Empty struct: ' + str(struct))
                struct = None
            continue
        elif name in line:
            # struct = get_unique_declaration_key(lineno, line)
            # print(struct)
            # sys.exit(1)
            arg = getMacro(line)
            struct = tuple(arg) if type(arg) is list else arg
            # struct = arg
            structs[struct] = []
            continue
    return structs

def get_entry_signature(filename, src):
    entry = '_MAIN('
    pcf = None
    vs_reference = None
    enable_waveops = False
    for line_no, line in enumerate(src):
        if line.isspace() or line.strip().startswith('//'):
            continue

        if 'PATCH_CONSTANT_FUNC' in line:
            pcf = getMacro(line.strip()).strip('"')

        if 'TESS_VS_SHADER' in line:
            vs_reference = getMacro(line.strip()).strip('"')

        if 'ENABLE_WAVEOPS' in line:
            enable_waveops = True

        loc = line.find(entry)
        if loc > -1:
            ''' get return type '''
            target, target_loc = get_stage_from_entry(line)

            fsl_assert(target, filename, line_no, message='Cannot determine Target from \''+line+'\'')

            ret = line[:target_loc].strip()
            arguments = getMacro(line[loc:])
            arguments = arguments if type(arguments) == list else [arguments]
            if len(arguments) == 1 and arguments[0] == '':
                inputs = []
            else:
                inputs = [param.strip() for param in arguments]
            fsl_assert(ret, message='Could not determine entry point return type: ' + line)
            return target, ret, inputs, pcf, vs_reference, enable_waveops
    
    fsl_assert(False, filename, message='Could not determine shader entry point')

def getMacroArg(line):
    s, e = line.find('('), line.rfind(')')
    if s > -1 and e > -1:
        return line[s+1:e]
    return line

def getMacro(line):
    e = line.rfind(')')
    if e < 0: return line
    args = line[line.find('(')+1:e]
    result = []
    n, p = False, 0
    for i, c in enumerate(args):
        if c == '(':
            n += 1
        if c == ')':
            n -= 1
        if c == ',' and n == 0:
            result += [args[p:i].strip()]
            p = i+1
        if i == len(args)-1:
            result += [args[p:i+1].strip()]
    args = result
    if len(args) == 1:
        return args[0]
    return args

def getMacroName(line):
    br = line.find('(')
    if br > -1:
        return line[:br].strip()
    return line.strip()

def isMacro(line):
    return line.find('(') > -1 and line.rfind(')') > -1

def isArray(name):
    return '[' and ']' in name

def resolveName(defines, name):
    if isArray(name):
        arrLen = getArrayLen(defines, name)
        return name[:name.find('[')+1] +str(arrLen) + name[name.find(']'):]
    else:
        return name

def getArrayBaseName(name):
    i = name.find('[')
    return name if i < 0 else name[:i]

def getArrayLenFlat(n):
    return n[n.find('[')+1:n.find(']')]
    
def getArrayLen(defines, n):
    arrlen = n[n.find('[')+1:n.find(']')]
    if arrlen.strip().isnumeric():
        return int(arrlen)
    elif arrlen not in defines or not defines[arrlen].strip().isnumeric():
        print(defines)
        fsl_assert(False, message='Could not deduce array size for ' + n)
    return int(defines[arrlen])

def get_resources(src):
    resources = []
    for line in src:
        if line.isspace():
            continue
        if line.strip().startswith('//'):
            continue
        if 'RES(' in line:
            resources += [tuple(getMacro(line))]
            fsl_assert(');' in line, message='Resource declaration requires ;-termination:\n\"{}\"'.format(line.strip()))
    return resources

class Shader:
    lines = []
    stage = Stages.NONE
    struct_args = []
    flat_args = []
    returnType = None
    defines = {}
    cBuffers = {}
    # cBFreq = {}
    structs = {}
    pushConstant = {}
    resources = []

    # tesselation
    pcf = None
    pcf_returnType = None
    pcf_arguments = None
    input_patch_arg = None
    output_patch_arg = None
    vs_reference = None

    enable_waveops = False

    def getArrayLen(self, name):
        return getArrayLen(self.defines, name)

def getDefines(lines):
    defines = {}
    for line in lines:
        if line.isspace() or line.strip().startswith('//'):
            continue
        if '#define' in line:
            elems = line.strip().split(' ')
            defines[elems[1]] = ' '.join(elems[2:])
    return defines


def collect_resources(lines: list, ws: set):
    # assert os.path.exists(fsl), 'Could not find \"{}\"'.format(fsl)
    # lines = open(fsl).readlines()
    cb = get_declarations('CBUFFER(', lines)
    pc = get_declarations('PUSH_CONSTANT(', lines)
    st = get_declarations('STRUCT(', lines)
    rs = get_resources(lines)
    df = getDefines(lines)
    # cbFreq = {}

    # print(cb.keys())
    # sys.exit(1)

    # dirname = os.path.dirname(fsl)
    for line in lines:
        if line.isspace() or line.strip().startswith('//'):
            continue
        if 'CBUFFER(' in line:
            elems = getMacro(line)
            # cbFreq[elems[0]] = elems
    # return cb, cbFreq, pc, st, rs, df
    return cb, pc, st, rs, df


def getShader(platform: Platforms, fsl_path: str, fsl: list, dst=None, line_directives=True) -> Shader:
    
    # collect shader declarations
    incSet = set()
    lines = fsl

    # print(dst)
    # open(dst, 'w').writelines(lines)

    # sys.exit(1)

    cbuffers, pushConstant, structs, resources, defines = collect_resources(lines, incSet)
    # rpc = [ res for res in resources if 'PUSH_CONSTANT' in res[0] ]
    # if rpc:
    #     rpc = rpc[0]
    #     resources.remove(rpc)
    #     pushConstant = { getMacro(rpc[0]): rpc[1] }
        # pushConstant = {rpc[1]: structs[getMacro(rpc[0])]}

    # copy included files to destination
    # if dst:
    #     dirname = os.path.dirname(fsl)
    #     for incFile in incSet:
    #         relPath = os.path.relpath(incFile, dirname)
    #         absPath = os.path.join(os.path.dirname(dst), relPath)
    #         if True or not os.path.exists(absPath):
    #             print('copying {} to {}'. format(incFile, absPath))
    #             copyfile(incFile, absPath)
    
    # lines = open(fsl).readlines()

    stage, entry_ret, entry_args, pcf, vs_reference, enable_waveops = get_entry_signature(fsl_path, lines)
    # assert len(pushConstant) < 2, 'Only single PUSH_CONSTANT decl per shader'

    if stage != Stages.COMP:
        if isMacro(entry_ret) and getMacroName(entry_ret) not in structs:
            mName = getMacroName(entry_ret)
            if 'SV_Depth' not in mName and mName != 'float4' and mName != 'float3' and mName != 'float2' and mName != 'float':
                print('Shader Entry return type must be a STRUCT, but is \'{}\''.format(entry_ret))
                print('Known STRUCTs:')
                for struct in structs.items():
                    print(struct)
                assert False

    ''' check entry signature '''
    # returnStruct = None
    struct_args = []
    flat_args = []
    input_patch_arg = None
    output_patch_arg = None
    for i, arg in enumerate(entry_args):

        # print(arg)

        # all entry arguments should have macro form
        # fsl_assert(isMacro(arg), filename=fsl, message='Invalid entry argument: \''+arg+'\'')

        # arg_dtype, arg_var = arg.rstrip(')').split('(', 1)
        # print(arg)
        # arg_dtype, arg_var = 
        # print(arg_dtype,':', arg_var)
        # sys.exit(0)

        
        # for tesselation, treat INPUT_PATCH and OUTPUT_PATCH
        if stage == Stages.TESC and 'INPUT_PATCH' in arg:
            fsl_assert(input_patch_arg is None, fsl_path, message=': More than one INPUT_PATCH')
            dtype, dvar = arg.rsplit(maxsplit=1)
            arg_dtype, np = getMacro(dtype)
            # print(arg_dtype, np, dvar)
            # sys.exit(0)
            input_patch_arg = (arg_dtype, np, dvar)
            struct_args += [(arg_dtype, dvar)]
            continue
        if stage == Stages.TESE and 'OUTPUT_PATCH' in arg:
            fsl_assert(output_patch_arg is None, fsl_path, message=': More than one OUTPUT_PATCH')
            dtype, dvar = arg.rsplit(maxsplit=1)
            arg_dtype, np = getMacro(dtype)
            output_patch_arg = (arg_dtype, np, dvar)
            struct_args += [(arg_dtype, dvar)]
            continue

        arg_elements = arg.split()
        fsl_assert(len(arg_elements) == 2, fsl_path, message=': error FSL: Invalid entry argument: \''+arg+'\'')
        arg_dtype, arg_var = arg_elements

        flat_arg_dtypes = [
            'SV_VERTEXID',
            'SV_INSTANCEID',
            'SV_GROUPID',
            'SV_DISPATCHTHREADID',
            'SV_GROUPTHREADID',
            'SV_GROUPINDEX',
            'SV_SAMPLEINDEX',
            'SV_PRIMITIVEID',
            'SV_POSITION',
            'SV_OUTPUTCONTROLPOINTID',
            'SV_DOMAINLOCATION',
            'SV_SHADINGRATE',
            'SV_ISFRONTFACE',
        ]
        is_builtin = False
        for flat_arg_dtype in flat_arg_dtypes:
            if arg.upper().startswith(flat_arg_dtype):
                flat_args += [(arg_dtype, arg_var)]
            # if flat_arg_dtype in arg_dtype:
                is_builtin = True
            #     break

        if is_builtin:
            continue
        #     flat_args += [(arg_dtype, arg_var)]
        # else:



        # print(arg)

        fsl_assert(arg_dtype in structs, fsl_path, message=': error FSL: Unknow entry argument: \''+arg+'\'')
        struct_args += [(arg_dtype, arg_var)]

        continue

    shader = Shader()
    shader.lines = lines
    shader.stage = stage
    shader.flat_args = flat_args
    shader.struct_args = struct_args

    shader.returnType = entry_ret if entry_ret != 'void' else None
    shader.defines = defines
    shader.cBuffers = cbuffers
    shader.structs = structs
    shader.pushConstant = pushConstant
    shader.resources = resources

    shader.enable_waveops = enable_waveops

    # tesselation
    if pcf:
        shader.pcf = pcf
        # retrieve pcf signature
        for line in lines:
            loc = line.find(shader.pcf)
            if loc > -1 and not shader.pcf_returnType:
                shader.pcf_returnType = line[:loc].strip()
                pcf_arguments = getMacro(line[loc:])
                shader.pcf_arguments = []
                for arg in pcf_arguments:
                    shader.pcf_arguments += [arg.rsplit(maxsplit=1)]
                # print('PCF:', shader.pcf_returnType, shader.pcf_arguments)

    shader.input_patch_arg = input_patch_arg
    shader.output_patch_arg = output_patch_arg

    # for Prospero we handle it in collect_shader_decl
    if shader.stage == Stages.TESC and platform != Platforms.PROSPERO:
        fsl_assert(vs_reference, message="TESC need a vs reference: TESS_VS_SHADER(\"shader.vert.fsl\")")
        abspath_vs_reference = os.path.join(os.path.dirname(fsl_path), vs_reference)
        fsl_assert(os.path.exists(abspath_vs_reference), message="Could not open TESC vs reference {}".format(abspath_vs_reference))
        shader.vs_reference = abspath_vs_reference

    return shader

def max_timestamp(filepath):
    return max_timestamp_recursive(filepath, [])

def max_timestamp_recursive(filepath, _files):
    if filepath in _files:
        return 0
    if not os.path.exists(filepath):
        return 0
    _files += [filepath]
    dirname = os.path.dirname(filepath)
    lines = open(filepath).readlines()
    mt = os.path.getmtime(filepath)

    for line in lines:
        if line.lstrip().startswith('#include'):
            include_filename = line.lstrip('#include').strip().strip('\"').lstrip('<').rstrip('>')
            include_filepath = os.path.join(dirname, include_filename).replace('\\', '/')
            mt = max(mt, max_timestamp_recursive(include_filepath, _files))

    return mt

def fixpath(rawpath):
    return rawpath.replace(os.sep, '/')

def needs_regen(args, dependency_filepath, platforms, regen, dependencies):
    exists, getmtime = os.path.exists, os.path.getmtime
    if not exists(dependency_filepath):
        regen.add('deps')
        return True # no deps file
    src_timestamp = getmtime(args.fsl_input)
    if getmtime(dependency_filepath) < src_timestamp:
        regen.add('deps')
        return True # out-of-date deps
    deps = open(dependency_filepath, 'r').read().split('\n\n')[1:]
    for bdep in deps:

        files = bdep.splitlines()
        if len(files) == 0:
            continue

        generated_filename = files.pop(0)
        dependencies[generated_filename] = [generated_filename] + files + ['']
        files = [ f[1:-1] for f in files ]

        for platform in platforms:
            platform_filename = generated_filename
            if platform in [Platforms.MACOS, Platforms.IOS]:
                platform_filename += '.metal'
            generated_filepath = os.path.join(args.destination, platform.name, platform_filename)
            generated_filepath = os.path.normpath(generated_filepath).replace(os.sep, '/')
            compiled_filepath = os.path.join(args.binaryDestination, platform.name, platform_filename)
            if not os.path.exists(generated_filepath):
                regen.add(platform_filename)
                continue
            if not os.path.exists(compiled_filepath):
                regen.add(platform_filename)
                continue
            dst_timestamp = getmtime(generated_filepath)
            if dst_timestamp < src_timestamp:
                regen.add(platform_filename)
                continue
            deps_timestamp = max([os.path.getmtime(filepath.strip()) for filepath in files])
            if dst_timestamp < deps_timestamp:
                regen.add(platform_filename)
                continue
            bin_timestamp = getmtime(compiled_filepath)
            if bin_timestamp < deps_timestamp:
                regen.add(platform_filename)
                continue
    return len(regen)

def collect_shader_decl(args, filepath: str, platforms, regen, dependencies, binary_declarations):
    pp = []
    if os.name == 'nt':
        pp += [join(dirname(dirname(dirname(__file__))), 'Utilities', 'ThirdParty', 'OpenSource', 'mcpp', 'bin', 'mcpp.exe')]
    else:
        pp += ['cc', '-E', '-']

    with open(filepath, 'r') as f:
        source = f.readlines()

    filedir = dirname(filepath)

    binary_identifiers = set()

    meta_source = []

    include_dirs = [f'-I{filedir}']
    if args.includes:
        include_dirs += [ f'-I{d}' for d in args.includes ]

    current_stage = Stages.NONE
    vs_reference_found = False

    for i, line in enumerate(source):
        line = line.strip()
        stage = Stages.NONE
        if line.startswith('#frag'):
            stage = Stages.FRAG
        if line.startswith('#vert'):
            stage = Stages.VERT
        if line.startswith('#comp'):
            stage = Stages.COMP
        if line.startswith('#geom'):
            stage = Stages.GEOM
        if line.startswith('#tese'):
            stage = Stages.TESE
        if line.startswith('#tesc'):
            stage = Stages.TESC
        if stage is not Stages.NONE:

            binary = ShaderBinary()
            binary.stage = stage
            binary.fsl_filepath = filepath

            fsl_assert(current_stage == Stages.NONE, filepath, message=' error: missing #end for the previous stage')
            current_stage = stage
            decl = line.strip().split()
            if len(decl) < 2:
                print('ERROR: Invalid binary declaration: ', line)
                sys.exit(1)
            binary.filename, _ = decl.pop(), decl.pop(0)
            for flag in decl:
                # check for MSL or ST shader targets depending on platform
                if Platforms.MACOS in platforms or Platforms.IOS in platforms:
                    # start looking from ST_END which is just before the msl targets
                    if flag in ShaderTarget._member_names_[int(ShaderTarget.ST_END.value[0]):]:
                        binary.target = ShaderTarget[flag]
                else:
                    # finish looking once we reach ST_END
                    if flag in ShaderTarget._member_names_[:int(ShaderTarget.ST_END.value[0])]:
                        binary.target = ShaderTarget[flag]
                
                if flag in StageFlags._member_names_:
                    binary.flags += [StageFlags[flag]]
                    
            binary_macro = abs(hash(binary))
            if binary_macro not in binary_identifiers:
                binary_declarations += [ binary ]
                meta_source += [ f'#if D_{binary_macro}\n'.encode('utf-8') ]
                binary_identifiers.add(binary_macro)
            else:
                print('WARN: duplicate shader, only compiling 1st, line:', i, ':', binary.filename)
                meta_source += [ b'#if 0\n' ]
            continue

        if line == '#end':
            fsl_assert(current_stage != Stages.NONE, filepath, message=' error: #end found without an opening stage')
            fsl_assert(current_stage != Stages.TESC or vs_reference_found, filepath, message='#tesc need a vs reference in this file: TESS_VS_SHADER(\"shader.vert.fsl\")')
            current_stage = Stages.NONE
            vs_reference_found = False
            meta_source += [ b'#endif\n' ]
            continue
        
        if 'TESS_VS_SHADER(' in line and current_stage == Stages.TESC:
            vs_reference_found = True
            vs_filename = getMacro(line).strip('"')
            # For Prospero we need to change the macro for an #include, if we don't do it at this preprocessor stage
            # we'll get duplicate definitions latter when compiling the binary.
            meta_source += [
                b'#ifdef PROSPERO\n',
                b'#undef VS_MAIN\n',
                b'#define VS_MAIN vs_main\n',
                b'#include "' + vs_filename.encode('utf-8') + b'"\n',
                b'#else\n',
                source[i].encode('utf-8'), # For other platforms we keep the macro as is
                b'#endif\n'
                ]
            continue

        meta_source += [ source[i].encode('utf-8') ]
    
    source = b''.join(meta_source)
    fsl_dependencies = {}
    for binary in binary_declarations:
        if args.incremental and regen and (not binary.filename in regen and not binary.filename+'.metal' in regen):
            if binary.filename in dependencies:
                fsl_dependencies[binary.filename] = dependencies[binary.filename]
            continue

        for platform in platforms:
            cmd = pp + [
                *include_dirs,
                f'-DD_{abs(hash(binary))}',
                f'-D{platform_langs[platform]}',
                f'-DTARGET_{platform.name}',
                f'-D{ "_DEBUG" if args.debug else "NDEBUG" }'
            ]

            binary.derivatives[platform] = [[]]
            
            if Platforms.QUEST == platform and StageFlags.VR_MULTIVIEW in binary.flags:
                cmd += ['-DVR_MULTIVIEW_ENABLED']

            deps_filepath = os.path.join( tempfile.gettempdir(), next(tempfile._get_candidate_names()) )
            if args.incremental:
                if os.name == 'nt':
                    cmd += ['-MD', deps_filepath]
                else:
                    cmd += ['-MMD', '-MF', deps_filepath]

            cp = subprocess.run(cmd, input=source, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

            if 0 != cp.returncode:
                _err = cp.stdout.decode()
                if len(_err) > 19:
                    if os.name == 'nt':
                        error_lines = f"{cp.stderr.decode().replace('<stdin>', filepath)}".splitlines()
                        for i, line in enumerate(error_lines):
                            if ': error: ' in line:
                                err, msg = line.split(': error: ')
                                src, line = err.rsplit(':', maxsplit=1)
                                error_lines[i] = f'{src}({line}): ERROR : {msg}'
                        error_message = '\n'.join(error_lines)
                    else:
                        error_message = f"{cp.stderr.decode().replace('<stdin>', filepath)}"
                    print(error_message)
                    sys.exit(cp.returncode)
            
            if args.incremental:
                if not os.path.exists(deps_filepath):
                    print('Deps file couldnt be found')
                    sys.exit(1)
                md_raw = [ l.rstrip('\\\n').rstrip(': ') for l in open(deps_filepath).readlines() if len(l) > 1 ]
                if len(md_raw) > 1:
                    md = [binary.filename] + [ f'\"{d.strip()}\"' for d in md_raw[1:] ] + ['']
                    fsl_dependencies[binary.filename] = md
                os.remove(deps_filepath)

            shaderSource = cp.stdout.replace(b'"<stdin>"', f'"{filepath}"'.encode('utf-8'))

            # glslangValidator doesn't seem to understand these empty directives that cc preprocessor adds (empty directive, just a hashtag)
            # We replace it with #line that it's understood
            if sys.platform == 'linux':
                correctLines = []
                for line in shaderSource.split(b'\n'):
                    if line.count(b'# ') == 0: correctLines += [line]; continue

                    line = line.replace(b'# ', b'#line ')
                    last_quote_index = line.rfind(b'"')
                    # some lines have numbers after the string that confuse glslangValidator, we remove them
                    if last_quote_index > 0 and last_quote_index + 1 < len(line):
                        to_remove = len(line) - last_quote_index - 1
                        line = line[:-to_remove]
                    correctLines += [line]
                shaderSource = b'\n'.join(correctLines)
            
            if shaderSource.find(b'_MAIN(') > -1:
                binary.preprocessed_srcs[platform] = shaderSource.decode().splitlines(keepends=True)

    return binary_declarations, fsl_dependencies

def preprocessed_from_file(filepath, line_directives, files_seen=None):
    if files_seen is None: files_seen = []
    if filepath in files_seen: return []
    filepath = fixpath(filepath)
    files_seen += [filepath]
    
    dirname = os.path.dirname(filepath)
    lines = open(filepath).readlines()
    result = []

    working_directory = fixpath(os.getcwd()) + '/'

    line_index = 1
    for line in lines:
        uc_line = line[:line.find('//')]
        if '#include' in uc_line:
            include_filename = uc_line.lstrip('#include').strip().strip('\"').lstrip('<').rstrip('>')
            include_filepath = fixpath(os.path.join(dirname, include_filename))
            if line_directives:
                result += ['#line 1 \"' + working_directory + include_filepath + '\"\n',
                    *preprocessed_from_file(include_filepath, line_directives, files_seen), '\n',
                    '#line {} \"'.format(line_index+1) + working_directory + filepath + '\"\n'
                ]
            else:
                result += [*preprocessed_from_file(include_filepath, line_directives, files_seen), '\n']
        else:
            result += [line]
            if line_directives and ('#else' in uc_line or '#elif' in uc_line or '#endif' in uc_line):
                result += ['\n', '#line {} \"'.format(line_index+1) + working_directory + filepath + '\"\n']
        line_index += 1

    return result

def dictAppendList(d : dict, key, val):
    if key not in d:
        d[key] = [val] if val else []
    else:
        d[key] += [val] if val else []

def isBaseType(dtype):
    types = [
        'void',
        'int',
        'uint',
        'atomic_uint',
        'float',
        'float2',
        'float3',
        'float4',
        # TODO: properly handle these in metal.py
        'ByteBuffer',
        'RWByteBuffer',
        'row_major(float4x4)',
        'float4x4',
    ]
    if dtype in types:
        return True
    return False

def is_input_struct(struct: str, shader):
    for dtype, _ in shader.struct_args:
        if struct == dtype:
            return True
    return False

def get_input_struct_var(struct: str, shader):
    for dtype, var in shader.struct_args:
        if struct == dtype:
            return var
    return None

def get_whitespace(line):
    return line[:len(line) - len(line.lstrip())]

def get_array_dims(array_name):
    return array_name.count('[')
def get_array_decl(array_name):
    l0 = array_name.find('[')
    if l0 > -1:
        return array_name[l0:]
    return ''

def is_groupshared_decl(line):
    return line.strip().startswith('GroupShared') and ';' in line

def visibility_from_stage(stage):
    masks = {
        Stages.VERT: 'SHADER_VIS_VS',
        Stages.FRAG: 'SHADER_VIS_PS',
        Stages.COMP: 'SHADER_VIS_CS',
    }
    return masks[stage]
    # print(stage)
    # sys.exit(0)
    # return ''

# returns a table of (fn_call, fn_decl (raw line), (last param + loc))
def get_fn_table(lines):
    import re
    table = {}
    scope_counter = 0
    in_scope = False
    for i, line in enumerate(lines):
        if line.strip().startswith('//'): continue
        for _ in re.finditer('{', line):
            scope_counter += 1
            if scope_counter == 1:
                counter = 0
                function = None
                insert = None
                for j, _line in enumerate(reversed(lines[:i])):
                    if 'struct ' in _line or '=' in _line:
                        break
                    for k, c in enumerate(reversed(_line)):
                        if c == ')':
                            counter-=1
                        if c == '(':
                            counter+=1
                            if counter == 0:
                                function = _line
                                insert = i-j-1, len(_line)-k
                                break
                    if function: break
                def skip_keyword(fn):
                    fn_mask = [
                        'STRUCT(',
                        'PUSH_CONSTANT(',
                        # 'PatchTess(',
                        '_MAIN(',
                    ]
                    for m in fn_mask:
                        if not fn or m in fn: return True
                    return False
                if not skip_keyword(function): # (not 'FSL_' in function or 'PatchTess(' in _line): # skip cbuffer, push_constant and entry fn
                    function = _line.split('(')[0].split()[-1]
                    # print('fn: ', function)
                    if 'PatchTess(' in _line:
                        function = _line.split('(')[1].split()[-1]
                    table[function] = (lines[insert[0]], insert)
        for _ in re.finditer('}', line):
            scope_counter -= 1

    # print('Function Table:')
    # for k, v in table.items():
    #     print(k, v)
    # sys.exit(0)
    return table
