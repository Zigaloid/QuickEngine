#include "ClassFactory.h"

tClassMap *ClassFactory::s_classFactoryMap = nullptr;

ClassFactory::ClassFactory(std::string className, createFunctionPtr creatFunction)
{
    if (s_classFactoryMap == nullptr)
    {
        s_classFactoryMap = new tClassMap;
    }
    s_classFactoryMap->insert(std::pair(className, creatFunction));
}

CReflectedBase * ClassFactory::createObject(const char *className)
{
    tClassMap::iterator foundItr = s_classFactoryMap->find(className);
    if( foundItr != s_classFactoryMap->end()  )
    {                
        return foundItr->second();
    }
    return nullptr;
}
