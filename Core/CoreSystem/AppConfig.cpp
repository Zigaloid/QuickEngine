#include "AppConfig.h"

namespace Core {

REFL_DEFINE_OBJECT(AppConfig)
    REFL_DEFINE_STRING_MEMBER(AppConfig, m_workingDirectory),
    REFL_DEFINE_INT_MEMBER(AppConfig, m_windowWidth),
    REFL_DEFINE_INT_MEMBER(AppConfig, m_windowHeight)
REFL_DEFINE_END

AppConfig::AppConfig()
{
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
        return m_workingDirectory + path.substr(2);
    return path;
}

} // namespace Core
