#!/bin/bash

# Change active working directory in case we run script for outside of TheForge
cd "$(dirname "$0")"

filename=Art.zip

rm $filename

curl -L -o $filename http://www.conffx.com/$filename
unzip -o $filename

# rsync --remove-source-files -a "./Art/PBR/" "./Examples_3/Unit_Tests/UnitTestResources/Textures/PBR"
if [ -d "Art/ZipFilesDds" ]; then
	mv -f "Art/ZipFilesDds" "Examples_3/Unit_Tests/UnitTestResources"
fi
if [ -d "Art/ZipFilesKtx" ]; then
	mv -f "Art/ZipFilesKtx" "Examples_3/Unit_Tests/UnitTestResources"
fi

rm $filename

exit 0
