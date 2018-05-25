#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <filesystem>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

using String = std::string;

template<typename T>
using Vector = std::vector<T>;

using Regex = std::regex;

namespace fs = std::experimental::filesystem;

struct RendererAPI
{
	String mNamespace;
	String mDefine;
	String mName;
	Vector<String> mIgnoreFunctions;
};

int main(int argc, char** argv)
{
	String include_dir;
#ifdef _WIN32
	if (IsDebuggerPresent())
		include_dir = fs::current_path().parent_path().string() + "/../Common_3/Renderer/";
	else
#endif
		include_dir = fs::path(argv[0]).parent_path().parent_path().string() +"/Common_3/Renderer/";

	std::cout << "Scanning renderer files in " << include_dir;

	String header_file_loc = include_dir + "IRendererDLL.h";
	String source_file_loc = include_dir + "Renderer.cpp";

	system(String(String("cd ") + include_dir).c_str());

	String functionDeclaration = "API_INTERFACE";

	String message = ("/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n"
		"\tThis file was auto-generated to contain all exported functions in The Forge.\n"
		"\tAll functions containing 'API_INTERFACE' and are located in the 'Renderer' directory\n"
		"\tare added automatically upon compilation.\n"
		"\n"
		"\tScript is located at TheForge/Tools/AutoGenerateRendererAPI.py\n"
		"\n"
		"\tNote : All manual changes to these files will be overwritten\n"
		"\tIf a function that you want is missing, please make sure the header file in which it is\n"
		"\tdeclared is located in the 'Renderer' directory and that you re-build The Forge.\n"
		"* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */\n\n");

	auto ignore_functions = { "addShader" };
	Vector<RendererAPI> renderer_apis = {
		{ "d3d12", "DIRECT3D12", "RENDERER_API_D3D12", {} },
		{ "vk", "VULKAN", "RENDERER_API_VULKAN", {"compileShader"} },
		{ "mtl", "METAL", "RENDERER_API_METAL", {"compileShader" } }
	};
	Vector<String> file_list = {};
	Vector<String> function_names = {};
	String pattern = functionDeclaration + " ([A-Za-z0-9:_ ]*) CALLTYPE ([A-Za-z0-9_]*)\\(([A-Za-z0-9:_, \\*\\(\\)]*)\\);";
	Regex regex = Regex(pattern);

	//// Get list of all files in the Include directory
	for (auto& p : fs::recursive_directory_iterator(include_dir))
	{
		if (!is_directory(p.path()))
		{
			file_list.emplace_back(p.path().string());
		}
	}

	std::ofstream header_file(header_file_loc);
	std::ofstream source_file(source_file_loc);

	header_file << "#pragma once\n\n";
	header_file << message;

	source_file << (message);
	source_file << ("#include \"IRenderer.h\"\n");
	source_file << ("#include \"../../Common_3/OS/Interfaces/ILogManager.h\"\n");
	source_file << ("\n");

	auto function_valid = [&ignore_functions, &function_names](String functionName) {
		return std::find(ignore_functions.begin(), ignore_functions.end(), functionName) == ignore_functions.end() &&
			std::find(function_names.begin(), function_names.end(), functionName) == function_names.end();
	};

	for (auto& file : file_list)
	{
		std::ifstream f(file);
		while (!f.eof())
		{
			String line;
			std::smatch result;
			getline(f, line);
			if (regex_search(line, result, regex) && function_valid(result[2]))
			{
				String functionName = result[2];
				String functionPointer = String(result[1]) + String("(*") + functionName + ")" + "(" + String(result[3]) + ");\n";
				String functionDeclaration = String(result[1]) + " " + functionName + "(" + String(result[3]) + ");";
				function_names.emplace_back(functionName);
				header_file << ("extern " + functionPointer);
				source_file << (functionPointer);

				for (auto& api : renderer_apis)
				{
					if (std::find(api.mIgnoreFunctions.begin(), api.mIgnoreFunctions.end(), functionName) == api.mIgnoreFunctions.end())
					{
						source_file << ("namespace " + api.mNamespace + " { extern " + functionDeclaration + " } \n");
					}
				}

				source_file << ("\n");
			}
		}
	}

	String load_function_declaration = "bool loadRendererFunctions(RendererApi api)";
	String unload_function_declaration = "void unloadRendererFunctions()";

	String unload_function = (
		unload_function_declaration + "\n"
		"{\n"
		"}\n");

	header_file << ("\n");
	header_file << ("extern " + load_function_declaration + ";\n");
	header_file << ("extern " + unload_function_declaration + ";\n");

	source_file << (load_function_declaration);
	source_file << ("\n");
	source_file << ("{\n");
	for (auto& api : renderer_apis)
	{
		source_file << ("#if defined(" + api.mDefine + ")\n");
		source_file << ("\tif (api == " + api.mName + ")\n");
		source_file << ("\t{\n");
		for (auto& function : function_names)
		{
			if (std::find(api.mIgnoreFunctions.begin(), api.mIgnoreFunctions.end(), function) == api.mIgnoreFunctions.end())
			{
				String line = "\t\t" + function + " = " + api.mNamespace + "::" + function + ";\n";
				source_file << (line);
			}
		}

		source_file << ("\t\treturn true;\n");
		source_file << ("\t}\n");
		source_file << ("#endif\n");
	}

	source_file << ("\n");
	source_file << ("\treturn false;\n");
	source_file << ("}\n");
	source_file << ("\n");

	source_file << (unload_function);

	header_file.close();
	source_file.close();

	return 0;
}