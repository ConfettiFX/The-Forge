#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//minimal tool to embed src html into microprofile.h

char* ReadFile(const char* pFile)
{
	FILE* F = fopen(pFile, "r");

	if(!F) return 0;
	fseek(F, 0, SEEK_END);
	long size = ftell(F);
	char* pData = (char*)malloc(size + 1);
	fseek(F, 0, SEEK_SET);	
	size = fread(pData, 1, size, F);
	fclose(F);

	pData[size] = '\0';
	return pData;
}
void DumpFile(FILE* pOut, const char* pEmbedData, const char* pPrefix, const char* pSuffix)
{

	// fprintf(pOut, "const char %s_begin[] =\n\"", pSymbolArg);
	// DumpFile(pOut, pEmbedStart, pSymbolArg, "_begin");
	// fprintf(pOut, "\";\n\n");
	// fprintf(pOut, "const size_t %s_begin_size = sizeof(%s_begin);\n", pSymbolArg, pSymbolArg);


	size_t len = strlen(pEmbedData);
	int nNumBlocks = 0;
	while(len)
	{
		//split into 10k chunks.. because of visual studio..
		size_t nChunk = len > (32<<10) ? (32<<10) : len;
		len -= nChunk;
		fprintf(pOut, "const char %s%s_%d[] =\n\"", pPrefix, pSuffix, nNumBlocks);
		for(size_t i = 0; i < nChunk; ++i)
		{
			char c = *pEmbedData++;
			switch(c)
			{
				case '\n':
				#ifdef _WIN32
					fprintf(pOut, "\\n\"\n\""); 
				#else
					fprintf(pOut, "\\n\"\r\n\""); 
				#endif
					break;
				case '\\':
					fprintf(pOut, "\\"); 
					break;
				case '\"':
					fprintf(pOut, "\\\""); 
					break;
				case '\'':
					fprintf(pOut, "\\\'"); 
					break;
				default:
					fprintf(pOut, "%c", c);
			}
		}
		fprintf(pOut, "\";\n\n");
		fprintf(pOut, "const size_t %s%s_%d_size = sizeof(%s%s_%d);\n", pPrefix, pSuffix, nNumBlocks, pPrefix, pSuffix, nNumBlocks);
		nNumBlocks++;
	}
	fprintf(pOut, "const char* %s%s[] = {\n", pPrefix, pSuffix);
	for(int i = 0; i < nNumBlocks; ++i)
	{
		fprintf(pOut, "&%s%s_%d[0],\n", pPrefix, pSuffix, i);
	}
	fprintf(pOut, "};\n");

	fprintf(pOut, "size_t %s%s_sizes[] = {\n", pPrefix, pSuffix);
	for(int i = 0; i < nNumBlocks; ++i)
	{
		fprintf(pOut, "sizeof(%s%s_%d),\n", pPrefix, pSuffix, i);
	}
	fprintf(pOut, "};\n");
	fprintf(pOut, "size_t %s%s_count = %d;\n", pPrefix, pSuffix, nNumBlocks);
}


int main(int argc, char* argv[])
{
	if(6 != argc)
	{
		printf("Syntax: %s dest embedsource pattern symbol define\n", argv[0]);
		return 1;
	}
	const char* pDestArg = argv[1];
	const char* pEmbedSourceArg = argv[2];
	const char* pPatternArg = argv[3];
	const char* pSymbolArg = argv[4];
	const char* pDefineArg = argv[5];

	char* pEmbedSrc = ReadFile(pEmbedSourceArg);
	
	if(!pEmbedSrc)
	{
		return 1;
	}

	char* pEmbedStart = pEmbedSrc;
	char* pEmbedStartEnd = strstr(pEmbedStart, pPatternArg);
	char* pEmbedEnd = pEmbedStartEnd + strlen(pPatternArg);
	*pEmbedStartEnd = '\0';

	FILE* pOut = fopen(pDestArg, "w");
	if(!pOut)
	{
		free(pEmbedSrc);
		return 1;		
	}
	fprintf(pOut, "///start file generated from %s\n", pEmbedSourceArg);
	fprintf(pOut, "#ifdef %s\n", pDefineArg);
	DumpFile(pOut, pEmbedStart, pSymbolArg, "_begin");
	DumpFile(pOut, pEmbedEnd, pSymbolArg, "_end");
    fprintf(pOut, "#endif //%s\n", pDefineArg);
    fprintf(pOut, "\n///end file generated from  %s\n", pEmbedSourceArg);
	fclose(pOut);
	free(pEmbedSrc);

	return 0;
}