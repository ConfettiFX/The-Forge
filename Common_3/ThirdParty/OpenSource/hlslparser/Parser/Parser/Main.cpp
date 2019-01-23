#include "Parser.h"
#include <direct.h> // for _mkdir

std::string ReadFile(const char* fileName)
{
	std::ifstream ifs(fileName);
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	return buffer.str();
}

bool WriteFile(const char* fileName, const char* contents)
{
	//check first if there is right folder along the path of filename
	//And make directory	

	size_t found;
	std::string FilenameStr(fileName);
	found = FilenameStr.find_last_of("/\\");
	std::string DirnameStr = FilenameStr.substr(0, found);

	_mkdir(DirnameStr.c_str());



	std::ofstream ofs(fileName);
	
	ofs << contents;
	ofs.close();
	return true;
}


const char* getIncludeFiles(char newDirPath[], const char* pIncludedFileName[MAX_INCLUDE_FILE+1], int *pIncludedCounter, const char* pIncluded[MAX_INCLUDE_FILE], std::string originFile)
{	
	char toCstr[65536];
	strcpy(toCstr, originFile.c_str());

	size_t index = 0;

	while (true)
	{
		index = originFile.find("#include", index);

		if (std::string::npos == index)
		{
			break;
		}

		strcpy(toCstr, originFile.substr(index).c_str());

		//char* includeFile = std::strtok(toCstr, " \n\t");
		char* includeFile = std::strtok(toCstr, " \"<\n\t");
		includeFile = std::strtok(NULL, " \"<\n\t");


		char* newTempFilePath = new char[256];
		strcpy(newTempFilePath, newDirPath);
		strcat(newTempFilePath, includeFile);

		std::string newContents = ReadFile(newTempFilePath);

		if (newContents == "")
		{
			//can't find include files
			return includeFile;
		}


		char newDirPath[256];
		char drive[16];
		char dir[256];

		_splitpath_s(newTempFilePath,
			drive, sizeof(drive),
			dir, sizeof(dir),
			NULL, 0,             // Don't need filename
			NULL, 0);


		strcpy(newDirPath, drive);
		strcat(newDirPath, dir);
		

		//check first it also has Include files
		const char* RESULT = getIncludeFiles(newDirPath, pIncludedFileName, pIncludedCounter, pIncluded, newContents);
		if (RESULT)
		{
			//can't find include files
			return RESULT;
		}


		char* temp = new char[65536];

		strcpy(temp, newContents.data());


		pIncludedFileName[(*pIncludedCounter)] = newTempFilePath;
		pIncluded[(*pIncludedCounter)++] = temp;

		index += 8;
	}

	return NULL;
}


void removeIncludedFileNames(int includedCounter, const char* pIncludedFileName[MAX_INCLUDE_FILE + 1])
{
	// don't forget to delete includefile contents
	for (int i = 0; i < includedCounter + 1; i++)
	{
		delete[] pIncludedFileName[i];
	}
}

void removeIncludedFiles(int includedCounter, const char* pIncluded[MAX_INCLUDE_FILE])
{
	// don't forget to delete includefile contents
	for (int i = 0; i < includedCounter; i++)
	{
		delete[] pIncluded[i];
	}
}

int main( int argc, char* argv[] )
{
	//using namespace M4;

	// Parse arguments
	const char* fileName = NULL;
	const char* entryName = NULL;
	const char* shader = NULL;
	const char* _language = NULL;
	const char* outputFile = NULL;

	//for hull shader in Metal
	const char* secondaryfileName = NULL;
	const char* secondaryentryName = NULL;

	Target target = Target_VertexShader;
	Language language = Language_GLSL;

	Parser parser;


	

	if (String_Equal(argv[1], "-fs"))
	{
		shader = argv[1];
	}
	else if (String_Equal(argv[1], "-vs"))
	{
		shader = argv[1];
	}
	else if (String_Equal(argv[1], "-hs"))
	{
		shader = argv[1];
	}
	else if (String_Equal(argv[1], "-ds"))
	{
		shader = argv[1];
	}
	else if (String_Equal(argv[1], "-gs"))
	{
		shader = argv[1];
	}
	else if (String_Equal(argv[1], "-cs"))
	{
		shader = argv[1];
	}

	if (String_Equal(argv[2], "-glsl"))
	{
		_language = argv[2];
	}
	else if (String_Equal(argv[2], "-hlsl"))
	{
		_language = argv[2];
	}
	else if (String_Equal(argv[2], "-legacyhlsl"))
	{
		_language = argv[2];
	}
	else if (String_Equal(argv[2], "-msl"))
	{
		_language = argv[2];
	}


	if (fileName == NULL)
	{
		fileName = argv[3];
	}

	if (entryName == NULL)
	{
		entryName = argv[4];
	}
	
	if (argc >= 5 && outputFile == NULL)
	{
		outputFile = argv[5];
	}

	if (argc >= 6 && secondaryfileName == NULL)
	{
		secondaryfileName = argv[6];
	}

	if (argc >= 7 && secondaryentryName == NULL)
	{
		secondaryentryName = argv[7];
	}




	// Read input file
	std::string source = ReadFile(fileName);
	
	if (secondaryfileName)
	{
		std::string source2 = ReadFile(secondaryfileName);
		source = source2.append(source);
	}
	


	// Read included files
	
	char newDirPath[256];
	char drive[16];
	char dir[256];

	_splitpath_s(fileName,
		drive, sizeof(drive),
		dir, sizeof(dir),    
		NULL, 0,             // Don't need filename
		NULL, 0);


	strcpy(newDirPath, drive);
	strcat(newDirPath, dir);


	const char* pIncludedFileName[MAX_INCLUDE_FILE + 1];
	const char* pIncluded[MAX_INCLUDE_FILE];
	int includedCounter = 0;

	
	char toCstr[65536];
	strcpy(toCstr, source.c_str());

	size_t index = 0;
	
	char RESULT[65536];

	const char* InlcudedResult = getIncludeFiles(newDirPath, pIncludedFileName, &includedCounter, pIncluded, source);
	if (InlcudedResult)
	{
		strcpy(RESULT,"error) cannot find an include file ");
		strcat(RESULT, InlcudedResult);
	}
	else
	{
		char* temp = new char[256];
		strcpy(temp, fileName);
		pIncludedFileName[includedCounter] = temp;
		parser.ParserEntry(RESULT, pIncludedFileName, source.data(), source.size(), entryName, shader, _language, pIncluded, includedCounter);
	}

	removeIncludedFiles(includedCounter, pIncluded);

	/*

	char errorCheck[7];
	errorCheck[0] = RESULT[0];
	errorCheck[1] = RESULT[1];
	errorCheck[2] = RESULT[2];
	errorCheck[3] = RESULT[3];
	errorCheck[4] = RESULT[4];
	errorCheck[5] = RESULT[5];
	errorCheck[6] = NULL;

	*/

	//if (String_Equal(errorCheck, "error)"))
	//	return -1;
	//else
	//{	
		//const char* header = "/*\n * Copyright (c) 2018-2019 Confetti Interactive Inc.\n * \n * This file is part of The-Forge\n * (see https://github.com/ConfettiFX/The-Forge). \n *\n * Licensed to the Apache Software Foundation (ASF) under one\n * or more contributor license agreements.  See the NOTICE file\n * distributed with this work for additional information\n * regarding copyright ownership.  The ASF licenses this file\n * to you under the Apache License, Version 2.0 (the\n * \"License\") you may not use this file except in compliance\n * with the License.  You may obtain a copy of the License at\n *\n *   http://www.apache.org/licenses/LICENSE-2.0\n *\n * Unless required by applicable law or agreed to in writing,\n * software distributed under the License is distributed on an\n * \"AS IS\" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY\n * KIND, either express or implied.  See the License for the\n * specific language governing permissions and limitations\n * under the License.\n*/\n";
		
		char RESULT2[65536];
		//strcpy(RESULT2, header);
		//strcat(RESULT2, RESULT);
		strcpy(RESULT2, RESULT);

		if(outputFile)
			WriteFile(outputFile, RESULT2);
		
		return 0;
	//}	
}
