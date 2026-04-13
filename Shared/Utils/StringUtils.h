#pragma once
#include <string>

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

