#include "ImGuiVisualizerManager.h"

#include "IImGuiVisualizer.h"
#include "KeyboardShortcutManager.h"
#include "imgui.h"

#include <algorithm>
#include <cassert>
#include <map>

namespace ImGuiVisualizers {

// ── Construction / Destruction ──────────────────────────────────────────

ImGuiVisualizerManager::ImGuiVisualizerManager() = default;

ImGuiVisualizerManager::~ImGuiVisualizerManager()
{
    Shutdown();
}

ImGuiVisualizerManager::ImGuiVisualizerManager(ImGuiVisualizerManager&&) noexcept = default;
ImGuiVisualizerManager& ImGuiVisualizerManager::operator=(ImGuiVisualizerManager&&) noexcept = default;

// ── Registration ────────────────────────────────────────────────────────

void ImGuiVisualizerManager::Register(const std::string& key,
                                       std::unique_ptr<IImGuiVisualizer> viz,
                                       bool visibleByDefault)
{
    assert(viz && "Cannot register a null visualizer");
    assert(m_keyIndex.find(key) == m_keyIndex.end() && "Duplicate visualizer key");

    Entry entry;
    entry.key = key;
    entry.visualizer = std::move(viz);
    entry.visible = visibleByDefault;
    entry.initialized = false;

    m_keyIndex[key] = m_entries.size();
    m_entries.push_back(std::move(entry));

    // If the manager is already initialized, initialize the new entry immediately
    if (m_initialized)
    {
        auto& added = m_entries.back();
        added.visualizer->Initialize();
        added.initialized = true;
    }

    RegisterShortcut(m_entries.back());
}

void ImGuiVisualizerManager::Unregister(const std::string& key)
{
    auto it = m_keyIndex.find(key);
    if (it == m_keyIndex.end())
        return;

    std::size_t index = it->second;
    auto& entry = m_entries[index];

    UnregisterShortcut(entry);

    if (entry.initialized) {
        entry.visualizer->Shutdown();
    }

    // Swap-and-pop for O(1) removal
    if (index != m_entries.size() - 1) {
        auto& last = m_entries.back();
        m_keyIndex[last.key] = index;
        m_entries[index] = std::move(last);
    }

    m_entries.pop_back();
    m_keyIndex.erase(it);
}

// ── Lifecycle ───────────────────────────────────────────────────────────

void ImGuiVisualizerManager::Initialize()
{
    for (auto& entry : m_entries)
    {
        if (!entry.initialized)
        {
            entry.visualizer->Initialize();
            entry.initialized = true;
        }
    }
    m_initialized = true;
}

void ImGuiVisualizerManager::Shutdown()
{
    for (auto& entry : m_entries)
    {
        UnregisterShortcut(entry);

        if (entry.initialized)
        {
            entry.visualizer->Shutdown();
            entry.initialized = false;
        }
    }
    m_entries.clear();
    m_keyIndex.clear();
    m_initialized = false;
}

// ── Per-frame ───────────────────────────────────────────────────────────

void ImGuiVisualizerManager::Update(float deltaTime)
{
    for (auto& entry : m_entries)
    {
        if (entry.initialized)
            entry.visualizer->Update(deltaTime);
    }
}

// Parse a shortcut string and extract only the key token (ignore modifiers).
// Accepts forms like "Ctrl+Alt+A" or "A" or "F1" but returns only the key.
// No parsing of modifiers: visualizers provide an ImGuiKey via GetShortcut()

void ImGuiVisualizerManager::RegisterShortcut(Entry& entry)
{
    ImGuiKey key = entry.visualizer->GetShortcut();
    if (key == ImGuiKey_None)
        return;

    // Visualizers must provide modifiers via GetShortcutModifiers(). Use that mask.
    Input::KeyModifier mods = entry.visualizer->GetShortcutModifiers();

    std::string name = ShortcutName(entry.key);
    std::string desc = std::string("Toggle ") + entry.visualizer->GetName();
    std::string entryKey = entry.key;

    Input::KeyboardShortcutManager::Instance().RegisterShortcut(
        name, desc, key, mods,
        [this, entryKey]() { ToggleVisible(entryKey); });
}

void ImGuiVisualizerManager::UnregisterShortcut(Entry& entry)
{
    Input::KeyboardShortcutManager::Instance().UnregisterShortcut(ShortcutName(entry.key));
}

std::string ImGuiVisualizerManager::ShortcutName(const std::string& key)
{
    return "Visualizer.Toggle." + key;
}

void ImGuiVisualizerManager::RenderAll()
{
    // Get the dockspace ID
    // with the same name ("MainDockSpace"), this will return a non-zero ImGuiID.
    ImGuiID mainDockId = ImGui::GetID("MainDockSpace");

    for (auto& entry : m_entries)
    {
        if (entry.visible && entry.initialized) {
            // Request docking into the main dockspace on first use.
            // Using ImGuiCond_FirstUseEver lets the user rearrange later.
            if (mainDockId != 0) {
                ImGui::SetNextWindowDockID(mainDockId, ImGuiCond_FirstUseEver);
            }
            entry.visualizer->Render(&entry.visible);
        }
    }
}

void ImGuiVisualizerManager::RenderMenuBar()
{
    RenderFileMenu();
    RenderWindowsMenu();
}

void ImGuiVisualizerManager::RenderWindowsMenu()
{
    if (ImGui::BeginMenu("Windows"))
    {
        std::map<std::string, std::vector<Entry*>> categories;
        std::vector<Entry*> rootItems;

        for (auto& entry : m_entries)
        {
            const char* cat = entry.visualizer->GetMenuCategory();
            if (cat && cat[0] != '\0')
                categories[cat].push_back(&entry);
            else
                rootItems.push_back(&entry);
        }

        for (auto* entry : rootItems)
        {
            // Build a display string for the shortcut (modifiers + key)
            ImGuiKey key = entry->visualizer->GetShortcut();
            const char* shortcutLabel = nullptr;
            std::string shortcutStr;
            if (key != ImGuiKey_None) {
                Input::KeyModifier mods = entry->visualizer->GetShortcutModifiers();
                std::string modStr = Input::KeyboardShortcutManager::ModifierToString(mods);
                if (!modStr.empty()) shortcutStr = modStr + "+";
                shortcutStr += Input::KeyboardShortcutManager::KeyToString(key);
                shortcutLabel = shortcutStr.c_str();
            }

            ImGui::MenuItem(entry->visualizer->GetName(),
                            shortcutLabel,
                            &entry->visible);
        }

        if (!categories.empty() && !rootItems.empty())
            ImGui::Separator();

        for (auto& [category, entries] : categories)
        {
            if (ImGui::BeginMenu(category.c_str()))
            {
                for (auto* entry : entries)
                {
                    // Build a display string for the shortcut (modifiers + key)
                    ImGuiKey key = entry->visualizer->GetShortcut();
                    const char* shortcutLabel = nullptr;
                    std::string shortcutStr;
                    if (key != ImGuiKey_None) {
                        Input::KeyModifier mods = entry->visualizer->GetShortcutModifiers();
                        std::string modStr = Input::KeyboardShortcutManager::ModifierToString(mods);
                        if (!modStr.empty()) shortcutStr = modStr + "+";
                        shortcutStr += Input::KeyboardShortcutManager::KeyToString(key);
                        shortcutLabel = shortcutStr.c_str();
                    }

                    ImGui::MenuItem(entry->visualizer->GetName(),
                                    shortcutLabel,
                                    &entry->visible);
                }
                ImGui::EndMenu();
            }
        }

        ImGui::EndMenu();
    }
}

// ── Visibility ──────────────────────────────────────────────────────────

void ImGuiVisualizerManager::SetVisible(const std::string& key, bool visible)
{
    if (auto* entry = FindEntry(key)) {
        entry->visible = visible;
    }
}

bool ImGuiVisualizerManager::IsVisible(const std::string& key) const
{
    if (const auto* entry = FindEntry(key)) {
        return entry->visible;
    }
    return false;
}

void ImGuiVisualizerManager::ToggleVisible(const std::string& key)
{
    if (auto* entry = FindEntry(key)) {
        entry->visible = !entry->visible;
    }
}

// ── Accessors ───────────────────────────────────────────────────────────

IImGuiVisualizer* ImGuiVisualizerManager::GetVisualizer(const std::string& key) const
{
    if (const auto* entry = FindEntry(key)) {
        return entry->visualizer.get();
    }
    return nullptr;
}

std::size_t ImGuiVisualizerManager::GetCount() const
{
    return m_entries.size();
}

void ImGuiVisualizerManager::SetFileMenuCallback(std::function<void()> callback)
{
    m_fileMenuCallback = std::move(callback);
}

// ── Internal helpers ────────────────────────────────────────────────────

ImGuiVisualizerManager::Entry* ImGuiVisualizerManager::FindEntry(const std::string& key)
{
    auto it = m_keyIndex.find(key);
    if (it == m_keyIndex.end())
        return nullptr;
    return &m_entries[it->second];
}

const ImGuiVisualizerManager::Entry* ImGuiVisualizerManager::FindEntry(const std::string& key) const
{
    auto it = m_keyIndex.find(key);
    if (it == m_keyIndex.end())
        return nullptr;
    return &m_entries[it->second];
}

void ImGuiVisualizerManager::RenderFileMenu()
{
    if (ImGui::BeginMenu("File"))
    {
        if (m_fileMenuCallback)
        {
            if (ImGui::MenuItem("Exit", "Alt+F4"))
                m_fileMenuCallback();
        }
        ImGui::EndMenu();
    }
}

} // namespace ImGuiVisualizers