#pragma once
#include <string>
class CReflectedBase;

void toLower(std::string& str);

void pathSanitize(std::string& path);

void DumpReflectionMap(CReflectedBase* object);