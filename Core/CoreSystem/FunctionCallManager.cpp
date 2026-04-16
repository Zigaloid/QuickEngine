#include "FunctionCallManager.h"

#include <algorithm>
#include <cctype>

namespace FunctionCall {

// ── ParameterConverter specializations ───────────────────────────────────────

template<>
int ParameterConverter::Convert<int>(const std::string& str)
{
    return std::stoi(str);
}

template<>
float ParameterConverter::Convert<float>(const std::string& str)
{
    return std::stof(str);
}

template<>
double ParameterConverter::Convert<double>(const std::string& str)
{
    return std::stod(str);
}

template<>
bool ParameterConverter::Convert<bool>(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes";
}

template<>
std::string ParameterConverter::Convert<std::string>(const std::string& str)
{
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"')
    {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

} // namespace FunctionCall
