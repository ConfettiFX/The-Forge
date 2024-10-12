@echo off

rem "$(SolutionDir)..\src\$(ProjectName)\compile_materials.bat" "$(SolutionDir)..\..\..\" "$(ProjectDir)..\src\$(ProjectName)\Materials" "$(OutDir)CompiledMaterials"

set TheForgeRoot=%~1
set input_dir=%~2
set output_dir=%~3

set PythonPath="%TheForgeRoot%\Tools\python-3.6.0-embed-amd64\python.exe"
set ForgeMaterialCompiler="%TheForgeRoot%\Common_3\Tools\ForgeMaterialCompiler\forge_material_compiler.py"
%PythonPath% %ForgeMaterialCompiler% -d "%input_dir%" -o "%output_dir%" --verbose

