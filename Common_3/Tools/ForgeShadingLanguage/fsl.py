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
    os.environ['FSL_COMPILER_FXC'] = os.path.normpath('C:/Program Files (x86)/Windows Kits/10.0.17763.0/x64')
if not 'FSL_COMPILER_DXC' in os.environ:
    os.environ['FSL_COMPILER_DXC'] = os.path.normpath(forge_root+'/Common_3/ThirdParty/OpenSource/DirectXShaderCompiler/bin/x64')
if not 'FSL_COMPILER_METAL' in os.environ:
    os.environ['FSL_COMPILER_METAL'] = os.path.normpath('C:/Program Files/Metal Developer Tools/macos/bin')

from utils import *
import generators, compilers

def initArgs():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--destination', help='output directory', required=True)
    parser.add_argument('-b', '--binaryDestination', help='output directory', required=True)
    parser.add_argument('-l', '--language', help='language, defaults to all')
    parser.add_argument('fsl_input', help='fsl file to generate from')
    parser.add_argument('--verbose', default=False, action='store_true')
    parser.add_argument('--compile', default=False, action='store_true')
    parser.add_argument('--rootSignature', default=None)
    args = parser.parse_args()
    args.language = args.language.split()
    return args

def main():
    args = initArgs()

    ''' collect shader languages '''
    languages = []
    for language in args.language:
        fsl_assert(language in [l.name for l in Languages], filename=args.fsl_input, message='Invalid target language {}'.format(language))
        languages += [Languages[language]]
        

    if args.verbose:
        print('FSL: Generating {}, from {}'.format(' '.join([l.name for l in languages]), args.fsl_input))

    class Gen:
        def __init__(self, g,c):
            self.gen, self.compile = g,c
        gen = None
        compile = None

    gen_map = {
        Languages.DIRECT3D11:       Gen(generators.d3d,    compilers.d3d11),
        Languages.DIRECT3D12:       Gen(generators.d3d12,  compilers.d3d12),
        Languages.VULKAN:      Gen(generators.vulkan, compilers.vulkan),
        Languages.METAL:       Gen(generators.metal,  compilers.metal),
        Languages.ORBIS:       Gen(generators.pssl,   compilers.orbis),
        Languages.PROSPERO:    Gen(generators.prospero,   compilers.prospero),
        Languages.XBOX :       Gen(generators.xbox,   compilers.xbox),
        Languages.SCARLETT :   Gen(generators.scarlett,   compilers.scarlett),
        Languages.GLES :       Gen(generators.gles,   compilers.gles),
    }

    if not os.path.exists(args.fsl_input):
        print(__file__+'('+str(currentframe().f_lineno)+'): error FSL: Cannot open source file \''+args.fsl_input+'\'')
        sys.exit(1)

    fsl_assert(args.destination, filename=args.fsl_input, message='Missing destionation directory')

    for language in languages:
        out_filename = os.path.basename(args.fsl_input).replace('.fsl', '')

        if language == Languages.METAL:
            out_filename = out_filename.replace('.tesc', '.tesc.comp')
            out_filename = out_filename.replace('.tese', '.tese.vert')
            out_filename += '.metal'

        dst_dir = args.destination
        # if generating multiple language in a single call, create per-language subdirectories
        if len(languages) > 1:
            dst_dir = os.path.join(dst_dir, language.name)
        os.makedirs(dst_dir, exist_ok=True)
        out_filepath = os.path.normpath(os.path.join(dst_dir, out_filename)).replace(os.sep, '/')

        rootSignature = None
        if args.rootSignature and args.rootSignature != 'None':
            assert os.path.exists(args.rootSignature)
            rootSignature = open(args.rootSignature).read()
            status = gen_map[language].gen(args.fsl_input, out_filepath, rootSignature=rootSignature)
        else:
            status = gen_map[language].gen(args.fsl_input, out_filepath)
        if status != 0: return 1

        if args.compile:
            fsl_assert(dst_dir, filename=args.fsl_input, message='Missing destionation binary directory')
            if not os.path.exists(args.binaryDestination): os.makedirs(args.binaryDestination)
            bin_filepath = os.path.join( args.binaryDestination, os.path.basename(out_filepath) )
            status = gen_map[language].compile(out_filepath, bin_filepath)
            if status != 0: return 1

    return 0

if __name__ == '__main__':
    sys.exit(main())