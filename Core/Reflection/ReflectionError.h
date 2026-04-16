#pragma once

#include <string>
#include <exception>
#include <vector>
#include <sstream>
#include <memory>
#include <optional>

// Forward declarations to avoid circular dependencies
namespace DebugChannels {
    class CDebugChannel;
}

namespace Reflection {

/** @brief Error severity levels. */
enum class ErrorSeverity
{
    Info,
    Warning,
    Error,
    Critical
};

/** @brief Error categories for better classification. */
enum class ErrorCategory
{
    FileIO,
    Parsing,
    Validation,
    Memory,
    TypeMismatch,
    PropertyAccess,
    ClassFactory,
    Unknown
};

/** @brief Detailed error information. */
struct ErrorInfo
{
    ErrorSeverity severity;
    ErrorCategory category;
    std::string   message;
    std::string   context;
    std::string   fileName;
    int           lineNumber;

    ErrorInfo(ErrorSeverity sev, ErrorCategory cat, const std::string& msg,
              const std::string& ctx = "", const std::string& file = "", int line = 0)
        : severity(sev), category(cat), message(msg), context(ctx), fileName(file), lineNumber(line)
    {
    }

    ErrorInfo()
        : severity(ErrorSeverity::Error), category(ErrorCategory::Unknown),
          message(""), context(""), fileName(""), lineNumber(0)
    {
    }

    std::string ToString() const
    {
        std::stringstream ss;
        ss << "[" << SeverityToString(severity) << "][" << CategoryToString(category) << "] ";
        ss << message;
        if (!context.empty())  ss << " (Context: " << context << ")";
        if (!fileName.empty()) ss << " [File: " << fileName;
        if (lineNumber > 0)    ss << ":" << lineNumber;
        if (!fileName.empty()) ss << "]";
        return ss.str();
    }

private:
    static std::string SeverityToString(ErrorSeverity sev)
    {
        switch (sev)
        {
        case ErrorSeverity::Info:     return "INFO";
        case ErrorSeverity::Warning:  return "WARN";
        case ErrorSeverity::Error:    return "ERROR";
        case ErrorSeverity::Critical: return "CRITICAL";
        default:                      return "UNKNOWN";
        }
    }

    static std::string CategoryToString(ErrorCategory cat)
    {
        switch (cat)
        {
        case ErrorCategory::FileIO:          return "FileIO";
        case ErrorCategory::Parsing:         return "Parsing";
        case ErrorCategory::Validation:      return "Validation";
        case ErrorCategory::Memory:          return "Memory";
        case ErrorCategory::TypeMismatch:    return "TypeMismatch";
        case ErrorCategory::PropertyAccess:  return "PropertyAccess";
        case ErrorCategory::ClassFactory:    return "ClassFactory";
        default:                             return "Unknown";
        }
    }
};

/** @brief Custom exception class for reflection errors. */
class ReflectionException : public std::exception
{
public:
    explicit ReflectionException(const ErrorInfo& error) : m_error(error) {}

    const char* what() const noexcept override
    {
        m_whatString = m_error.ToString();
        return m_whatString.c_str();
    }

    const ErrorInfo& GetErrorInfo() const { return m_error; }

private:
    ErrorInfo             m_error;
    mutable std::string   m_whatString;
};

/** @brief Interface for error handling strategies. */
class IErrorHandler
{
public:
    virtual ~IErrorHandler() = default;
    virtual void HandleError(const ErrorInfo& error) = 0;
    virtual bool ShouldThrow(ErrorSeverity severity) const = 0;
};

/** @brief Default error handler implementation. */
class DefaultErrorHandler : public IErrorHandler
{
public:
    explicit DefaultErrorHandler(bool throwOnError = true, bool throwOnWarning = false)
        : m_throwOnError(throwOnError), m_throwOnWarning(throwOnWarning)
    {
    }

    void HandleError(const ErrorInfo& error) override;

    bool ShouldThrow(ErrorSeverity severity) const override
    {
        switch (severity)
        {
        case ErrorSeverity::Critical:
        case ErrorSeverity::Error:
            return m_throwOnError;
        case ErrorSeverity::Warning:
            return m_throwOnWarning;
        default:
            return false;
        }
    }

    const std::vector<ErrorInfo>& GetErrors() const { return m_errors; }
    void ClearErrors() { m_errors.clear(); }

private:
    bool                      m_throwOnError;
    bool                      m_throwOnWarning;
    std::vector<ErrorInfo>    m_errors;
};

/** @brief Global error handler instance. */
extern std::unique_ptr<IErrorHandler> g_errorHandler;

/** @param handler New error handler to install. */
inline void SetErrorHandler(std::unique_ptr<IErrorHandler> handler)
{
    g_errorHandler = std::move(handler);
}

void ReportError(ErrorSeverity severity, ErrorCategory category,
                 const std::string& message, const std::string& context = "",
                 const std::string& file = "", int line = 0);

// Convenience macros for error reporting
#define REFL_ERROR(category, message, context) \
    Reflection::ReportError(Reflection::ErrorSeverity::Error, category, message, context, __FILE__, __LINE__)

#define REFL_WARNING(category, message, context) \
    Reflection::ReportError(Reflection::ErrorSeverity::Warning, category, message, context, __FILE__, __LINE__)

#define REFL_CRITICAL(category, message, context) \
    Reflection::ReportError(Reflection::ErrorSeverity::Critical, category, message, context, __FILE__, __LINE__)

/** @brief Result of an operation that can fail. */
template<typename T>
class Result
{
public:
    static Result Success(T&& value)
    {
        Result result;
        result.m_success = true;
        result.m_value = std::forward<T>(value);
        return result;
    }

    static Result Success(const T& value)
    {
        Result result;
        result.m_success = true;
        result.m_value = value;
        return result;
    }

    static Result Error(const ErrorInfo& error)
    {
        Result result;
        result.m_success = false;
        result.m_error = error;
        return result;
    }

    bool IsSuccess() const { return m_success; }
    bool IsError() const   { return !m_success; }

    const T& GetValue() const
    {
        if (!m_success)
        {
            throw ReflectionException(m_error.value());
        }
        return m_value.value();
    }

    const ErrorInfo& GetError() const
    {
        if (m_success)
        {
            throw std::logic_error("Cannot get error from successful result");
        }
        return m_error.value();
    }

private:
    Result() : m_success(false) {}

    bool                  m_success;
    std::optional<T>      m_value;
    std::optional<ErrorInfo> m_error;
};

bool ValidateFilePath(const std::string& path);
bool ValidatePropertyAccess(const void* ptr, const std::string& propertyName);

} // namespace Reflection
