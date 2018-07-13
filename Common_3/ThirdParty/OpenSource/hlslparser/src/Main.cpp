#include "Parser.h"

enum Target
{
	Target_VertexShader,
	Target_FragmentShader,
	Target_HullShader,
	Target_DomainShader,
	Target_GeometryShader,
	Target_ComputeShader,
};

enum Language
{
	Language_GLSL,
	Language_HLSL,
	Language_LegacyHLSL,
	Language_Metal,
};

std::string ReadFile(const char* fileName)
{
	std::ifstream ifs(fileName);
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	return buffer.str();
}

int main( int argc, char* argv[] )
{
	//using namespace M4;

	// Parse arguments
	const char* fileName = NULL;
	const char* entryName = NULL;
	const char* shader = NULL;
	const char* _language = NULL;

	Target target = Target_VertexShader;
	Language language = Language_GLSL;

	Parser parser;

	for( int argn = 1; argn < argc; ++argn )
	{
		const char* const arg = argv[ argn ];

		if( String_Equal( arg, "-h" ) || String_Equal( arg, "--help" ) )
		{
			return 0;
		}
		else if( String_Equal( arg, "-fs" ) )
		{
			shader = arg;
		}
		else if( String_Equal( arg, "-vs" ) )
		{
			shader = arg;
		}
		else if (String_Equal(arg, "-hs"))
		{
			shader = arg;
		}
		else if (String_Equal(arg, "-ds"))
		{
			shader = arg;
		}
		else if (String_Equal(arg, "-gs"))
		{
			shader = arg;
		}
		else if (String_Equal(arg, "-cs"))
		{
			shader = arg;
		}
		else if( String_Equal( arg, "-glsl" ) )
		{
			_language = arg;
		}
		else if( String_Equal( arg, "-hlsl" ) )
		{
			_language = arg;
		}
		else if( String_Equal( arg, "-legacyhlsl" ) )
		{
			_language = arg;
		}
		else if( String_Equal( arg, "-metal" ) )
		{
			_language = arg;
		}
		else if( fileName == NULL )
		{
			fileName = arg;
		}
		else if( entryName == NULL )
		{
			entryName = arg;
		}
		else
		{
			Log_Error( "Too many arguments\n" );
			return 1;
		}
	}


	// Read input file
	const std::string source = ReadFile(fileName);

	parser.ParserEntry(fileName, source.data(), source.size(), entryName, shader, _language);


	return 0;
}
