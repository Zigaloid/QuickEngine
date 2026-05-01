#include "AppConfig.h"

namespace Core {

REFL_DEFINE_OBJECT(AppConfig)
    REFL_DEFINE_STRING_MEMBER(AppConfig, m_workingDirectory)
REFL_DEFINE_END

AppConfig::AppConfig()
{
    // Silently ignores missing file — built-in defaults are used in that case.
    Read("AppConfig.json");
}

AppConfig& AppConfig::Instance()
{
    static AppConfig instance;
    return instance;
}

std::string AppConfig::ResolvePath(const std::string& path) const
{
    if (path.rfind("./", 0) == 0)
    {
        // Replace the leading "./" with the configured working directory.
        return m_workingDirectory + path.substr(2);
    }
    return path;
}

} // namespace Core
