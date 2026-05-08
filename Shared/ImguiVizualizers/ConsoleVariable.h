// Simple console variable system
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace Console {

struct IConsoleVar {
    virtual ~IConsoleVar() = default;
    virtual std::string GetAsString() const = 0;
    virtual bool SetFromString(const std::string& v) = 0;
    std::string description;
};

using VarMap = std::unordered_map<std::string, IConsoleVar*>;

inline VarMap& GetRegistry() {
    static VarMap s_registry;
    return s_registry;
}

inline std::mutex& GetRegistryMutex() {
    static std::mutex s_mutex;
    return s_mutex;
}

inline void RegisterVar(const std::string& name, IConsoleVar* var) {
    std::lock_guard<std::mutex> lk(GetRegistryMutex());
    GetRegistry()[name] = var;
}

inline IConsoleVar* FindVar(const std::string& name) {
    std::lock_guard<std::mutex> lk(GetRegistryMutex());
    auto it = GetRegistry().find(name);
    return it == GetRegistry().end() ? nullptr : it->second;
}

inline std::vector<std::string> GetAllVarNames() {
    std::lock_guard<std::mutex> lk(GetRegistryMutex());
    std::vector<std::string> out;
    out.reserve(GetRegistry().size());
    for (auto& kv : GetRegistry()) out.push_back(kv.first);
    return out;
}

template<typename T>
struct ConsoleVar : public IConsoleVar {
    T* ptr;
    ConsoleVar(const std::string& name, T* p, const std::string& desc = "") : ptr(p) {
        description = desc;
        RegisterVar(name, this);
    }
    std::string GetAsString() const override;
    bool SetFromString(const std::string& v) override;
};

// Specializations
template<> inline std::string ConsoleVar<bool>::GetAsString() const { return *ptr ? "1" : "0"; }
template<> inline bool ConsoleVar<bool>::SetFromString(const std::string& v) {
    if (v == "1" || v == "true" || v == "True") { *ptr = true; return true; }
    if (v == "0" || v == "false" || v == "False") { *ptr = false; return true; }
    return false;
}

template<> inline std::string ConsoleVar<int>::GetAsString() const { return std::to_string(*ptr); }
template<> inline bool ConsoleVar<int>::SetFromString(const std::string& v) {
    try { *ptr = std::stoi(v); return true; } catch(...) { return false; }
}

template<> inline std::string ConsoleVar<float>::GetAsString() const { return std::to_string(*ptr); }
template<> inline bool ConsoleVar<float>::SetFromString(const std::string& v) {
    try { *ptr = std::stof(v); return true; } catch(...) { return false; }
}

template<> inline std::string ConsoleVar<std::string>::GetAsString() const { return *ptr; }
template<> inline bool ConsoleVar<std::string>::SetFromString(const std::string& v) { *ptr = v; return true; }

// Helper macros for declaring console variables. Usage:
// CONSOLE_COMMAND(bool, PhysDebug, "description")
#define __CONCAT(a,b) a##b
#define _CONCAT(a,b) __CONCAT(a,b)
#define CONSOLE_COMMAND(type, name, desc) \
    static type name = type(); \
    static Console::ConsoleVar<type> _CONCAT(s_consolevar_reg_, name)(#name, &name, desc)

// Macro to get the variable by name (simply expands to the variable)
#define CONSOLE_GET(name) (name)

} // namespace Console
