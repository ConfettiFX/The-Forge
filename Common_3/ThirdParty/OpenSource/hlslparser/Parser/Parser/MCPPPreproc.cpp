#include "MCPPPreproc.h"

#include "Engine.h"

extern "C"
{
	typedef enum {
		// OUT causes a conflict, so just rename it here
		OUT_0,                        /* ~= fp_out    */
		ERR,                        /* ~= fp_err    */
		DBG,                        /* ~= fp_debug  */
		NUM_OUTDEST
	} OUTDEST;

	void mcpp_use_mem_buffers(int tf);
	int mcpp_lib_main(int argc, char ** argv);
	char *  mcpp_get_mem_buffer(
		OUTDEST od);
}

bool MCPPPreproc::FetchPreProcDataAsString(
	eastl::string & dstOut, // actual preprocessed file
	eastl::string & dstErr, // errors from mcpp
	eastl::string & dstDebug, // debug info from mcpp
	const eastl::string & fileName,
	const eastl::vector < eastl::string > & macroLhs,
	const eastl::vector < eastl::string > & macroRhs)
{
	dstOut = "";
	dstErr = "";
	dstDebug = "";

	if (macroLhs.size() != macroRhs.size())
	{
		return false;
	}

	mcpp_use_mem_buffers(true);

	eastl::vector < eastl::string > strData;
	strData.push_back("mcpp.exe");
	strData.push_back(fileName);

	int numMacros = (int)macroLhs.size();
	ASSERT_PARSER(numMacros == (int)macroRhs.size());

	for (int i = 0; i < numMacros; i++)
	{
		eastl::string currMacro = "-D" + macroLhs[i];
		if (macroRhs[i].size() > 0)
		{
			currMacro += "=" + macroRhs[i];
		}
		strData.push_back(currMacro);
	}

	eastl::vector < const char *> argv;
	argv.resize(strData.size());
	for (int i = 0; i < strData.size(); i++)
	{
		argv[i] = strData[i].c_str();
	}

	int ret = mcpp_lib_main((int)argv.size(), (char **)argv.data());

	const char * pOut = mcpp_get_mem_buffer(OUT_0);
	const char * pErr = mcpp_get_mem_buffer(ERR);
	const char * pDbg = mcpp_get_mem_buffer(DBG);

	dstOut = (pOut == nullptr) ? "" : pOut;
	dstErr = (pErr == nullptr) ? "" : pErr;
	dstDebug = (pDbg == nullptr) ? "" : pDbg;

	// this seems to be the way to free the mem buffers?
	mcpp_use_mem_buffers(0);

	bool success = (ret == 0); // mcpp_lib_main returns IO_SUCCESS, which is hardcoded to 0, when everything goes well
	return success;
}
