#!/bin/sh

OldTfConfig=`cat ../../../../Common_3/Application/Config.h`
echo '#define AUTOMATED_TESTING 1' > ../../../../Common_3/Application/Config.h
echo "$OldTfConfig" >> ../../../../Common_3/Application/Config.h

codelite-make -w "../../UbuntuCodelite/UbuntuUnitTests.workspace" -p 16_Raytracing -c Release -e

mkdir -p "LinuxVkBenchmarks"
VK_LAYER_PATH=../../../../Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Linux/
LD_LIBRARY_PATH=../../../../Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Linux/:$LD_LIBRARY_PATH

../../UbuntuCodelite/16_Raytracing/Release/16_Raytracing -b 512 -o "../../../src/16_Raytracing/LinuxVkBenchmarks/"

echo "$OldTfConfig" > ../../../../Common_3/Application/Config.h