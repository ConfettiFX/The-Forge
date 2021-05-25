#!/bin/sh

codelite-make -w "../../UbuntuCodelite/UbuntuUnitTests.workspace" -p 16_Raytracing -c Release -e

mkdir -p "LinuxVkBenchmarks"

../../UbuntuCodelite/16_Raytracing/Release/16_Raytracing -b 512 -o "../../../src/16_Raytracing/LinuxVkBenchmarks/"