REM Change active working directory in case we run script for outside of TheForge
cd /D "%~dp0"


echo Installing Shader Build Extension
"Tools/ShaderBuildCommand.vsix"

set filename=Art.zip

del %filename%

echo Pulling Art Assets
"Tools/wget" -O %filename% http://www.conffx.com/%filename%
"Tools/7z" x %filename% > NUL

del %filename%

exit 0