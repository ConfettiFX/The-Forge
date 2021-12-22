import os, sys

parent_dir = os.path.sep.join(os.path.abspath(__file__).split(os.path.sep)[:-2])
sys.path.append(parent_dir)
from fsl import main

assert len(sys.argv) == 9
script, windowsSDK, dst, binary_dst, src, lang, compile, verbose, config = sys.argv

windowsSDKPaths = windowsSDK.split(';')
if windowsSDKPaths:
    os.environ['FSL_COMPILER_FXC'] = windowsSDKPaths[0]

if 'Auto' == lang:
    if config.endswith('Dx11'):
        lang = 'DIRECT3D11'
    elif config.endswith('Dx'):
        lang = 'DIRECT3D12'
    elif config.endswith('Vk'):
        lang = 'VULKAN'
    elif config.endswith('GLES'):
        lang = 'GLES'
    else:
        print(src+': error FSL: Could not deduce target lang from current VS Config.')
        sys.exit(1)

sys.argv = [
    script,
    src,
    '-d', dst,
    '-b', binary_dst,
    '-l', lang,
    '--incremental',
]
if verbose=='true': sys.argv += ['--verbose']
if compile=='true': sys.argv += ['--compile']

sys.exit(main())