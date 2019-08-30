#ifndef STRING_LIBRARY_H
#define STRING_LIBRARY_H


#include "../../../EASTL/vector.h"
#include "../../../EASTL/string.h"
#include "../../../EASTL/hash_set.h"

class StringLibrary
{
public:
	StringLibrary();

	void Reset();

	const char* InsertDirect(const eastl::string& value);

	bool HasString(const eastl::string & value) const;

	eastl::vector < eastl::string > GetFlatStrings() const;

private:

	eastl::hash_set<eastl::string> m_hashSet;
};

#endif


