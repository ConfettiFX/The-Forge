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
FSL shader generator tool
"""

import os, sys, argparse, json, hashlib, socket
from inspect import currentframe, getframeinfo
from multiprocessing import Pool, cpu_count

# add fsl roots to path
fsl_root = os.path.sep.join(os.path.abspath(__file__).split(os.path.sep)[:-1])
forge_root = os.path.dirname(os.path.dirname(os.path.dirname(fsl_root)))
fsl_orbis_root    = os.path.join(forge_root, 'PS4', 'Common_3', 'Tools', 'ForgeShadingLanguage')
fsl_prospero_root = os.path.join(forge_root, 'Prospero', 'Common_3', 'Tools', 'ForgeShadingLanguage')
fsl_xbox_root     = os.path.join(forge_root, 'Xbox', 'Common_3', 'Tools', 'ForgeShadingLanguage')
sys.path.extend( [fsl_root, fsl_orbis_root, fsl_prospero_root, fsl_xbox_root])

# set default compiler paths
if not 'FSL_COMPILER_FXC' in os.environ:
    os.environ['FSL_COMPILER_FXC'] = os.path.normpath('C:/Program Files (x86)/Windows Kits/10/bin/10.0.17763.0/x64')
if not 'FSL_COMPILER_DXC' in os.environ:
    os.environ['FSL_COMPILER_DXC'] = os.path.normpath(forge_root+'/Common_3/Graphics/ThirdParty/OpenSource/DirectXShaderCompiler/bin/x64')
if not 'FSL_COMPILER_VK' in os.environ:
    os.environ['FSL_COMPILER_VK'] = os.path.normpath(forge_root+'/Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Win32/')
    os.environ['FSL_COMPILER_LINUX_VK'] = os.path.normpath(forge_root+'/Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Linux/')
if not 'FSL_COMPILER_GLES' in os.environ:
    os.environ['FSL_COMPILER_GLES'] = os.path.normpath(forge_root+'/Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Win32/')
if not 'FSL_COMPILER_MACOS' in os.environ:
    os.environ['FSL_COMPILER_MACOS'] = os.path.normpath('C:/Program Files/Metal Developer Tools/macos/bin')
if not 'FSL_COMPILER_IOS' in os.environ:
    os.environ['FSL_COMPILER_IOS'] = os.path.normpath('C:/Program Files/Metal Developer Tools/ios/bin')

from utils import *
import generators, compilers
from compilers import compile_binary

def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--destination', help='output directory', required=True)
    parser.add_argument('-b', '--binaryDestination', help='output directory', required=True)
    parser.add_argument('-i', '--intermediateDestination', help='intermediate directory used for caching invocations of `fsl.py`')
    parser.add_argument('-l', '--language', help='language, defaults to all')
    parser.add_argument('-I', '--includes', help='optional include dirs', nargs='+')
    parser.add_argument('fsl_input', help='fsl file to generate from')
    parser.add_argument('--verbose', default=False, action='store_true')
    parser.add_argument('--compile', default=False, action='store_true')
    parser.add_argument('--debug', default=False, action='store_true')
    parser.add_argument('--rootSignature', default=None)
    parser.add_argument('--incremental', default=False, action='store_true')
    parser.add_argument('--mp', type=int, default=-1)
    parser.add_argument('--shaderServerPort', type=int, default=6543,
                        help='Port written to `shader-server.txt` which is used by the device to recompile shaders on a ShaderServer running on this port')
    parser.add_argument('--cache-args', action='store_true', help='Cache arguments used to invoke `fsl.py` and also generate `shader-server.txt` used by ShaderServer client application') 
    args = parser.parse_args()
    if args.language:
        args.language = args.language.split()
    if not args.intermediateDestination:
        args.intermediateDestination = args.binaryDestination
    return args

def process_binary_declaration(job):

    args, regen, binary, platform, dst_dir, bin_dir = job

    out_filename = binary.filename
    if platform == Platforms.MACOS or platform == Platforms.IOS:
        out_filename += '.metal'

    # generate
    out_filepath = os.path.normpath(os.path.join(dst_dir, out_filename)).replace(os.sep, '/')

    if args.incremental and regen is not None and out_filename not in regen:
        return 0
    
    if platform not in binary.preprocessed_srcs:
        if args.verbose:
            print('FSL: Empty Declaration {} {}'.format(platform.name, out_filename))
        return 0

    generator = getattr(generators, platform.name.lower())
    try:
        status, deps = generator(args.debug, binary, out_filepath)
    except Exception as e:
        for arg in e.args:
            print(arg)
        return 1
    if status != 0: return 1

    # compile
    if args.compile:
        bin_filepath = os.path.normpath(os.path.join(bin_dir, out_filename)).replace(os.sep, '/')
        if args.verbose:
            print(f'Compiling {os.getpid()}: {platform.name} {binary.stage.name} \n\t{out_filepath}\n\t{bin_filepath}')
        try:
            compile_binary(platform, args.debug, binary, out_filepath, bin_filepath)
        except SystemExit:
            return 1

        for stage, out_filepath, out_filename in deps:
            bin_filepath = os.path.normpath(os.path.join(bin_dir, out_filename)).replace(os.sep, '/')
            if args.verbose:
                print(f'Compiling Dep {os.getpid()}: {platform.name} {stage.name} \n\t{out_filepath}\n\t{bin_filepath}')
            temp_stage = binary.stage
            binary.stage = stage
            try:
                compile_binary(platform, args.debug, binary, out_filepath, bin_filepath)
            except SystemExit:
                return 1
            binary.stage = temp_stage
    return 0


def get_host_ip():
    (_, _, ips) = socket.gethostbyname_ex(socket.gethostname())
    for ip in sorted(ips):
        if ip.startswith('192.168'):
            return ip
    raise RuntimeError(f'Could not find local network IP in the list of all IP addresses: {ips}')


def generate_shader_server_info(fsl_input, binary_dir, intermediate_dir, port):
    # write info to text file
    shader_server_file = os.path.join(binary_dir, 'shader-server.txt')
    with open(shader_server_file, 'w') as f:
        lines = [
            get_host_ip(),
            str(port),
            os.getcwd(), 
            binary_dir, 
            intermediate_dir
        ]
        for line in lines:
            f.write(line)
            f.write('\n')

    # write arguments to JSON file
    fsl_cmds = os.path.join(intermediate_dir, 'fsl_cmds')
    os.makedirs(fsl_cmds, exist_ok=True)
    file_name = str(hashlib.md5(fsl_input.encode()).hexdigest()) + '.json'
    with open(os.path.join(fsl_cmds, file_name), 'w') as f:
        json.dump(sys.argv[1:], f)


def main():
    args = get_args()


    ''' collect shader Platforms '''
    platforms = []
    if args.language:
        for language in args.language:
            fsl_assert(language in [l.name for l in Platforms], filename=args.fsl_input, message='Invalid target language {}'.format(language))
            platforms += [Platforms[language]]
    else:
        for platform in Platforms:
            platforms += [platform]

    if args.fsl_input.endswith('.h.fsl'):
        return 0

    if not os.path.exists(args.fsl_input):
        print(__file__+'('+str(currentframe().f_lineno)+'): error FSL: Cannot open source file \''+args.fsl_input+'\'')
        sys.exit(1)

    fsl_input = os.path.normpath(args.fsl_input).replace(os.sep, '/')

    dependency_filepath = os.path.join(args.destination, os.path.basename(args.fsl_input)) + '.deps'
    regen = set()
    dependencies = {}
    if args.incremental:
        if not needs_regen(args, dependency_filepath, platforms, regen, dependencies):
            return 0
        if 'deps' in regen:
            regen = None

    fsl_assert(args.destination, filename=fsl_input, message='Missing destionation directory')
    binary_declarations, fsl_dependencies = collect_shader_decl(args, fsl_input, platforms, regen, dependencies, [])

    if not binary_declarations:
        return 0

    if args.verbose:
        print('FSL: Generating {}, from {}'.format([p.name for p in platforms], args.fsl_input))
    
    os.makedirs(args.destination, exist_ok=True)

    if args.incremental:
        with open(dependency_filepath, 'w') as deps:
            deps.write(':{}\n\n'.format(os.path.normpath(os.path.abspath(args.fsl_input)).replace(os.sep, '/')))
            deps.write('\n'.join( '\n'.join(kv[1]) for kv in fsl_dependencies.items() ))

    jobs = {stage: [] for stage in Stages}
    for platform in platforms:

        # Create per-platform subdirectories
        dst_dir = os.path.join(args.destination, platform.name)
        os.makedirs(dst_dir, exist_ok=True)
        bin_dir = None
        if args.compile:
            bin_dir = os.path.join(args.binaryDestination, platform.name)
            os.makedirs(bin_dir, exist_ok=True)

            for binary in binary_declarations:
                job = (args, regen, binary, platform, dst_dir, bin_dir)
                jobs[binary.stage] += [job]
    exit_code = 0

    if args.mp:
        nprocess = cpu_count() if args.mp == -1 else args.mp
        for stagejobs in jobs.values():
            if not stagejobs: continue
            with Pool( processes=nprocess ) as pool:
                ret = pool.map(process_binary_declaration, stagejobs)
                pool.close()
                pool.join()
                exit_code = next((ret_code for ret_code in ret if ret_code != 0), exit_code)
    else:
        for stagejobs in jobs.values():
            for job in stagejobs:
                ret = process_binary_declaration(job)
                if ret != 0: 
                    exit_code = ret

    if args.shaderServerPort and args.cache_args:
        generate_shader_server_info(args.fsl_input, args.binaryDestination, args.intermediateDestination, args.shaderServerPort)
    
    return exit_code


if __name__ == '__main__':
    sys.exit(main())