FOR %%A IN (*.vert) DO ..\Release\Parser.exe -vs -hlsl %%A -E main -Fo HLSL\%%A
FOR %%A IN (*.geom) DO ..\Release\Parser.exe -gs -hlsl %%A -E main -Fo HLSL\%%A
FOR %%A IN (*.frag) DO ..\Release\Parser.exe -fs -hlsl %%A -E main -Fo HLSL\%%A
FOR %%A IN (*.comp) DO ..\Release\Parser.exe -cs -hlsl %%A -E main -Fo HLSL\%%A

FOR %%A IN (*.vert) DO ..\Release\Parser.exe -vs -glsl %%A -E main -Fo GLSL\%%A -fvk-s-shift 8 0 -fvk-t-shift 16 0 -fvk-u-shift 24 0
FOR %%A IN (*.geom) DO ..\Release\Parser.exe -gs -glsl %%A -E main -Fo GLSL\%%A -fvk-s-shift 8 0 -fvk-t-shift 16 0 -fvk-u-shift 24 0
FOR %%A IN (*.frag) DO ..\Release\Parser.exe -fs -glsl %%A -E main -Fo GLSL\%%A -fvk-s-shift 8 0 -fvk-t-shift 16 0 -fvk-u-shift 24 0
FOR %%A IN (*.comp) DO ..\Release\Parser.exe -cs -glsl %%A -E main -Fo GLSL\%%A -fvk-s-shift 8 0 -fvk-t-shift 16 0 -fvk-u-shift 24 0

FOR %%A IN (*.vert) DO ..\Release\Parser.exe -vs -msl %%A -E main -Fo Metal\%%A.metal -fvk-s-shift 8 0 -fvk-t-shift 16 0 -fvk-u-shift 24 0
FOR %%A IN (*.geom) DO ..\Release\Parser.exe -gs -msl %%A -E main -Fo Metal\%%A.metal -fvk-s-shift 8 0 -fvk-t-shift 16 0 -fvk-u-shift 24 0
FOR %%A IN (*.frag) DO ..\Release\Parser.exe -fs -msl %%A -E main -Fo Metal\%%A.metal -fvk-s-shift 8 0 -fvk-t-shift 16 0 -fvk-u-shift 24 0
FOR %%A IN (*.comp) DO ..\Release\Parser.exe -cs -msl %%A -E main -Fo Metal\%%A.metal -fvk-s-shift 8 0 -fvk-t-shift 16 0 -fvk-u-shift 24 0

mkdir SPV
FOR %%A IN (GLSL\*) DO glslangValidator -V %%A -o SPV\%%~nxA

mkdir DXIL
FOR %%A IN (HLSL\*.vert) DO dxc /T vs_6_0 %%A /Fo DXIL\%%~nxA
FOR %%A IN (HLSL\*.geom) DO dxc /T gs_6_0 %%A /Fo DXIL\%%~nxA
FOR %%A IN (HLSL\*.frag) DO dxc /T ps_6_0 %%A /Fo DXIL\%%~nxA
FOR %%A IN (HLSL\*.comp) DO dxc /T cs_6_0 %%A /Fo DXIL\%%~nxA

REM FOR %%A IN (*.vert) DO ..\Release\Parser.exe -vs -orbis %%A -E main -Fo Orbis\%%A
REM FOR %%A IN (*.frag) DO ..\Release\Parser.exe -fs -orbis %%A -E main -Fo Orbis\%%A