"""
FSL shader generator tool
"""

import os, sys, argparse, subprocess, time
from inspect import currentframe, getframeinfo

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
    parser.add_argument('-l', '--language', help='language, defaults to all')
    parser.add_argument('-I', '--includes', help='optional include dirs', nargs='+')
    parser.add_argument('fsl_input', help='fsl file to generate from')
    parser.add_argument('--verbose', default=False, action='store_true')
    parser.add_argument('--compile', default=False, action='store_true')
    parser.add_argument('--debug', default=True, action='store_true')
    parser.add_argument('--rootSignature', default=None)
    parser.add_argument('--incremental', default=False, action='store_true')
    args = parser.parse_args()
    if args.language:
        args.language = args.language.split()
    return args

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

    for platform in platforms:
    
        # Create per-platform subdirectories
        dst_dir = os.path.join(args.destination, platform.name)
        os.makedirs(dst_dir, exist_ok=True)


        if args.compile:
            bin_dir = os.path.join( args.binaryDestination, platform.name)
            os.makedirs(bin_dir, exist_ok=True)

        for i, binary in enumerate(binary_declarations):

            out_filename = binary.filename

            if platform == Platforms.MACOS or platform == Platforms.IOS:
                out_filename = out_filename.replace('.tesc', '.tesc.comp')
                out_filename = out_filename.replace('.tese', '.tese.vert')
                out_filename += '.metal'

            # generate
            out_filepath = os.path.normpath(os.path.join(dst_dir, out_filename)).replace(os.sep, '/')

            if args.incremental and regen is not None and out_filename not in regen:
                continue
            
            if platform not in binary.preprocessed_srcs:
                if args.verbose:
                    print('FSL: Empty Declaration {} {}'.format(platform.name, out_filename))
                continue

            generator = getattr(generators, platform.name.lower())
            status, deps = generator(args.debug, binary, out_filepath)
            if status != 0: return 1

            # compile
            if args.compile:
                bin_filepath = os.path.normpath(os.path.join(bin_dir, out_filename)).replace(os.sep, '/')
                if args.verbose:
                    print(f'Compiling: {platform.name} {binary.stage.name} {binary.target.name}\n\t{out_filepath}\n\t{bin_filepath}')
                status = compile_binary(platform, args.debug, binary, out_filepath, bin_filepath)
                if status != 0: return 1

                for stage, out_filepath, out_filename in deps:
                    bin_filepath = os.path.normpath(os.path.join(bin_dir, out_filename)).replace(os.sep, '/')
                    if args.verbose:
                        print(f'Compiling: {platform.name} {stage.name} {binary.target.name}\n\t{out_filepath}\n\t{bin_filepath}')
                    temp_stage = binary.stage
                    binary.stage = stage
                    status = compile_binary(platform, args.debug, binary, out_filepath, bin_filepath)
                    binary.stage = temp_stage
                    if status != 0: return 1

    return 0

if __name__ == '__main__':
    sys.exit(main())