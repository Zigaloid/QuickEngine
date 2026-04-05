#ifndef CLASSFACTORY_H
#define CLASSFACTORY_H

#include <map>
#include <string>
#include <vector>

class CReflectedBase;

typedef CReflectedBase* (*createFunctionPtr)();
typedef std::map<std::string, createFunctionPtr> tClassMap;

class ClassFactory
{
public:
    ClassFactory(std::string name, createFunctionPtr creatFunction );
    static CReflectedBase *createObject(const char *);
    // Return list of registered class names (snapshot)
    static std::vector<std::string> GetRegisteredClassNames();
private:
    static tClassMap *s_classFactoryMap;    
};

#endif