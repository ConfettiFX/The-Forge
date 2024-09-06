#! /bin/sh

src=$1;
srcroot=$2;
assets=$3;
dst=$4;
projName=$5;

#Shaders
rsync -u -r "${srcroot}/../The-Forge/Shaders/" "${dst}/Shaders"
rsync -u -r "${srcroot}/../The-Forge/CompiledShaders/" "${dst}/CompiledShaders"

#GPU config
if [ -d "${assets}/GPUCfg/${projName}/" ]; then
  rsync -u -r "${assets}/GPUCfg/${projName}/" "${dst}/GPUCfg"
fi
mkdir -p "${dst}/InputActions/"
rsync -r "${srcroot}/../../../../Common_3/OS/Darwin/apple_gpu.data" "${dst}/gpu.data"

#Fonts
rsync -u -r "${assets}/Fonts/" "${dst}/Fonts"

#Scripts
mkdir -p "${dst}/Scripts"
rsync -u -r --exclude='*/' "${assets}/Scripts/" "${dst}/Scripts"
if [ -d "${assets}/Scripts/${projName}/" ]; then
	rsync -u -r "${assets}/Scripts/${projName}/" "${dst}/Scripts/${projName}"
fi
if [ -d "${src}/Scripts/" ]; then
  rsync -u -r "${src}/Scripts/" "${dst}/Scripts"
fi
