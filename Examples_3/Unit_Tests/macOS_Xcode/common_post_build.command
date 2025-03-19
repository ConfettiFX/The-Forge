#! /bin/sh

src=$1;
srcroot=$2;
assets=$3;
dst=$4;
projName=$5;
isMac=$6;

#PathStatements
if [ -n "$isMac" ]; then
  rsync -r "${assets}/PathStatement.MacOS.txt" "${dst}/PathStatement.txt"
else
  rsync -r "${assets}/PathStatement.iOS.txt" "${dst}/PathStatement.txt"
fi

#Shaders
rsync -u -r "${srcroot}/../The-Forge/Shaders/" "${dst}/Shaders"
rsync -u -r "${srcroot}/../The-Forge/CompiledShaders/" "${dst}/CompiledShaders"

#GPU config
if [ -d "${src}/GPUCfg/" ]; then
  rsync -u -r "${src}/GPUCfg/" "${dst}/GPUCfg"
fi

mkdir -p "${dst}/InputActions/"
rsync -r "${srcroot}/../../../../Common_3/OS/Darwin/apple_gpu.data" "${dst}/gpu.data"

#Scripts
rsync -u -r "${srcroot}/../../../../Common_3/Scripts/" "${dst}/Scripts"
if [ -d "${src}/Scripts/" ]; then
  rsync -u -r "${src}/Scripts/" "${dst}/Scripts"
fi

if [ -n "$isMac" ]; then
  # we don't need copy the rest in macOS
  exit 0
fi

#circlepad
rsync -u "${assets}/Textures/ktx/circlepad.tex" "${dst}/Textures/"

#Fonts
rsync -u -r "${assets}/Fonts/" "${dst}/Fonts"


