@echo off


if "%1"=="" (
set directory="%~dp0..\..\"
) else (
set directory=%1 
)

:: Call from root of the forge with no extra arguments
:: Call from Scripts folder without extra Arguments
:: Call from anywhere with the root of the forge as the argument 
set ExcludedFolders="ThirdParty\ CommonRaytracing_3\ThirdParty\ Scripts\ Common_3\Tools\SpirvTools\ AutoGenerateRendererAPI Shaders"
set extensions=*.h *.hpp *.c *.cpp *.m *.mm

:: Version of clang-format executable. Download from here:
:: https://llvm.org/builds/
set ClangFormatVersion=r351376

:: Full path of Clang-format executable including version
set ClangFormatExe=.\Tools\ClangFormatter\clang-format-%ClangFormatVersion%.exe

:: Full command used to format individual files. epeccts the desired file as argument
set ClangformatterCommand= %ClangFormatExe% -i -style=file

pushd %directory%
echo "Executing '%ClangformatterCommand%' on all files with following extensions: '%extensions%' and following Dirs excluded '%ExcludedFolders%"'

for /R %directory% %%f in (%extensions%) do (
	::ECHO %%f | FIND /I "Common_3/ThirdParty">Nul || (
	ECHO "%%f"| findstr /v %ExcludedFolders%>NUL
	IF NOT ERRORLEVEL 1 %ClangformatterCommand% "%%f"
)

echo "Outputing summary of changes due to clang-fomat"
git diff --shortstat
popd

exit /b 0 
