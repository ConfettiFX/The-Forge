SETLOCAL

set INSTALLPATH=
set AdditionalForgeDefines=AUTOMATED_TESTING=1

if exist "%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /F "tokens=* USEBACKQ" %%F in (`"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -version 16.0 -property installationPath`) do set INSTALLPATH=%%F
)

set OUTPATH=WindowsDXBenchmarks

if not exist "%CD%\%OUTPATH%\" mkdir "%CD%\%OUTPATH%"

"%INSTALLPATH%\MSBuild\Current\Bin\MSBuild.exe" /p:Configuration=Release;Platform=x64 "../../PC Visual Studio 2019/Unit_Tests.sln" /target:Examples\16_Raytracing
start /d "../../PC Visual Studio 2019/x64/Release/16_Raytracing" 16_Raytracing.exe -f -b 512 -o "../../../../src/16_Raytracing/%OUTPATH%/" --d3d12

ENDLOCAL