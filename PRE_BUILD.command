#!/bin/bash

# Change active working directory in case we run script for outside of TheForge
cd "$(dirname "$0")"

filename=Art.zip

rm $filename

curl -L -o $filename http://www.conffx.com/$filename
unzip -o $filename

# rsync --remove-source-files -a "./Art/PBR/" "./Examples_3/Unit_Tests/UnitTestResources/Textures/PBR"

rm $filename

exit 0
