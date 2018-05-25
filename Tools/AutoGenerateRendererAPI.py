import re, os

include_dir = os.path.dirname(os.path.realpath(__file__)) + "/../Common_3/Renderer/"
header_file_loc = include_dir + "IRendererDLL.h"
source_file_loc = include_dir + "Renderer.cpp"
os.chdir(include_dir)

functionDeclaration = "API_INTERFACE"

message = ("/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n" 
                "\tThis file was auto-generated to contain all exported functions in The Forge.\n"
                "\tAll functions containing 'API_INTERFACE' and are located in the 'Renderer' directory\n"
                "\tare added automatically upon compilation.\n"
                "\n"
                 "\tScript is located at TheForge/Tools/AutoGenerateRendererAPI.py\n"
                 "\n"
                 "\tNote : All manual changes to these files will be overwritten\n"
                 "\tIf a function that you want is missing, please make sure the header file in which it is\n"
                 "\tdeclared is located in the 'Renderer' directory and that you re-build The Forge.\n"
                 "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */\n\n")

ignore_functions = [ "addShader" ]
renderer_apis = [ [ "d3d12", "DIRECT3D12", "RENDERER_API_D3D12", [] ],
                  [ "vk", "VULKAN", "RENDERER_API_VULKAN", [ "compileShader" ] ],
                  [ "mtl", "METAL", "RENDERER_API_METAL", [ "compileShader" ] ] ]
file_list = []
function_names = []
pattern = functionDeclaration + " ([A-Za-z0-9:_ ]*) CALLTYPE ([A-Za-z0-9_]*)\(([A-Za-z0-9:_, \*\(\)]*)\);"
regex = re.compile(pattern)

# Get list of all files in the Include directory
for path, subdirs, files in os.walk(include_dir):
    for name in files:
        file_list.append(os.path.join(path, name))

header_file = open(header_file_loc, "w")
source_file = open(source_file_loc, "w")

header_file.write("#pragma once\n\n")
header_file.write(message)

source_file.write(message)
source_file.write("#include \"IRenderer.h\"\n")
source_file.write("#include \"../../Common_3/OS/Interfaces/ILogManager.h\"\n")
source_file.write("\n")

def function_valid(functionName):
    return functionName not in ignore_functions and functionName not in function_names

for file in file_list:
    with open(file) as f:
        for line in f:
            result = regex.search(line)
            if (result and function_valid(result.group(2))):
                functionName = result.group(2)
                functionPointer = result.group(1) + "(*" + result.group(2) + ")" + "(" + result.group(3) + ");\n"
                functionDeclaration = result.group(1) + " " + functionName + "(" + result.group(3) + ");"
                function_names.append(functionName)
                header_file.write("extern " + functionPointer)
                source_file.write(functionPointer)
                for api in renderer_apis:
                    if (functionName not in api[3]):
                        source_file.write("namespace " + api[0] + " { extern " + functionDeclaration + " } \n")
                source_file.write("\n")

load_function_declaration = "bool loadRendererFunctions(RendererApi api)"
unload_function_declaration = "void unloadRendererFunctions()"

unload_function = (
unload_function_declaration + "\n"
"{\n"
"}\n")

header_file.write("\n")
header_file.write("extern " + load_function_declaration + ";\n")
header_file.write("extern " + unload_function_declaration + ";\n")

source_file.write(load_function_declaration)
source_file.write("\n")
source_file.write("{\n")
for api in renderer_apis:
    source_file.write("#if defined(" + api[1] + ")\n")
    source_file.write("\tif (api == " + api[2] + ")\n")
    source_file.write("\t{\n")
    for function in function_names:
        line = "\t\t" + function + " = " + api[0] + "::" + function + ";\n"
        if (function not in api[3]):
            source_file.write(line)
    source_file.write("\t\treturn true;\n")
    source_file.write("\t}\n")
    source_file.write("#endif\n")

source_file.write("\n")
source_file.write("\treturn false;\n")
source_file.write("}\n")
source_file.write("\n")

source_file.write(unload_function)

header_file.close()
source_file.close()
