FOR %%A IN (*.vert) DO ..\Release\Parser.exe -vs -glsl %%A main GLSL\%%A
FOR %%A IN (*.frag) DO ..\Release\Parser.exe -fs -glsl %%A main GLSL\%%A

FOR %%A IN (*.vert) DO ..\Release\Parser.exe -vs -msl %%A main Metal\%%A.metal
FOR %%A IN (*.frag) DO ..\Release\Parser.exe -fs -msl %%A main Metal\%%A.metal

mkdir SPV
cd .\GLSL
FOR %%A IN (*) DO glslangValidator -V %%A -o ..\SPV\%%A
cd ..

REM mkdir DXIL

REM FOR %%A IN (*.vert) DO "C:\Program Files (x86)\Windows Kits\10\bin\10.0.18362.0\x64\dxc" /T vs_6_0 %%A /Fo DXIL\%%A
REM FOR %%A IN (*.frag) DO "C:\Program Files (x86)\Windows Kits\10\bin\10.0.18362.0\x64\dxc" /T ps_6_0 %%A /Fo DXIL\%%A

