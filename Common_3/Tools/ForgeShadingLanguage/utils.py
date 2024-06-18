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

"""
"""
from enum import Enum, Flag
import datetime, os, sys
from shutil import copyfile
import subprocess, hashlib
from os.path import dirname, join
import tempfile

class Platforms(Enum):
    DIRECT3D11 =      0
    DIRECT3D12 =      1
    VULKAN =          2
    MACOS =           3
    IOS   =           4
    ORBIS =           5
    PROSPERO =        6
    XBOX =            7
    SCARLETT =        8
    ANDROID_GLES =    9
    SWITCH =         10
    ANDROID_VULKAN = 11
    QUEST =          12

platform_langs = {
    Platforms.DIRECT3D11:      'DIRECT3D11',
    Platforms.DIRECT3D12:      'DIRECT3D12',
    Platforms.VULKAN:          'VULKAN',
    Platforms.MACOS:           'METAL',
    Platforms.IOS:             'METAL',
    Platforms.ORBIS:           'ORBIS',
    Platforms.PROSPERO:        'PROSPERO',
    Platforms.XBOX :           'DIRECT3D12',
    Platforms.SCARLETT :       'DIRECT3D12',
    Platforms.SWITCH :         'VULKAN',
    Platforms.ANDROID_VULKAN : 'VULKAN',
    Platforms.ANDROID_GLES :   'GLES',
    Platforms.QUEST :          'VULKAN',
}

def get_target(platform: Platforms):
    if platform in [Platforms.ANDROID_VULKAN, Platforms.ANDROID_GLES]:
        return 'ANDROID'
    return platform.name

class Stages(Enum):
    VERT = 0
    FRAG = 1
    COMP = 2
    GEOM = 3
    NONE = 4

class Features(Enum):
    PRIM_ID = 0,
    RAYTRACING = 1,
    VRS = 2,
    MULTIVIEW = 3,
    NO_AB = 5,
    ICB = 6, # indirect command
    VDP, = 7, # Vertex Draw Parameters
    INVARIANT = 8,
    ATOMICS_64 = 9,
    DYNAMIC_RESOURCES = 10,

feature_mask = { f: [] for f in Features }
feature_mask[Features.MULTIVIEW] = [Platforms.QUEST]

# Same enum WaveopsFlags in C++, make sure values match in case we want to store these flags in the compiled shader file to read in runtime
class WaveopsFlags(Flag):
	WAVE_OPS_NONE = 0x0
	WAVE_OPS_BASIC_BIT = 0x00000001
	WAVE_OPS_VOTE_BIT = 0x00000002
	WAVE_OPS_ARITHMETIC_BIT = 0x00000004
	WAVE_OPS_BALLOT_BIT = 0x00000008
	WAVE_OPS_SHUFFLE_BIT = 0x00000010
	WAVE_OPS_SHUFFLE_RELATIVE_BIT = 0x00000020
	WAVE_OPS_CLUSTERED_BIT = 0x00000040
	WAVE_OPS_QUAD_BIT = 0x00000080
	WAVE_OPS_PARTITIONED_BIT_NV = 0x00000100
	WAVE_OPS_ALL = 0x7FFFFFFF

def str_to_WaveopsFlags(string: str) -> WaveopsFlags:
    return getattr(WaveopsFlags, string, WaveopsFlags.WAVE_OPS_NONE)

class DescriptorSets(Enum):
    UPDATE_FREQ_NONE = 0
    UPDATE_FREQ_PER_FRAME = 1
    UPDATE_FREQ_PER_BATCH = 2
    UPDATE_FREQ_PER_DRAW = 3
    UPDATE_FREQ_USER = 4
    space4 = 5
    space5 = 6
    space6 = 7

class StructType(Enum):
    CBUFFER = 0
    PUSH_CONSTANT = 1
    STRUCT = 2

class ShaderBinary:
    def __init__(self):
        self.stage = Stages.NONE
        self.features = []
        self.preprocessed_srcs = {}
        self.filename = None
        self.fsl_filepath = None
        self.derivatives = {}
        self.num_threads = (0,0,0)
        self.waveops_flags = WaveopsFlags.WAVE_OPS_NONE
        self.output_types_mask = 0

# assert variant to handle external error messages
def fsl_platform_assert(platform: Platforms, condition, filepath, message ):
    if condition: return False

    if platform in [Platforms.ANDROID_VULKAN, Platforms.VULKAN]:
        for error in message.split('ERROR: '):
            fne = error.find(':', 2)
            if fne > 0:
                src = error[:fne]
                line, msg = error[fne+1:].split(':', 1)
                message = '{}({}): ERROR : {}\n'.format(src, line, msg )
                break

    if Platforms.ANDROID_GLES == platform:
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
    
    if platform in [Platforms.MACOS, Platforms.IOS]:
        if "metal2.4" in message:
            print("Failed to compile a raytracing shader. Xcode version needs to be >= 13.0")
            print(message)
            return True

    # Some messages don't give any information about the file, if that happens the user at least gets this filepath
    print('Error in file {0}'.format(filepath))

    print(message)
    assert False

def fsl_assert(condition, filename=None, lineno=None, message=''):
    if not condition:
        if os.name == 'nt':
            print(f"{filename}({lineno}): ERROR: {message}")
        else:
            print(f'{filename}:{lineno}: error: {message}')
        assert False

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

def internal_dependencies():
    return [
        __file__,
        os.path.join(os.path.dirname(__file__), 'fsl.py'),
        os.path.join(os.path.dirname(__file__), 'compilers.py'),
        os.path.join(os.path.dirname(__file__), 'includes', 'metal.h'),
        os.path.join(os.path.dirname(__file__), 'includes', 'vulkan.h'),
        os.path.join(os.path.dirname(__file__), 'includes', 'd3d.h'),
        os.path.join(os.path.dirname(__file__), 'includes', 'gles.h'),
        os.path.join(os.path.dirname(__file__), 'generators', 'metal.py'),
        os.path.join(os.path.dirname(__file__), 'generators', 'vulkan.py'),
        os.path.join(os.path.dirname(__file__), 'generators', 'd3d.py'),
        os.path.join(os.path.dirname(__file__), 'generators', 'gles.py'),
    ]

def internal_timestamp():
    return max( [ os.path.getmtime(d) for d in internal_dependencies() ] )

def get_stage_from_entry(line):

    stages = {
        Stages.VERT: 'VS_MAIN',
        Stages.FRAG: 'PS_MAIN',
        Stages.COMP: 'CS_MAIN',
    }

    for stage, entry_name in stages.items():
        loc = line.find(entry_name)
        if loc > -1: return stage, loc

    return None, None

def iter_lines(lines):
    file, lineno = None, 0
    for line in lines:
        if line.startswith('#line ') or line.startswith('# '):
            _, lineno, file = line.split()[:3]
            lineno, file = int(lineno) - 1, file.strip('"')
        else:
            lineno += 1
        yield file, lineno, line

def get_entry_signature(filename, src):
    entry = '_MAIN('
    waveops_flags = WaveopsFlags.WAVE_OPS_NONE
    for fi, ln, line in iter_lines(src):

        if 'ENABLE_WAVEOPS' in line:
            waveops_flag_strings = getMacro(line.strip()).split("|")
            for flagStr in waveops_flag_strings:
                flagStr = flagStr.strip()
                if not hasattr(WaveopsFlags, flagStr):
                    fsl_assert(False, fi, ln, message='Invalid WaveopsFlag \''+flag+'\'')
                    continue

                waveops_flags |= str_to_WaveopsFlags(flagStr)

        loc = line.find(entry)
        if loc > -1:
            ''' get return type '''
            target, target_loc = get_stage_from_entry(line)

            fsl_assert(target, fi, ln, message='Cannot determine Target from \''+line+'\'')

            ret = line[:target_loc].strip()
            arguments = getMacro(line[loc:])
            arguments = arguments if type(arguments) == list else [arguments]
            if len(arguments) == 1 and arguments[0] == '':
                inputs = []
            else:
                inputs = [param.strip() for param in arguments]
            fsl_assert(ret, fi, ln, message='Could not determine entry point return type: ' + line)
            return target, ret, inputs, waveops_flags
    
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

def getMacroFirstArg(line):
    result = getMacro(line)
    return result if isinstance(result, str) else result[0]

def getMacroName(line):
    br = line.find('(')
    if br > -1:
        return line[:br].strip()
    return line.strip()

def isMacro(line):
    return line.find('(') > -1 and line.rfind(')') > -1

def isArray(name):
    return '[' in name and ']' in name

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

    waveops_flags = WaveopsFlags.WAVE_OPS_NONE

    def getArrayLen(self, name):
        return getArrayLen(self.defines, name)

def collect_resources(lines: list):
    decls, rs = { dt: {} for dt in StructType }, []
    dt: StructType = None
    decl = None
    for fi, ln, line in iter_lines(lines):

        if 'RES(' in line:
            rs += [tuple(getMacro(line))]
            fsl_assert(');' in line, fi, ln, message='Resource declaration requires ;-termination')

        if decl:
            if 'DATA('in line:
                element_decl = tuple(getMacro(line))
                fsl_assert(';' in line, fi, ln, message='Data decl, needs to end with semicolon')
                fsl_assert(len(element_decl) == 3, fi, ln, message='Invalid Struct Element declaration')
                decls[dt][decl] += [element_decl]

                if StructType.PUSH_CONSTANT == dt and isArray(element_decl[1]):
                    fsl_assert(False, fi, ln, f"PUSH_CONSTANT \"{decl[0]}\" field \"{element_decl[1]}\": arrays not supported in push constants")

            elif '};' in line:
                decl, dt = None, None
            continue

        if 'PUSH_CONSTANT(' in line:
            dt = StructType.PUSH_CONSTANT
            decl = tuple(getMacro(line))
            fsl_assert(2 == len(decl), fi, ln, f"Malformed PUSH_CONSTANT declaration, should be PUSH_CONSTANT(name, register)")
            decls[dt][decl] = []

        elif 'CBUFFER(' in line:
            decl = tuple(getMacro(line))
            dt = StructType.CBUFFER
            fsl_assert(4 == len(decl), fi, ln, f"Malformed CBUFFER declaration, should be CBUFFER(name, freq, register, binding)")
            decls[dt][decl] = []

        elif 'STRUCT(' in line:
            decl = getMacro(line)
            dt = StructType.STRUCT
            fsl_assert(str == type(decl), fi, ln, f"Malformed STRUCT declaration, should be STRUCT(name)")
            decls[dt][decl] = []

    return \
        decls[StructType.CBUFFER], \
        decls[StructType.PUSH_CONSTANT], \
        decls[StructType.STRUCT], rs

def getShader(platform: Platforms, fsl_path: str, fsl: list, dst=None, line_directives=True) -> Shader:
    
    # collect shader declarations
    lines = fsl
    cbuffers, pushConstant, structs, resources = collect_resources(lines)
    stage, entry_ret, entry_args, waveops_flags = get_entry_signature(fsl_path, lines)

    ''' check entry signature '''
    # returnStruct = None
    struct_args = []
    flat_args = []
    for i, arg in enumerate(entry_args):

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
            'SV_COVERAGE',
        ]
        is_builtin = False
        for flat_arg_dtype in flat_arg_dtypes:
            if arg.upper().startswith(flat_arg_dtype):
                flat_args += [(arg_dtype, arg_var)]
                is_builtin = True

        if is_builtin:
            continue

        fsl_assert(arg_dtype in structs, fsl_path, message=': error FSL: Unknow entry argument: \''+arg+'\'')
        struct_args += [(arg_dtype, arg_var)]

        continue

    shader = Shader()
    shader.lines = lines
    shader.stage = stage
    shader.flat_args = flat_args
    shader.struct_args = struct_args

    shader.returnType = entry_ret if entry_ret != 'void' else None
    shader.cBuffers = cbuffers
    shader.structs = structs
    shader.pushConstant = pushConstant
    shader.resources = resources
    shader.waveops_flags = waveops_flags

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
    src_timestamp = max(getmtime(args.fsl_input), internal_timestamp())
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
            allTimestamps = []
            for filepath in files:
                if exists(filepath.strip()):
                    allTimestamps += [ getmtime(filepath.strip()) ]
            if not allTimestamps:
                regen.add(platform_filename)
                continue
            deps_timestamp = max(allTimestamps)
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

    include_dirs = [f'-I{filedir}', f'-I{os.path.dirname(__file__)}/includes']
    if Platforms.PROSPERO in platforms:
        import prospero as prospero_utils
        include_dirs += [prospero_utils.include_dir()]
    if Platforms.XBOX in platforms or Platforms.SCARLETT in platforms:
        import xbox as xbox_utils
        include_dirs += xbox_utils.include_dirs()

    if args.includes:
        include_dirs += [ f'-I{d}' for d in args.includes ]

    current_stage = Stages.NONE
    vs_reference_found = False

    global_features = set()

    for i, line in enumerate(source):
        line = line.strip()

        if line.startswith('#pragma '):
            fts = line[8:].split()
            for ftt in fts:
                ft = ftt.strip('FT_').strip('~FT_')
                if not ft in Features._member_names_: continue
                feature = Features[ft]
                if ftt[0] == '~' and feature in global_features:
                    global_features.remove( feature )
                if ftt[0] == 'F':
                    global_features.add( feature )

        stage = Stages.NONE
        if line.startswith('#frag'):
            stage = Stages.FRAG
        if line.startswith('#vert'):
            stage = Stages.VERT
        if line.startswith('#comp'):
            stage = Stages.COMP
        if line.startswith('#geom'):
            stage = Stages.GEOM
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
            binary.features = list(global_features)
            embed_ext = False
            if Features.MULTIVIEW in global_features:
                    embed_ext = True
            for flag in [ f[3:] for f in decl if f.startswith('FT_')]:
                if flag in Features._member_names_:
                    binary.features += [Features[flag]]
                    # Implicetly embed fsl_ext.h when multiview is requested
                    if Features.MULTIVIEW == Features[flag]:
                        embed_ext = True
            if embed_ext:
                meta_source += [ b'#include "fsl_ext.h"\n' ]
                    
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
            current_stage = Stages.NONE
            vs_reference_found = False
            meta_source += [ b'#endif\n' ]
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
                '-D__fsl',
                f'-DD_{abs(hash(binary))}',
                f'-D{platform_langs[platform]}',
                f'-DTARGET_{get_target(platform)}',
                f'-D{ "_DEBUG" if args.debug else "NDEBUG" }',
                f'-DSTAGE_{ binary.stage.name }'
            ]

            for ft in binary.features:
                if not feature_mask[ft] or platform in feature_mask[ft]:
                    cmd += [ f'-DFT_{ft.name}']

            binary.derivatives[platform] = [[]]

            deps_filepath = os.path.join( tempfile.gettempdir(), next(tempfile._get_candidate_names()) )
            if args.incremental:
                if os.name == 'nt':
                    cmd += ['-MD', deps_filepath, '-MT', f'"{binary.filename}"']
                else:
                    cmd += ['-MMD', '-MF', deps_filepath]

            cp = subprocess.run(cmd, input=source, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

            if 0 != cp.returncode:
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
                fsl_assert(os.path.exists(deps_filepath), 'Deps file could not be found')
                deps = [ l.replace('\\\n', '').strip() for l in open(deps_filepath).readlines() if l.strip() ]
                if len(deps) > 1:
                    md = [binary.filename] + [ f'\"{d.strip()}\"' for d in deps[1:] ] + ['']
                    fsl_dependencies[binary.filename] = md
                os.remove(deps_filepath)

            shaderSource = cp.stdout.replace(b'"<stdin>"', f'"{filepath}"'.encode('utf-8')).replace(b'\r\n', b'\n')

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
		'uint2',
		'uint3',
		'uint4',
        'atomic_uint',
        'uint64_t',
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

# maps CENTROID(X)/FLAT(X) to centroid/flat
def get_interpolation_modifier(dtype):
    dtype = dtype.lower()
    if 'flat' in dtype: return 'flat'
    if 'centroid' in dtype: return 'centroid'
    return None

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
    overloading_detected = False
    for i, line in enumerate(lines):
        if line.strip().startswith('//'): continue
        for m in re.finditer('{', line):
            scope_counter += 1
            if scope_counter == 1:
                counter = 0
                function = None
                insert = None
                for j, _line in enumerate(reversed(lines[:i + 1])):
                    if j == 0:
                        _line = _line[:m.end()]
                    if 'STRUCT(' in _line or 'struct ' in _line or '=' in _line:
                        break
                    for k, c in enumerate(reversed(_line)):
                        if c == ')':
                            counter -= 1
                        if c == '(':
                            counter += 1
                            if counter == 0:
                                function = _line
                                insert = i - j, len(_line) - k
                                break
                    if function: break
                def skip_keyword(fn):
                    fn_mask = [
                        'STRUCT(',
                        'PUSH_CONSTANT(',
                        '_MAIN(',
                    ]
                    for m in fn_mask:
                        if not fn or m in fn: return True
                    return False
                if not skip_keyword(function): # (not 'FSL_' in function : # skip cbuffer, push_constant and entry fn
                    function = _line.split('(')[0].split()[-1]
                    if function in table:
                        overloading_detected = True
                        print(f'error: function "{function}" is already defined.')
                    table[function] = (lines[insert[0]], insert)
        for _ in re.finditer('}', line):
            scope_counter -= 1

    # print('Function Table:')
    # for k, v in table.items():
    #     print(k, v)
    # sys.exit(0)
    if overloading_detected:
        raise Exception('error: function overloading is not supported.')
    return table
