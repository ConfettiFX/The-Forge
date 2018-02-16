echo Installing Shader Build Extension
"Tools/ShaderBuildCommand.vsix"

set filename=Art.zip

del %filename%

echo Pulling Art Assets
"Tools/wget" -O %filename% http://www.conffx.com/%filename%
"Tools/7z" x %filename% > NUL

del %filename%
