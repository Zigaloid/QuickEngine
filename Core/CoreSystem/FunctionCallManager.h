#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <sstream>
#include <typeinfo>
#include <any>
#include <memory>
#include <stdexcept>
#include <algorithm>

namespace FunctionCall {

// Forward declarations
class FunctionCallManager;

// ── CallResult ────────────────────────────────────────────────────────────────

/** @brief Wrapper for function call results, carrying either a value or an error message. */
template<typename T>
class CallResult {
private:
    T           m_value;
    bool        m_hasValue = false;
    std::string m_error;

public:
    CallResult() : m_hasValue(false) {}
    explicit CallResult(const T& value) : m_value(value), m_hasValue(true) {}
    explicit CallResult(const std::string& error) : m_error(error), m_hasValue(false) {}

    bool IsSuccess() const { return m_hasValue; }
    const T& GetValue() const
    {
        if (!m_hasValue) throw std::runtime_error("No value available: " + m_error);
        return m_value;
    }
    const std::string& GetError() const { return m_error; }
};

/** @brief Specialization of CallResult for void return type. */
template<>
class CallResult<void> {
private:
    bool        m_success = true;
    std::string m_error;

public:
    CallResult() : m_success(true) {}
    explicit CallResult(const std::string& error) : m_error(error), m_success(false) {}

    bool IsSuccess() const { return m_success; }
    const std::string& GetError() const { return m_error; }
};

// ── ParameterConverter ────────────────────────────────────────────────────────

/** @brief Utility for converting string tokens to typed values. */
class ParameterConverter {
public:
    template<typename T>
    static T Convert(const std::string& str);

    template<>
    static int Convert<int>(const std::string& str);

    template<>
    static float Convert<float>(const std::string& str);

    template<>
    static double Convert<double>(const std::string& str);

    template<>
    static bool Convert<bool>(const std::string& str);

    template<>
    static std::string Convert<std::string>(const std::string& str);
};

// ── IFunctionWrapper ──────────────────────────────────────────────────────────

/** @brief Base class for type-erased function wrappers. */
class IFunctionWrapper {
public:
    virtual ~IFunctionWrapper() = default;
    virtual std::any Call(const std::vector<std::string>& params) = 0;
    virtual std::string GetSignature() const = 0;
    virtual size_t GetParameterCount() const = 0;
};

// ── FunctionWrapper ───────────────────────────────────────────────────────────

/** @brief Concrete wrapper that stores a typed callable and converts string parameters. */
template<typename ReturnType, typename... Args>
class FunctionWrapper : public IFunctionWrapper {
private:
    std::function<ReturnType(Args...)> m_function;
    std::string                        m_name;
    std::vector<std::string>           m_paramNames;

    template<size_t Index = 0>
    auto ConvertParameters(const std::vector<std::string>& params)
    {
        if constexpr (Index == sizeof...(Args))
        {
            return std::tuple<>();
        }
        else
        {
            using ArgType = std::tuple_element_t<Index, std::tuple<Args...>>;
            if (Index >= params.size())
            {
                throw std::runtime_error("Not enough parameters provided");
            }

            ArgType converted = ParameterConverter::Convert<ArgType>(params[Index]);
            auto rest = ConvertParameters<Index + 1>(params);
            return std::tuple_cat(std::make_tuple(converted), rest);
        }
    }

public:
    FunctionWrapper(const std::string& name, std::function<ReturnType(Args...)> func,
                    const std::vector<std::string>& paramNames = {})
        : m_function(func), m_name(name), m_paramNames(paramNames) {}

    std::any Call(const std::vector<std::string>& params) override
    {
        if (params.size() != sizeof...(Args))
        {
            throw std::runtime_error("Parameter count mismatch. Expected " +
                std::to_string(sizeof...(Args)) + ", got " + std::to_string(params.size()));
        }

        try
        {
            auto convertedParams = ConvertParameters(params);

            if constexpr (std::is_void_v<ReturnType>)
            {
                std::apply(m_function, convertedParams);
                return std::any{};
            }
            else
            {
                ReturnType result = std::apply(m_function, convertedParams);
                return std::make_any<ReturnType>(result);
            }
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("Parameter conversion failed: " + std::string(e.what()));
        }
    }

    std::string GetSignature() const override
    {
        std::stringstream ss;
        ss << m_name << "(";

        if (!m_paramNames.empty())
        {
            for (size_t i = 0; i < m_paramNames.size(); ++i)
            {
                if (i > 0) ss << ", ";
                ss << m_paramNames[i];
            }
        }
        else
        {
            std::vector<std::string> typeNames = {typeid(Args).name()...};
            for (size_t i = 0; i < typeNames.size(); ++i)
            {
                if (i > 0) ss << ", ";
                ss << "param" << i;
            }
        }

        ss << ")";
        if constexpr (!std::is_void_v<ReturnType>)
        {
            ss << " -> " << typeid(ReturnType).name();
        }

        return ss.str();
    }

    size_t GetParameterCount() const override
    {
        return sizeof...(Args);
    }
};

// ── FunctionCallParser ────────────────────────────────────────────────────────

/** @brief Parses a string of the form "FuncName(arg1, arg2)" into name + parameter list. */
class FunctionCallParser {
public:
    struct ParsedCall {
        std::string              functionName;
        std::vector<std::string> parameters;
        bool                     isValid = false;
        std::string              error;

        ParsedCall() : isValid(false) {}
    };

    static ParsedCall Parse(const std::string& callString)
    {
        ParsedCall result;

        std::string trimmed = Trim(callString);

        size_t parenPos = trimmed.find('(');
        if (parenPos == std::string::npos)
        {
            result.error = "Missing opening parenthesis";
            return result;
        }

        result.functionName = Trim(trimmed.substr(0, parenPos));
        if (result.functionName.empty())
        {
            result.error = "Empty function name";
            return result;
        }

        size_t closeParenPos = trimmed.rfind(')');
        if (closeParenPos == std::string::npos || closeParenPos <= parenPos)
        {
            result.error = "Missing or misplaced closing parenthesis";
            return result;
        }

        std::string paramStr = trimmed.substr(parenPos + 1, closeParenPos - parenPos - 1);
        paramStr = Trim(paramStr);

        if (!paramStr.empty())
        {
            result.parameters = SplitParameters(paramStr);
        }

        result.isValid = true;
        return result;
    }

private:
    static std::string Trim(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";

        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    static std::vector<std::string> SplitParameters(const std::string& paramStr)
    {
        std::vector<std::string> params;
        std::string current;
        bool inQuotes = false;
        char quoteChar = '\0';
        int parenDepth = 0;

        for (char c : paramStr)
        {
            if (!inQuotes && (c == '"' || c == '\''))
            {
                inQuotes = true;
                quoteChar = c;
                current += c;
            }
            else if (inQuotes && c == quoteChar)
            {
                inQuotes = false;
                current += c;
            }
            else if (!inQuotes && c == '(')
            {
                parenDepth++;
                current += c;
            }
            else if (!inQuotes && c == ')')
            {
                parenDepth--;
                current += c;
            }
            else if (!inQuotes && c == ',' && parenDepth == 0)
            {
                params.push_back(Trim(current));
                current.clear();
            }
            else
            {
                current += c;
            }
        }

        if (!current.empty())
        {
            params.push_back(Trim(current));
        }

        return params;
    }
};

// ── FunctionCallManager ───────────────────────────────────────────────────────

/** @brief Registry for named callable functions, with string-based invocation support. */
class FunctionCallManager {
private:
    std::unordered_map<std::string, std::unique_ptr<IFunctionWrapper>> m_functions;
    bool m_caseSensitive = true;

public:
    explicit FunctionCallManager(bool caseSensitive = true) : m_caseSensitive(caseSensitive) {}

    /**
     * @brief Register a function with the manager.
     * @param name The function name used for lookup.
     * @param func The callable to register.
     * @param paramNames Optional parameter names for signature display.
     */
    template<typename ReturnType, typename... Args>
    void RegisterFunction(const std::string& name, std::function<ReturnType(Args...)> func,
                          const std::vector<std::string>& paramNames = {})
    {
        std::string actualName = m_caseSensitive ? name : ToLower(name);
        m_functions[actualName] = std::make_unique<FunctionWrapper<ReturnType, Args...>>(
            name, func, paramNames);
    }

    /**
     * @brief Register a member function bound to an instance.
     * @param name The function name used for lookup.
     * @param instance Pointer to the object instance.
     * @param func Pointer-to-member-function.
     * @param paramNames Optional parameter names for signature display.
     */
    template<typename ReturnType, typename ClassType, typename... Args>
    void RegisterMemberFunction(const std::string& name, ClassType* instance,
                                ReturnType(ClassType::*func)(Args...),
                                const std::vector<std::string>& paramNames = {})
    {
        auto wrapper = [instance, func](Args... args) -> ReturnType
        {
            if constexpr (std::is_void_v<ReturnType>)
            {
                (instance->*func)(args...);
            }
            else
            {
                return (instance->*func)(args...);
            }
        };
        RegisterFunction<ReturnType, Args...>(name, wrapper, paramNames);
    }

    /**
     * @brief Call a function by parsing a "Name(arg1, arg2)" string.
     * @param callString The full call expression to parse and execute.
     */
    template<typename ReturnType = void>
    CallResult<ReturnType> CallFunction(const std::string& callString)
    {
        auto parsed = FunctionCallParser::Parse(callString);
        if (!parsed.isValid)
        {
            return CallResult<ReturnType>(parsed.error);
        }

        return CallFunction<ReturnType>(parsed.functionName, parsed.parameters);
    }

    /**
     * @brief Call a function by name with a pre-split parameter list.
     * @param functionName Name of the registered function.
     * @param parameters String-encoded argument values.
     */
    template<typename ReturnType = void>
    CallResult<ReturnType> CallFunction(const std::string& functionName,
                                        const std::vector<std::string>& parameters)
    {
        std::string actualName = m_caseSensitive ? functionName : ToLower(functionName);
        auto it = m_functions.find(actualName);
        if (it == m_functions.end())
        {
            return CallResult<ReturnType>("Function '" + functionName + "' not found");
        }

        try
        {
            std::any result = it->second->Call(parameters);

            if constexpr (std::is_void_v<ReturnType>)
            {
                return CallResult<ReturnType>();
            }
            else
            {
                try
                {
                    ReturnType value = std::any_cast<ReturnType>(result);
                    return CallResult<ReturnType>(value);
                }
                catch (const std::bad_any_cast& e)
                {
                    return CallResult<ReturnType>("Return type mismatch: " + std::string(e.what()));
                }
            }
        }
        catch (const std::exception& e)
        {
            return CallResult<ReturnType>("Function call failed: " + std::string(e.what()));
        }
    }

    /** @brief Returns the signatures of all registered functions. */
    std::vector<std::string> GetRegisteredFunctions() const
    {
        std::vector<std::string> functions;
        for (const auto& [name, wrapper] : m_functions)
        {
            functions.push_back(wrapper->GetSignature());
        }
        return functions;
    }

    /** @brief Returns true if a function with the given name is registered. */
    bool IsFunctionRegistered(const std::string& name) const
    {
        std::string actualName = m_caseSensitive ? name : ToLower(name);
        return m_functions.find(actualName) != m_functions.end();
    }

    /** @brief Removes a registered function by name. Returns true if it was found and removed. */
    bool UnregisterFunction(const std::string& name)
    {
        std::string actualName = m_caseSensitive ? name : ToLower(name);
        return m_functions.erase(actualName) > 0;
    }

    /** @brief Removes all registered functions. */
    void Clear()
    {
        m_functions.clear();
    }

    /** @brief Returns the number of registered functions. */
    size_t GetFunctionCount() const
    {
        return m_functions.size();
    }

private:
    std::string ToLower(const std::string& str) const
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
};

} // namespace FunctionCall
