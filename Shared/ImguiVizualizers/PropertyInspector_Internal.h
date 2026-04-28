#pragma once

// Private implementation header shared between PropertyInspector.cpp and
// PropertyInspector_TypeRenderers.cpp. Not part of the public API.

#include "imgui.h"

#include <string>
#include <functional>

namespace ImGuiVisualizers {

    // ?? Type colour palette ?????????????????????????????????????????????????????
    static const ImVec4 COLOR_TYPE_PRIMITIVE = ImVec4(0.7f, 0.9f, 0.7f, 1.0f);
    static const ImVec4 COLOR_TYPE_OBJECT    = ImVec4(0.7f, 0.7f, 0.9f, 1.0f);
    static const ImVec4 COLOR_TYPE_POINTER   = ImVec4(0.9f, 0.7f, 0.7f, 1.0f);
    static const ImVec4 COLOR_TYPE_VECTOR    = ImVec4(0.9f, 0.9f, 0.7f, 1.0f);
    static const ImVec4 COLOR_TYPE_COMPONENT = ImVec4(0.9f, 0.7f, 0.9f, 1.0f);

    // ?? Shared UI helpers ???????????????????????????????????????????????????????

    // Sets next item width to the available content region on the current line.219945265
    static void SetNextItemWidthToContentRegionAvail(float scaler = 0.75f)
    {
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail <= 0.0f) {
            float winWidth = ImGui::GetWindowWidth();
            float cursorX  = ImGui::GetCursorPosX();
            const ImGuiStyle& style = ImGui::GetStyle();
            avail = winWidth - cursorX - style.ItemSpacing.x * 2.0f;
            if (avail < 0.0f) avail = 0.0f;
        }
        avail *= scaler;
        ImGui::SetNextItemWidth(avail);
    }

    // Renders + / - add / remove buttons for vector properties.
    static void RenderVecAddRemoveButtons(
        const std::string&           idBase,
        bool                         readOnly,
        const std::function<void()>& onAdd,
        const std::function<void()>& onRemove)
    {
        ImGui::SameLine();
        ImGui::PushID(idBase.c_str());
        if (!readOnly) {
            if (onAdd && ImGui::SmallButton(" + "))
                onAdd();
            ImGui::SameLine();
            if (onRemove && ImGui::SmallButton(" - "))
                onRemove();
        }
        ImGui::PopID();
    }

} // namespace ImGuiVisualizers
