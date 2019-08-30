#ifndef MCPP_PREPROC_H_
#define MCPP_PREPROC_H_

#include "../../../EASTL/vector.h"
#include "../../../EASTL/string.h"

namespace MCPPPreproc
{
	// macroLhs and macroRhs must be the same size
	bool FetchPreProcDataAsString(
		eastl::string & dstOut, // actual preprocessed file
		eastl::string & dstErr, // errors from mcpp
		eastl::string & dstDebug, // debug info from mcpp
		const eastl::string & fileName,
		const eastl::vector < eastl::string > & macroLhs,
		const eastl::vector < eastl::string > & macroRhs);
}

#endif

