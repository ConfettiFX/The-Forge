import re, os

include_dir = os.path.dirname(os.path.realpath(__file__)) + "/../Common_3/Renderer/"
header_file_loc = include_dir + "IRendererDLL.h"
source_file_loc = include_dir + "RendererDLL.cpp"
os.chdir(include_dir)

functionDeclaration = "API_INTERFACE"

message = ("/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n" 
                "\tThis file was auto-generated to contain all exported functions in The Forge.\n"
                "\tAll functions containing 'API_INTERFACE' and are located in the 'Renderer' directory\n"
                "\tare added automatically upon compilation.\n"
                "\n"
                 "\tScript is located at TheForge/Tools/ExportRendererDLLFunctions.py\n"
                 "\n"
                 "\tNote : All manual changes to these files will be overwritten\n"
                 "\tIf a function that you want is missing, please make sure the header file in which it is\n"
                 "\tdeclared is located in the 'Renderer' directory and that you re-build The Forge.\n"
                 "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */\n\n")

ignore_functions = [ "addShader" ]
optional_functions = [ "compileShader" ]
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
                function_names.append(functionName)
                header_file.write("extern " + functionPointer)
                source_file.write(functionPointer)

load_function_declaration = "bool loadRendererFunctions(const char* pDllName)"
unload_function_declaration = "void unloadRendererFunctions()"

load_function = (
load_function_declaration + "\n"
"{\n"
"\tpDLL = LoadLibraryA(pDllName);\n"
"\tif (!pDLL)\n"
"{\n"
"\t\tASSERT(false && String::format(\"Failed to load dll %s\", pDllName).c_str());\n"
"\t\treturn false;\n"
"\t}\n")

unload_function = (
unload_function_declaration + "\n"
"{\n"
"\tFreeLibrary(pDLL);\n"
"}\n")

header_file.write("\n")
header_file.write("extern " + load_function_declaration + ";\n")
header_file.write("extern " + unload_function_declaration + ";\n")

source_file.write("\n")
source_file.write("// #TODO: Implement DLL Loading for other platforms if needeed\n")
source_file.write("#ifdef _WIN32\n")
source_file.write("static HINSTANCE pDLL = NULL;\n")

source_file.write("\n")
source_file.write(load_function)
source_file.write("\n")
for function in function_names:
    line = "\t" + function + " = (decltype(" + function + "))GetProcAddress(pDLL, \"" + function + "\");\n"
    validation = (
        "\tif (!" + function + ")\n"
        "\t{\n"
        "\t\tASSERT(false && \"" + function + " not implemented\");\n"
        "\t\treturn false;\n"
        "\t}\n")
    source_file.write(line)
    if (function not in optional_functions):
        source_file.write(validation)

source_file.write("\n")
source_file.write("\treturn true;\n")
source_file.write("}\n")
source_file.write("\n")

source_file.write(unload_function)
source_file.write("#endif\n")

header_file.close()
source_file.close()
