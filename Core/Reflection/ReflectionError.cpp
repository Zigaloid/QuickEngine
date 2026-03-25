#include "ReflectionError.h"
#include "DebugChannel\DebugChannel.h"

// External declaration - defined in ReflectionBase.cpp
extern DebugChannels::CDebugChannel ReflectionDebug;

namespace Reflection {
    // Initialize global error handler
    std::unique_ptr<IErrorHandler> g_errorHandler = 
        std::make_unique<DefaultErrorHandler>(true, false);

    void DefaultErrorHandler::HandleError(const ErrorInfo& error) {
        // Log to debug channel
        ReflectionDebug.print(error.ToString());
        
        // Store error for later retrieval
        m_errors.push_back(error);
        
        // Throw if configured to do so
        if (ShouldThrow(error.severity)) {
            throw ReflectionException(error);
        }
    }

    void ReportError(ErrorSeverity severity, ErrorCategory category, 
                    const std::string& message, const std::string& context,
                    const std::string& file, int line) {
        if (g_errorHandler) {
            ErrorInfo error(severity, category, message, context, file, line);
            g_errorHandler->HandleError(error);
        }
    }

    bool ValidateFilePath(const std::string& path) {
        if (path.empty()) return false;
        if (path.find_first_of("*?\"<>|") != std::string::npos) return false;
        return true;
    }
    
    bool ValidatePropertyAccess(const void* ptr, const std::string& propertyName) {
        if (!ptr) {
            REFL_ERROR(ErrorCategory::Memory, "Null pointer access", 
                      "Property: " + propertyName);
            return false;
        }
        return true;
    }
}