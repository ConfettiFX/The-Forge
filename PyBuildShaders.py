"""
script which generates and compiles all shaders
"""
def BuildShaders() -> int:
	import os, sys
	_filedir = os.path.abspath(os.path.dirname(__file__))
	# directly import fsl as module, calling the tool directly is much faster that way than spawning a process for each invocation
	sys.path.append(os.path.join(_filedir, 'Common_3', 'Tools', 'ForgeShadingLanguage'))
	from fsl import main
	from compilers import get_available_compilers

	# set default compiler paths
	os.environ['FSL_COMPILER_FXC'] = os.path.normpath('C:/Program Files (x86)/Windows Kits/8.1/bin/x64/fxc.exe')
	os.environ['FSL_COMPILER_DXC'] = os.path.normpath(_filedir+'/Common_3/Graphics/ThirdParty/OpenSource/DirectXShaderCompiler/bin/x64/dxc.exe')
	os.environ['FSL_COMPILER_METAL'] = os.path.normpath('C:/Program Files/Metal Developer Tools/macos/bin/metal.exe')
	available_compilers = get_available_compilers()

	shader_dirs = [     
		'Examples_3/Visibility_Buffer/src/Shaders/FSL',
		'Examples_3/Unit_Tests/src/01_Transformations/Shaders/FSL/',
		'Examples_3/Unit_Tests/src/02_Compute/Shaders/FSL/',
		'Examples_3/Unit_Tests/src/03_MultiThread/Shaders/FSL/',
		'Examples_3/Unit_Tests/src/04_ExecuteIndirect/Shaders/FSL/',
	]

	output_dir = 'GeneratedShaders'
	if not os.path.exists(output_dir):
		os.makedirs(output_dir)

	# sys.argv += ['--verbose', 'true']
	# sys.argv += ['--compile', 'true']
	argv = sys.argv[:]

	languages = ['D3D12', 'Metal', 'Vulkan']

	for lang in languages:
		for shader_dir in shader_dirs:
			project = os.path.normpath(shader_dir).split(os.path.sep)[-3]
			shader_dir = os.path.join(_filedir, shader_dir)
			for fsl_file in [os.path.join(shader_dir, _file) for _file in os.listdir(shader_dir) if _file.endswith('.fsl')]:

				if lang != 'Metal' and 'icb.comp' in fsl_file: continue


				sys.argv = argv[:]
				if lang in available_compilers: sys.argv += ['--compile', 'true']
				sys.argv += ['-l', lang]
				sys.argv += ['-d', os.path.join(output_dir, project, lang)]
				sys.argv += [fsl_file]

				status = main()
				if status != 0: return status
	return 0

if __name__ == '__main__':
	BuildShaders()