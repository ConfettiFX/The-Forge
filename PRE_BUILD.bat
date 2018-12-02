:: ENTRY POINT
::
@echo off
@SETLOCAL EnableDelayedExpansion

:: Change active working directory in case we run script for outside of TheForge
cd /D "%~dp0"

:: Install Shader Build Command extension if it's not installed
call :CheckIfExtensionInstalled "ShaderBuildCommand"
set ShaderBuildCommandInstalled=%errorlevel%

if %ShaderBuildCommandInstalled% EQU 0 (
    echo Installing Shader Build Extension
    "Tools/ShaderBuildCommand.vsix"
) else (
    echo Shader Build Extension Already Installed.
)


set filename=Art.zip
if exist %filename% (
    del %filename%
)

echo Pulling Art Assets
"Tools/wget" -O %filename% http://www.conffx.com/%filename%

echo Unzipping Art Assets...
"Tools/7z" x %filename% -y > NUL

echo Finishing up...
del %filename%

exit /b 0

:: Goes through non-admin Extension directory of all visual studio versions 
:: which live in %LOCALAPPDATA% - admin ones live in %ProgramFiles% - and 
:: checks for the extension files: returns 1 if they exist, 0 if not
::
:CheckIfExtensionInstalled
set VS_Extension_Root_Dir=%LOCALAPPDATA%\Microsoft\VisualStudio\

:: get the vs extension name, strip it from quotes
set Extension_Name=%~1

::---Debug
::echo Checking if Extension installed: %Extension_Name%
::---Debug


:: change dir to extension root dir and search for the Extension_Name
:: passed as the first parameter in %1
pushd "%VS_Extension_Root_Dir%"

set /A NumFilesFound=0
for /R %%G in ("*%Extension_Name%.pkgdef") do ( 
    ::---Debug
    ::@echo "%%G" 
    ::---Debug
    set /A NumFilesFound=%NumFilesFound% + 1
)
popd

::---Debug
::echo NumFilesFound=%NumFilesFound%
::---Debug

if %NumFilesFound% EQU 0 (
    exit /b 0
) else (
    exit /b 1
)

exit /b 0

EXIT /B %RETURN_CODE%
