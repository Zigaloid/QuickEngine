#pragma once
#include <string>
#include "CoreSystem/CoreDebugChannels.h"

void toLower(std::string& str)
{
    for (char& c : str)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
}

void pathSanitize(std::string& path)
{
    // Replace backslashes with forward slashes
    for (char& c : path)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }
    toLower(path);
}


void DumpReflectionMap(CReflectedBase* object)
{
	if (!object) return;

	ReflectionDebug.printf("DumpReflectionMap for object\n");
	std::vector<std::pair<const char*, std::vector<CReflectionMapEntry>*>> maps;
	object->CollectHierarchyReflectionMaps(maps);
	for (auto& p : maps) {
		const char* className = p.first;
		ReflectionDebug.printf("Class: %s\n", className);
		for (auto& entry : *p.second) {
			CPropertyBase* prop = entry.GetProperty();
			if (!prop) continue;
				ReflectionDebug.printf("  prop='%s' type=%d offset=%zu size=%zu\n",	prop->GetName().c_str(), static_cast<int>(prop->GetType()), prop->GetOffset(), prop->GetSize());
		}
	}
}