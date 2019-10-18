@echo off

if "%1"=="" (
set root_directory="%~dp0..\..\"
) else (
set root_directory=%1 
)

:: Call from root of the forge with no extra arguments
:: Call from Scripts folder without extra Arguments
:: Call from anywhere with the root of the forge as the argument 
set ExcludedFolders="ThirdParty\ CommonRaytracing_3\ThirdParty\ Tools\ Scripts\ Common_3\Tools\SpirvTools\ AutoGenerateRendererAPI Shaders"
set extensions=*.h *.hpp *.c *.cpp *.m *.mm

:: Version of clang-format executable. Download from here:
:: https://llvm.org/builds/
set ClangFormatVersion=r351376

:: Full path of Clang-format executable including version
set ClangFormatExe=.\Tools\ClangFormatter\clang-format-%ClangFormatVersion%.exe

:: Copy .clang-format as it needs to be in parent dir
copy %~dp0.clang-format %root_directory%

:: Full command used to format individual files.
set ClangformatterCommand= %ClangFormatExe% -i -style=file -fallback-style=none

pushd %root_directory%
echo "Executing '%ClangformatterCommand%' on all files with following extensions: '%extensions%' and following Dirs excluded '%ExcludedFolders%'"

for /R %root_directory% %%f in (%extensions%) do (	
	ECHO "%%f"| findstr /v %ExcludedFolders%>NUL
	IF NOT ERRORLEVEL 1 %ClangformatterCommand% "%%f"
)
popd

:: Delete copy of .clang-format 
DEL %root_directory%.clang-format

pushd %root_directory%
echo "Outputing summary of changes due to clang-fomat"
git diff --shortstat
popd

exit /b 0 
