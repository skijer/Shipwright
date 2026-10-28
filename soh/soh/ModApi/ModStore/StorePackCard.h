#pragma once

#include <algorithm>
#include <cfloat>
#include <string>
#include <imgui.h>

// Presentation only: activation uses ImGui's selectable so mouse and keyboard behave alike.
namespace StorePackCard {

inline std::string SingleLine(std::string value) {
    for (char& c : value) {
        if (c == '\n' || c == '\r' || c == '\t')
            c = ' ';
    }
    return value;
}

inline std::string FitLine(const std::string& text, float size, float width) {
    const auto value = SingleLine(text);
    if (ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, value.c_str()).x <= width) {
        return value;
    }
    std::string result = value;
    while (!result.empty()) {
        size_t last = result.size() - 1;
        while (last > 0 && (static_cast<unsigned char>(result[last]) & 0xc0) == 0x80) {
            --last;
        }
        result.erase(last);
        if (ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, (result + "...").c_str()).x <= width) {
            return result + "...";
        }
    }
    return "";
}

inline bool Draw(const std::string& name, const std::string& author, const std::string& version, ImTextureID icon,
                 bool expanded, float width) {
    ImGui::Dummy(ImVec2(0, 0)); // Reset the previous table cell's text baseline before placing the card.
    const float base = ImGui::GetFontSize();
    const float padding = base * 0.65f;
    const float titleSize = base * 1.4f;
    const float smallSize = base * 0.85f;
    const float titleLine = titleSize * 1.1f;
    const float smallLine = smallSize * 1.4f;
    const float iconSize = 3 * titleLine + 2 * smallLine;
    const float height = iconSize + 2 * padding;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const bool activated = ImGui::Selectable("##pack-card", expanded, ImGuiSelectableFlags_None, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(start.x + width, start.y + height);
    const ImU32 background = ImGui::GetColorU32(expanded  ? ImGuiCol_FrameBgActive
                                                : hovered ? ImGuiCol_HeaderHovered
                                                          : ImGuiCol_FrameBg);
    draw->AddRectFilled(start, end, background, base * 0.3f);
    draw->AddRect(start, end, ImGui::GetColorU32(focused || expanded ? ImGuiCol_CheckMark : ImGuiCol_Border),
                  base * 0.3f, 0, focused || expanded ? 2.0f : 1.0f);
    const ImVec2 imageMin(start.x + padding, start.y + padding);
    const ImVec2 imageMax(imageMin.x + iconSize, imageMin.y + iconSize);
    if (icon) {
        draw->AddImage(icon, imageMin, imageMax);
    } else {
        draw->AddRectFilled(imageMin, imageMax, ImGui::GetColorU32(ImGuiCol_WindowBg), base * 0.2f);
        const float size = base * 2.5f;
        const auto letter = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, "P");
        draw->AddText(ImGui::GetFont(), size,
                      ImVec2(imageMin.x + (iconSize - letter.x) / 2, imageMin.y + (iconSize - letter.y) / 2),
                      ImGui::GetColorU32(ImGuiCol_TextDisabled), "P");
    }
    const float textX = imageMax.x + padding;
    const float textWidth = std::max(base, end.x - padding - textX);
    const ImVec4 clip(textX, imageMin.y, end.x - padding, imageMax.y);
    const auto titleText = SingleLine(name);
    const char* remaining = titleText.c_str();
    const char* nameEnd = remaining + titleText.size();
    for (int line = 0; line < 3 && *remaining; ++line) {
        const char* wrap = ImGui::GetFont()->CalcWordWrapPositionA(titleSize / ImGui::GetFont()->FontSize, remaining,
                                                                   nameEnd, textWidth);
        if (wrap <= remaining) {
            wrap = remaining + 1;
            while ((*wrap & 0xc0) == 0x80)
                ++wrap;
        }
        std::string title = line == 2 ? FitLine(remaining, titleSize, textWidth) : std::string(remaining, wrap);
        draw->AddText(ImGui::GetFont(), titleSize, ImVec2(textX, imageMin.y + line * titleLine),
                      ImGui::GetColorU32(ImGuiCol_Text), title.c_str(), nullptr, 0, &clip);
        remaining = wrap;
        while (*remaining == ' ' || *remaining == '\n' || *remaining == '\r')
            ++remaining;
    }
    ImVec4 secondary = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    secondary.w *= 0.78f;
    const ImU32 secondaryColor = ImGui::GetColorU32(secondary);
    const auto by = FitLine("By " + author, smallSize, textWidth);
    const auto release = FitLine(version, smallSize, textWidth);
    draw->AddText(ImGui::GetFont(), smallSize, ImVec2(textX, imageMin.y + 3 * titleLine), secondaryColor, by.c_str(),
                  nullptr, 0, &clip);
    draw->AddText(ImGui::GetFont(), smallSize, ImVec2(textX, imageMin.y + 3 * titleLine + smallLine), secondaryColor,
                  release.c_str(), nullptr, 0, &clip);
    if (hovered) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(name.c_str());
        ImGui::TextDisabled("By %s", author.c_str());
        ImGui::TextUnformatted(version.c_str());
        ImGui::TextDisabled(expanded ? "Click to collapse" : "Click for description and options");
        ImGui::EndTooltip();
    }
    return activated;
}

} // namespace StorePackCard
