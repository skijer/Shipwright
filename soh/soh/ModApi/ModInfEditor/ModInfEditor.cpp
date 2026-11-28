#include "soh/ModApi/ModInfEditor/ModInfEditor.h"

#include <imgui.h>

#include "soh/ModApi/ModCounters/ModCounters.h"
#include "soh/ModApi/RandoInfCustom/RandoInfCustom.h"

namespace {

void DrawFlags(const ImGuiTextFilter& filter) {
    for (uint16_t flag = 1; flag <= RandoInfCustom_GetCount(); flag++) {
        const char* key = RandoInfCustom_GetKey(flag);
        if (!filter.PassFilter(key)) {
            continue;
        }
        bool isSet = RandoInfCustom_Get(SOH_RANDO_INF_SAVE, flag);
        if (ImGui::Checkbox(key, &isSet)) {
            RandoInfCustom_Set(SOH_RANDO_INF_SAVE, flag, isSet);
        }
    }
}

void DrawCounters(const ImGuiTextFilter& filter) {
    for (const ModCounter& counter : ModCounters_List()) {
        if (!filter.PassFilter(counter.key.c_str())) {
            continue;
        }
        int value = counter.value;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        if (ImGui::InputInt(counter.key.c_str(), &value)) {
            ModCounters_Set(counter.key.c_str(), value);
        }
    }
}

} // namespace

void ModInfEditor_Draw() {
    const bool hasFlags = RandoInfCustom_GetCount() > 0;
    const bool hasCounters = !ModCounters_List().empty();
    if (!hasFlags && !hasCounters) {
        ImGui::TextDisabled("No mod registered a flag or counter.");
        return;
    }
    static ImGuiTextFilter filter;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16);
    filter.Draw("##modInfFilter");
    if (hasCounters && ImGui::TreeNodeEx("Counters", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawCounters(filter);
        ImGui::TreePop();
    }
    if (hasFlags && ImGui::TreeNodeEx("Flags", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawFlags(filter);
        ImGui::TreePop();
    }
}
