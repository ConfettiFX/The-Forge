#!/bin/bash

THE_FORGE_ROOT=$1
INPUT_DIR=$2
OUTPUT_DIR=$3

EXPECTED_PARAM_NUM=3
if [ $# != $EXPECTED_PARAM_NUM ]
then
echo "compile_materials.sh: Error: Invalid number of parameters. Expected " $EXPECTED_PARAM_NUM " got " $#
exit
fi

FORGE_MATERIAL_COMPILER="$THE_FORGE_ROOT/Common_3/Tools/ForgeMaterialCompiler/forge_material_compiler.py"
python3 "$FORGE_MATERIAL_COMPILER" -d "$INPUT_DIR" -o "$OUTPUT_DIR" --verbose

