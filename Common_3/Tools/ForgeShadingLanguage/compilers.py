import shutil
import subprocess
import os
from utils import Languages, fsl_assert

fsl_basepath = os.path.dirname(__file__)

_config = {
    Languages.DIRECT3D11: ('FSL_COMPILER_FXC', 'fxc.exe'),
    Languages.DIRECT3D12: ('FSL_COMPILER_DXC', 'dxc.exe'),
    Languages.VULKAN:     ('VULKAN_SDK', 'Bin/glslangValidator.exe'),
    Languages.METAL:      ('FSL_COMPILER_METAL', 'metal.exe'),
    Languages.ORBIS:      ('SCE_ORBIS_SDK_DIR', 'host_tools/bin/orbis-wave-psslc.exe'),
    Languages.PROSPERO:   ('SCE_PROSPERO_SDK_DIR', 'host_tools/bin/prospero-wave-psslc.exe'),
    Languages.XBOX:       ('GXDKLATEST', 'bin/XboxOne/dxc.exe'),
    Languages.SCARLETT:   ('GXDKLATEST', 'bin/Scarlett/dxc.exe'),
    Languages.GLES:       ('VULKAN_SDK', 'Bin/glslangValidator.exe'),
}

def get_available_compilers():
    available = []
    for lang, compiler_path in _config.items():
        if get_compiler_from_env(*compiler_path, _assert=False):
            available += [lang.name]
    return available

def get_status(bin, params):
    return  subprocess.getstatusoutput([bin] + params)

def get_compiler_from_env(varname, subpath = None, _assert=True):
    if not varname in os.environ:
        print('WARN: {} not in env vars'.format(varname))
        if _assert: assert False
        return None
    compiler = os.environ[varname]
    if subpath: compiler = os.path.join(compiler, subpath)
    if not os.path.exists(compiler):
        print('WARN: {} doesn\'t exist'.format(compiler))
        if _assert: assert False
        return None
    return os.path.abspath(compiler)

def vulkan(src, dst):
    print('--> compiling VULKAN (glslangValidator) {}'.format(src))
    
    bin = get_compiler_from_env(*_config[Languages.VULKAN])

    ''' prepend glsl directives '''
    params = ['-V', src, '-o', dst, '-I'+fsl_basepath]
    # params = ['-DFSL_GLSL', '-DSAMPLE_COUNT 2', '-V', src, '-o', dst, '-I'+fsl_basepath]
    # params = ['-DFSL_GLSL', '-DSAMPLE_COUNT 2', src, '-I'+fsl_basepath, '-E']
    if '.frag' in src:
        params += ['-S', 'frag']
    if '.vert' in src:
        params += ['-S', 'vert']
    if '.comp' in src:
        params += ['-S', 'comp']
    if '.geom' in src:
        params += ['-S', 'geom']
    if '.tesc' in src:
        params += ['-S', 'tesc']
    if '.tese' in src:
        params += ['-S', 'tese']

    params += ['--target-env', 'vulkan1.1']

    status, output = get_status(bin, params)
    fsl_assert(status == 0, src, message=output)
    return status

    # return get_status(bin, params)

def metal(src, dst):
    print('--> compiling METAL (metal.exe) {}'.format(src))

    bin = get_compiler_from_env(*_config[Languages.METAL])

    # params = ['-D','FSL_METAL', '-dD', '-C', '-E', '-o', dst, tmp]
    # params = ['-D','FSL_METAL', '-dD', '-S', '-o', dst, tmp]
    # params = ['-dD', '-I', fsl_basepath, '-o', dst, tmp]

    params = ['-I', fsl_basepath]
    params += ['-dD', '-o', dst, src]

    

    status, output = get_status(bin, params)
    fsl_assert(status == 0, src+'.fsl', message=output)
    return status
    
    # return get_status(bin, params)

def d3d11(src, dst):
    print('--> compiling DIRECT3D11 (fxc) {}'.format(src))
    
    bin = get_compiler_from_env(*_config[Languages.DIRECT3D11])

    params = ['/Zi']
    if '.frag' in src:
        params += ['/T', 'ps_5_0']
    if '.vert' in src:
        params += ['/T', 'vs_5_0']
    if '.comp' in src:
        params += ['/T', 'cs_5_0']

    # handling network path on windows
    if src.startswith("//"):
        src = src.replace("//", "\\\\", 1)

    if dst.startswith("//"):
        dst = dst.replace("//", "\\\\", 1)

    params += ['/I', fsl_basepath, '/Fo', dst, src]
    # params = []
    # params += ['/I', fsl_basepath, '/P', dst, src]
    

    status, output = get_status(bin, params)
    fsl_assert(status == 0, src+'.fsl', message=output)
    return status

def d3d12(src, dst):
    print('--> compiling DIRECT3D12 (dxc) {}'.format(src))
    
    bin = get_compiler_from_env(*_config[Languages.DIRECT3D12])

    params = []
    if '.frag' in src:
        params += ['/T', 'ps_6_4']
    if '.vert' in src:
        params += ['/T', 'vs_6_4']
    if '.comp' in src:
        params += ['/T', 'cs_6_0']
    if '.geom' in src:
        params += ['/T', 'gs_6_0']
    if '.tesc' in src:
        params += ['/T', 'hs_6_0']
    if '.tese' in src:
        params += ['/T', 'ds_6_0']
    # params += ['/I', fsl_basepath, '/DFSL_D3D12', '/Fo', dst, src]
    params += ['/I', fsl_basepath, '/Fo', dst, src]
    

    status, output = get_status(bin, params)
    fsl_assert(status == 0, src+'.fsl', message=output)
    return status

def orbis(src, dst):
    print('--> compiling orbis-wave-psslc {}'.format(src))

    bin = get_compiler_from_env(*_config[Languages.ORBIS])

    params = ['-DGNM', '-O4']
    if '.frag' in src:
        params += ['-profile', 'sce_ps_orbis']
    if '.vert' in src:
        params += ['-profile', 'sce_vs_vs_orbis']
    if '.comp' in src:
        params += ['-profile', 'sce_cs_orbis']
    if '.tesc' in src:
        params += ['-profile', 'sce_hs_off_chip_orbis']
    if '.tese' in src:
        params += ['-profile', 'sce_ds_vs_off_chip_orbis']
    if '.geom' in src:
        params += ['-profile', 'sce_gs_orbis']
    params += ['-I'+fsl_basepath, '-o', dst + '.sb', src]
    # params += ['-I'+fsl_basepath, '-o', dst + '.sb', src, '-E']
    # params += ['-I'+fsl_basepath, src, '-E']
    
    status, output = get_status(bin, params)
    fsl_assert(status == 0, src+'.fsl', message=output)
    if output:
        print(output)
    return status
    
    # return get_status(bin, params)

def prospero(src, dst):
    print('--> compiling prospero-wave-psslc {}'.format(src))

    bin = get_compiler_from_env(*_config[Languages.PROSPERO])

    params = ['-DGNM', '-O4']
    if '.frag' in src:
        params += ['-profile', 'sce_ps_prospero']
    if '.vert' in src:
        params += ['-profile', 'sce_vs_vs_prospero']
    if '.comp' in src:
        params += ['-profile', 'sce_cs_prospero']
    if '.tesc' in src:
        params += ['-profile', 'sce_hs_prospero']
    if '.tese' in src:
        params += ['-profile', 'sce_ds_vs_prospero']
    if '.geom' in src:
        params += ['-profile', 'sce_gs_prospero']
    params += ['-I'+fsl_basepath, '-o', dst, src]
    
    status, output = get_status(bin, params)
    fsl_assert(status == 0, src+'.fsl', message=output)
    if output:
        print(output)
    return status


def xbox(src, dst):
    print('--> compiling (dxc) XBOX {}'.format(src))

    bin = get_compiler_from_env(*_config[Languages.XBOX])

    params = ['/Zi', '-Qembed_debug']
    if '.frag' in src:
        params += ['/T', 'ps_6_0']
    if '.vert' in src:
        params += ['/T', 'vs_6_0']
    if '.comp' in src:
        params += ['/T', 'cs_6_0']
    if '.tesc' in src:
        params += ['/T', 'hs_6_0']
    if '.tese' in src:
        params += ['/T', 'ds_6_0']
    params += ['/I', fsl_basepath, '/D__XBOX_DISABLE_PRECOMPILE', '/Fo', dst, src]
    
    status, output = get_status(bin, params)
    fsl_assert(status == 0, src+'.fsl', message=output)
    if output:
        print(output)
    return status

def scarlett(src, dst):
    print('--> compiling (dxc) SCARLETT {}'.format(src))

    bin = get_compiler_from_env(*_config[Languages.SCARLETT])

    params = ['/Zi', '-Qembed_debug']
    if '.frag' in src:
        params += ['/T', 'ps_6_0']
    if '.vert' in src:
        params += ['/T', 'vs_6_0']
    if '.comp' in src:
        params += ['/T', 'cs_6_0']
    if '.tesc' in src:
        params += ['/T', 'hs_6_0']
    if '.tese' in src:
        params += ['/T', 'ds_6_0']
    params += ['/I', fsl_basepath, '/D__XBOX_DISABLE_PRECOMPILE', '/Fo', dst, src]
    
    status, output = get_status(bin, params)
    fsl_assert(status == 0, src+'.fsl', message=output)
    if output:
        print(output)
    return status
    return status

def gles(src, dst):
    print('--> compiling GLES using (glslangValidator) {}'.format(src))
    
    bin = get_compiler_from_env(*_config[Languages.GLES])

    ''' prepend glsl directives '''
    params = [src, '-I'+fsl_basepath]
    if '.frag' in src:
        params += ['-S', 'frag']
    if '.vert' in src:
        params += ['-S', 'vert']
    if '.comp' in src:
        params += ['-S', 'comp']
    if '.geom' in src:
        params += ['-S', 'geom']
    if '.tesc' in src:
        params += ['-S', 'tesc']
    if '.tese' in src:
        params += ['-S', 'tese']

    status, output = get_status(bin, params)
    fsl_assert(status == 0, src, message=output)
    return status