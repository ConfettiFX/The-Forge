#include "StringLibrary.h"


#include <assert.h>

StringLibrary::StringLibrary()
{
}

void StringLibrary::Reset()
{
	m_hashSet.clear();
}

const char* StringLibrary::InsertDirect(const eastl::string& value)
{
	return m_hashSet.insert(value).first->c_str();
}

bool StringLibrary::HasString(const eastl::string & value) const
{
	eastl::hash_set<eastl::string>::iterator iter = m_hashSet.find(value);

	return iter != m_hashSet.end();
}

eastl::vector < eastl::string > StringLibrary::GetFlatStrings() const
{
	eastl::vector < eastl::string > vec;
	
	for (eastl::hash_set<eastl::string>::iterator iter = m_hashSet.begin();
		iter != m_hashSet.end();
		iter++)
	{
		vec.push_back(*iter);
	}
	return vec;
}


