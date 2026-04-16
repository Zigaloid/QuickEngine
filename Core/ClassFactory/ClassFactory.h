#pragma once

#include <map>
#include <string>
#include <vector>

class CReflectedBase;

using CreateFunctionPtr = CReflectedBase* (*)();
using ClassMap = std::map<std::string, CreateFunctionPtr>;

/** @brief Static registry that maps class name strings to factory functions for reflected objects. */
class ClassFactory
{
public:
	/** @param className Name of the class to register.
	 *  @param createFunction Factory function that constructs an instance. */
	explicit ClassFactory(std::string className, CreateFunctionPtr createFunction);

	/** @param className Name of the class to instantiate.
	 *  @param Returns a new instance, or nullptr if the class is not registered. */
	static CReflectedBase* CreateObject(const char* className);

	/** @brief Returns a snapshot of all registered class names. */
	static std::vector<std::string> GetRegisteredClassNames();

private:
	static ClassMap* s_classFactoryMap;
};
