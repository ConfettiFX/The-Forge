import shutil
import subprocess
import os, sys
from utils import Platforms, fsl_assert, Stages, ShaderTarget, StageFlags, ShaderBinary, fsl_platform_assert
import tempfile, struct

fsl_basepath = os.path.dirname(__file__)

_config = {
    Platforms.DIRECT3D11: ('FSL_COMPILER_FXC', 'fxc.exe'),
    Platforms.DIRECT3D12: ('FSL_COMPILER_DXC', 'dxc.exe'),
    Platforms.VULKAN:     ('VULKAN_SDK', 'Bin/glslangValidator.exe'),
    Platforms.ANDROID:    ('VULKAN_SDK', 'Bin/glslangValidator.exe'),
    Platforms.SWITCH:     ('VULKAN_SDK', 'Bin/glslangValidator.exe'),
    Platforms.QUEST:      ('VULKAN_SDK', 'Bin/glslangValidator.exe'),
    Platforms.MACOS:      ('FSL_COMPILER_MACOS', 'metal.exe'),
    Platforms.IOS:        ('FSL_COMPILER_IOS', 'metal.exe'),
    Platforms.ORBIS:      ('SCE_ORBIS_SDK_DIR', 'host_tools/bin/orbis-wave-psslc.exe'),
    Platforms.PROSPERO:   ('SCE_PROSPERO_SDK_DIR', 'host_tools/bin/prospero-wave-psslc.exe'),
    Platforms.XBOX:       ('GXDKLATEST', 'bin/XboxOne/dxc.exe'),
    Platforms.SCARLETT:   ('GXDKLATEST', 'bin/Scarlett/dxc.exe'),
    Platforms.GLES:       ('VULKAN_SDK', 'Bin/glslangValidator.exe'),
}

def get_available_compilers():
    available = []
    for lang, compiler_path in _config.items():
        if get_compiler_from_env(*compiler_path, _assert=False):
            available += [lang.name]
    return available

def get_status(bin, params):
    # For some reason calling subprocess.getstatusoutput on these platforms fails
    if sys.platform == "darwin" or sys.platform == "linux":
        result = subprocess.run([bin] + params, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return result.returncode, result.stderr.decode() + result.stdout.decode()
    else:
        return subprocess.getstatusoutput([bin] + params)

def get_compiler_from_env(varname, subpath = None, _assert=True):

    if os.name == 'posix' and 'metal.exe' in subpath:
        return 'xcrun'

    if sys.platform == "linux" and 'VULKAN_SDK' in varname:
        return "glslangValidator"

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

def util_shadertarget_dx(stage, shader_target):
    stage_dx = {
        Stages.VERT: 'vs',
        Stages.FRAG: 'ps',
        Stages.COMP: 'cs',
        Stages.GEOM: 'gs',
        Stages.TESC: 'hs',
        Stages.TESE: 'ds',
    }
    return stage_dx[stage] + shader_target.name[2:]

def util_shadertarget_metal(shader_target):
    return shader_target.name[4:].replace("_",".")

def compile_binary(platform: Platforms, debug: bool, binary: ShaderBinary, src, dst):
    
    # remove any existing binaries
    if os.path.exists(dst): os.remove(dst)

    if sys.platform == 'win32': # todo
        if src.startswith("//"):
            src = src.replace("//", "\\\\", 1)

        if dst.startswith("//"):
            dst = dst.replace("//", "\\\\", 1)

    bin = get_compiler_from_env(*_config[platform])

    class CompiledDerivative:
        def __init__(self):
            self.hash = 0
            self.code = b''

    compiled_derivatives = []
    for derivative_index, derivative in enumerate(binary.derivatives[platform]):
        
        compiled_filepath = os.path.join(tempfile.gettempdir(), next(tempfile._get_candidate_names()))
        params = []

        if platform in [Platforms.VULKAN, Platforms.QUEST, Platforms.SWITCH, Platforms.ANDROID]:
            if debug: params += ['-g']
            params += ['-V', src, '-o', compiled_filepath, '-I'+fsl_basepath]
            params += ['-S', binary.stage.name.lower()]
            params += ['--target-env', 'vulkan1.1']

        elif platform == Platforms.DIRECT3D11:
            if debug: params += ['/Zi']
            params += ['/T', util_shadertarget_dx(binary.stage, ShaderTarget.ST_5_0)] # d3d11 doesn't support other shader targets
            params += ['/I', fsl_basepath, '/Fo', compiled_filepath, src]

        elif platform == Platforms.DIRECT3D12:
            if debug: params += ['/Zi', '-Qembed_debug']
            params += ['/T', util_shadertarget_dx(binary.stage, binary.target)]
            params += ['/I', fsl_basepath, '/Fo', compiled_filepath, src]

        elif platform == Platforms.ORBIS:
            # todo: if debug: ...

            params = ['-DGNM', '-O4', '-cache', '-cachedir', os.path.dirname(dst)]
            shader_profile = {
                Stages.VERT: 'sce_vs_vs_orbis',
                Stages.FRAG: 'sce_ps_orbis',
                Stages.COMP: 'sce_cs_orbis',
                Stages.GEOM: 'sce_gs_orbis',
                Stages.TESC: 'sce_hs_off_chip_orbis',
                Stages.TESE: 'sce_ds_vs_off_chip_orbis',
            }

            profile = shader_profile[binary.stage]

            # Vertex shader is local shader in tessellation pipeline and domain shader is the vertex shader
            if binary.stage == Stages.VERT:
                if StageFlags.ORBIS_TESC in binary.flags:
                    profile = 'sce_vs_ls_orbis'
                elif StageFlags.ORBIS_GEOM in binary.flags:
                    profile = 'sce_vs_es_orbis'
            
            params += ['-profile', profile]
            params += ['-I'+fsl_basepath, '-o', compiled_filepath, src]

        elif platform == Platforms.PROSPERO:
            # todo: if debug: ...
            params = ['-DGNM', '-O4']
            shader_profile = {
                Stages.VERT: 'sce_vs_vs_prospero',
                Stages.FRAG: 'sce_ps_prospero',
                Stages.COMP: 'sce_cs_prospero',
                Stages.GEOM: 'sce_gs_prospero',
                Stages.TESC: 'sce_hs_prospero',
                Stages.TESE: 'sce_ds_vs_prospero',
            }
            params += ['-profile', shader_profile[binary.stage]]
            params += ['-I'+fsl_basepath, '-o', compiled_filepath, src]

        elif platform == Platforms.XBOX:
            if debug: params += ['/Zi', '-Qembed_debug']
            params += ['/T', util_shadertarget_dx(binary.stage, binary.target)]
            params += ['/I', fsl_basepath, '/D__XBOX_DISABLE_PRECOMPILE', '/Fo', compiled_filepath, src]

        elif platform == Platforms.SCARLETT:
            if debug: params += ['/Zi', '-Qembed_debug']
            params += ['/T', util_shadertarget_dx(binary.stage, binary.target)]
            params += ['/I', fsl_basepath, '/D__XBOX_DISABLE_PRECOMPILE', '/Fo', compiled_filepath, src]

        elif platform == Platforms.GLES:
            params = [src, '-I'+fsl_basepath]
            params += ['-S', binary.stage.name.lower()]
            with open(compiled_filepath, "wb") as dummy:
                dummy.write(b'NULL')


        elif platform == Platforms.MACOS:
            # todo: if debug: ...
            if os.name == 'nt':
                params = ['-I', fsl_basepath]
                params += ['-dD', '-o', compiled_filepath, src]
            else:
                params = '-sdk macosx metal '.split(' ')
                params += [f"-std=macos-metal{util_shadertarget_metal(binary.target)}"]
                params += ['-I', fsl_basepath]
                params += ['-dD', src, '-o', compiled_filepath]

        elif platform == Platforms.IOS:
            # todo: if debug: ...
            if os.name == 'nt':
                params = ['-I', fsl_basepath]
                params += ['-dD','-std=ios-metal2.2', '-mios-version-min=8.0', '-o', compiled_filepath, src]
            else:
                params = '-sdk iphoneos metal'.split(' ')
                params += [f"-std=ios-metal{util_shadertarget_metal(binary.target)}"]
                params += ['-mios-version-min=11.0']
                params += ['-I', fsl_basepath]
                params += ['-dD', src, '-o', compiled_filepath]

        params += ['-D' + define for define in derivative ]

        cp = subprocess.run([bin] + params, stdout=subprocess.PIPE)
        fsl_platform_assert(platform, cp.returncode == 0, binary.fsl_filepath, message=cp.stdout.decode())

        with open(compiled_filepath, 'rb') as compiled_binary:
            cd = CompiledDerivative()
            cd.hash = derivative_index # hash(''.join(derivative)) #TODO If we use a hash it needs to be the same as in C++
            cd.code = compiled_binary.read()
            compiled_derivatives += [ cd ]

    with open(dst, 'wb') as combined_binary:

        # Needs to match:
        #
        # struct FSLMetadata
        # {
        #     uint32_t mUseMultiView;
        # };
        #
        # struct FSLHeader
        # {
        #     char mMagic[4];
        #     uint32_t mDerivativeCount;
        #     FSLMetadata mMetadata;
        # };

        num_derivatives = len(compiled_derivatives)
        combined_binary.write(struct.pack('=4sI', b'@FSL', num_derivatives) )
        combined_binary.write(struct.pack('=I', StageFlags.VR_MULTIVIEW in binary.flags))
        data_start = combined_binary.tell() + 24 * num_derivatives
        for cd in compiled_derivatives:
            combined_binary.write(struct.pack('=QQQ', cd.hash, data_start, len(cd.code)))
            data_start += len(cd.code)
        for cd in compiled_derivatives:
            combined_binary.write(cd.code)
    return 0