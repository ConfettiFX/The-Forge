#! /bin/sh

src=$1;
srcroot=$2;
assets=$3;
dst=$4;

#Shaders
rsync -u -r "${srcroot}/../The-Forge/Shaders/" "${dst}/Shaders"
rsync -u -r "${srcroot}/../The-Forge/CompiledShaders/" "${dst}/CompiledShaders"

#GPU config
rsync -u -r "${src}/GPUCfg/" "${dst}/GPUCfg"
rsync -r "${srcroot}/../../../../Common_3/OS/Darwin/apple_gpu.data" "${dst}/GPUCfg/gpu.data"

#Fonts
rsync -u -r "${assets}/Fonts/" "${dst}/Fonts"

#Scripts
rsync -u -r "${assets}/Scripts/" "${dst}/Scripts"
if [ -d "${src}/Scripts/" ]; then
  rsync -u -r "${src}/Scripts/" "${dst}/Scripts"
fi
