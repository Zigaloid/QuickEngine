#pragma once

#include "Reflection/Reflection.h"
#include <string>

namespace Core {

/** @brief Application configuration loaded from AppConfig.json at startup.
 *
 *  Uses the Reflection system to deserialise settings from JSON.
 *  Call AppConfig::Instance() to access the singleton; on first access it
 *  attempts to read "AppConfig.json" from the current working directory.
 *  If the file is absent the built-in defaults are used (workingDirectory = "./").
 */
class AppConfig : public CReflectedBase
{
public:
    REFL_DECLARE_OBJECT(AppConfig, CReflectedBase)

    /** @brief Returns the singleton instance (loads config on first call). */
    static AppConfig& Instance();

    /** @brief Returns the configured working directory (always ends with '/').
     *  Defaults to "./" if no config file is found. */
    const std::string& GetWorkingDirectory() const { return m_workingDirectory; }

    /** @brief Resolves a path by substituting the leading "./" with the
     *  configured working directory.
     *  Paths that do not begin with "./" are returned unchanged. */
    std::string ResolvePath(const std::string& path) const;

private:
    AppConfig();

    /** @brief Working directory prepended to asset paths that begin with "./".
     *  Must end with a '/' (or '\' on Windows).  Default: "./" */
    std::string m_workingDirectory = "../";
};

} // namespace Core
