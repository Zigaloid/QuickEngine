#include "ClassFactory.h"

ClassMap* ClassFactory::s_classFactoryMap = nullptr;

ClassFactory::ClassFactory(std::string className, CreateFunctionPtr createFunction)
{
	if (s_classFactoryMap == nullptr)
	{
		s_classFactoryMap = new ClassMap;
	}
	s_classFactoryMap->insert(std::pair(className, createFunction));
}

CReflectedBase* ClassFactory::CreateObject(const char* className)
{
	ClassMap::iterator foundItr = s_classFactoryMap->find(className);
	if (foundItr != s_classFactoryMap->end())
	{
		return foundItr->second();
	}
	return nullptr;
}

std::vector<std::string> ClassFactory::GetRegisteredClassNames()
{
	std::vector<std::string> names;
	if (!s_classFactoryMap) return names;
	names.reserve(s_classFactoryMap->size());
	for (const auto& kv : *s_classFactoryMap)
	{
		names.push_back(kv.first);
	}
	return names;
}
