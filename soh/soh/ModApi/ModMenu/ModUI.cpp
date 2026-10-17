#include "ModUI.h"
#include <imgui.h>
#include <cstring>

#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/UIWidgets.hpp"

static bool ValidTextFlags(uint32_t flags) {
    constexpr uint32_t callbacks = ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory |
        ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackResize |
        ImGuiInputTextFlags_CallbackEdit;
    return !(flags & callbacks);
}
uint32_t ModUI_GetVersion(void) { return 1; }
const char* ModUI_GetImGuiVersion(void) { return IMGUI_VERSION; }

// The common controls wear the menu theme so a mod's page matches the rest of the menu.
bool ModUI_Button(const char* label, float width, float height) {
    if (label == nullptr) {
        return false;
    }
    UIWidgets::PushStyleButton(THEME_COLOR);
    const bool pressed = ImGui::Button(label, ImVec2(width, height));
    UIWidgets::PopStyleButton();
    return pressed;
}

bool ModUI_SmallButton(const char* label) {
    if (label == nullptr) {
        return false;
    }
    UIWidgets::PushStyleButton(THEME_COLOR);
    const bool pressed = ImGui::SmallButton(label);
    UIWidgets::PopStyleButton();
    return pressed;
}

bool ModUI_RadioButton(const char* label, int32_t* value, int32_t choice) {
    if (label == nullptr || value == nullptr) {
        return false;
    }
    UIWidgets::PushStyleCheckbox(THEME_COLOR);
    const bool pressed = ImGui::RadioButton(label, value, choice);
    UIWidgets::PopStyleCheckbox();
    return pressed;
}

bool ModUI_Combo(const char* label, int32_t* selected, const char* const* items, int32_t count) {
    if (label == nullptr || selected == nullptr || items == nullptr || count <= 0) {
        return false;
    }
    UIWidgets::PushStyleCombobox(THEME_COLOR);
    const bool changed = ImGui::Combo(label, selected, items, count);
    UIWidgets::PopStyleCombobox();
    return changed;
}

// The theme stays pushed while the popup is open, so EndCombo pops it.
bool ModUI_BeginCombo(const char* label, const char* preview, uint32_t flags) {
    if (label == nullptr) {
        return false;
    }
    UIWidgets::PushStyleCombobox(THEME_COLOR);
    const bool open = ImGui::BeginCombo(label, preview ? preview : "", flags);
    if (!open) {
        UIWidgets::PopStyleCombobox();
    }
    return open;
}

void ModUI_EndCombo(void) {
    ImGui::EndCombo();
    UIWidgets::PopStyleCombobox();
}

bool ModUI_Selectable(const char* label, bool selected, uint32_t flags, float width, float height) { return label && ImGui::Selectable(label, selected, flags, ImVec2(width, height)); }
bool ModUI_ListBox(const char* label, int32_t* selected, const char* const* items, int32_t count, int32_t rows) { return label && selected && items && count > 0 && ImGui::ListBox(label, selected, items, count, rows); }
bool ModUI_BeginListBox(const char* label, float width, float height) { return label && ImGui::BeginListBox(label, ImVec2(width, height)); }
void ModUI_EndListBox(void) { ImGui::EndListBox(); }

bool ModUI_InputText(const char* label, char* buffer, uint32_t capacity, uint32_t flags) {
    if (label == nullptr || buffer == nullptr || capacity == 0 || !ValidTextFlags(flags)) {
        return false;
    }
    UIWidgets::PushStyleInput(THEME_COLOR);
    const bool edited = ImGui::InputText(label, buffer, capacity, flags);
    UIWidgets::PopStyleInput();
    return edited;
}

bool ModUI_InputTextMultiline(const char* label, char* buffer, uint32_t capacity, float width, float height, uint32_t flags) {
    if (label == nullptr || buffer == nullptr || capacity == 0 || !ValidTextFlags(flags)) {
        return false;
    }
    UIWidgets::PushStyleInput(THEME_COLOR);
    const bool edited = ImGui::InputTextMultiline(label, buffer, capacity, ImVec2(width, height), flags);
    UIWidgets::PopStyleInput();
    return edited;
}

bool ModUI_InputTextWithHint(const char* label, const char* hint, char* buffer, uint32_t capacity, uint32_t flags) {
    if (label == nullptr || buffer == nullptr || capacity == 0 || !ValidTextFlags(flags)) {
        return false;
    }
    UIWidgets::PushStyleInput(THEME_COLOR);
    const bool edited = ImGui::InputTextWithHint(label, hint ? hint : "", buffer, capacity, flags);
    UIWidgets::PopStyleInput();
    return edited;
}

bool ModUI_InputFloat(const char* label, float* value, float step, float fastStep) { return label && value && ImGui::InputFloat(label, value, step, fastStep); }
bool ModUI_InputDouble(const char* label, double* value, double step, double fastStep) { return label && value && ImGui::InputDouble(label, value, step, fastStep); }

bool ModUI_SliderInt(const char* label, int32_t* value, int32_t minimum, int32_t maximum) {
    if (label == nullptr || value == nullptr || minimum > maximum) {
        return false;
    }
    UIWidgets::PushStyleSlider(THEME_COLOR);
    const bool changed = ImGui::SliderInt(label, value, minimum, maximum);
    UIWidgets::PopStyleSlider();
    return changed;
}

bool ModUI_DragInt(const char* label, int32_t* value, float speed, int32_t minimum, int32_t maximum) {
    if (label == nullptr || value == nullptr) {
        return false;
    }
    UIWidgets::PushStyleSlider(THEME_COLOR);
    const bool changed = ImGui::DragInt(label, value, speed, minimum, maximum);
    UIWidgets::PopStyleSlider();
    return changed;
}

bool ModUI_SliderIntN(const char* label, int32_t* values, int32_t count, int32_t minimum, int32_t maximum) { return label && values && count > 0 && count <= 4 && minimum <= maximum && ImGui::SliderScalarN(label, ImGuiDataType_S32, values, count, &minimum, &maximum); }

bool ModUI_SliderFloat(const char* label, float* value, float minimum, float maximum) {
    if (label == nullptr || value == nullptr || minimum > maximum) {
        return false;
    }
    UIWidgets::PushStyleSlider(THEME_COLOR);
    const bool changed = ImGui::SliderFloat(label, value, minimum, maximum);
    UIWidgets::PopStyleSlider();
    return changed;
}

bool ModUI_DragFloat(const char* label, float* value, float speed, float minimum, float maximum) {
    if (label == nullptr || value == nullptr) {
        return false;
    }
    UIWidgets::PushStyleSlider(THEME_COLOR);
    const bool changed = ImGui::DragFloat(label, value, speed, minimum, maximum);
    UIWidgets::PopStyleSlider();
    return changed;
}

bool ModUI_SliderFloatN(const char* label, float* values, int32_t count, float minimum, float maximum) { return label && values && count > 0 && count <= 4 && minimum <= maximum && ImGui::SliderScalarN(label, ImGuiDataType_Float, values, count, &minimum, &maximum); }
bool ModUI_ColorEdit3(const char* label, float* rgba, uint32_t flags) { return label && rgba && ImGui::ColorEdit3(label, rgba, flags); }
bool ModUI_ColorEdit4(const char* label, float* rgba, uint32_t flags) { return label && rgba && ImGui::ColorEdit4(label, rgba, flags); }
bool ModUI_ColorPicker3(const char* label, float* rgba, uint32_t flags) { return label && rgba && ImGui::ColorPicker3(label, rgba, flags); }
bool ModUI_ColorPicker4(const char* label, float* rgba, uint32_t flags) { return label && rgba && ImGui::ColorPicker4(label, rgba, flags); }
bool ModUI_ColorButton(const char* label, const float* rgba, uint32_t flags, float width, float height) { return label && rgba && ImGui::ColorButton(label, ImVec4(rgba[0],rgba[1],rgba[2],rgba[3]), flags, ImVec2(width,height)); }
void ModUI_TextWrapped(const char* text) { if (text) ImGui::TextWrapped("%s", text); }
void ModUI_TextColored(const float* rgba, const char* text) { if (rgba && text) ImGui::TextColored(ImVec4(rgba[0],rgba[1],rgba[2],rgba[3]), "%s", text); }
void ModUI_TextDisabled(const char* text) { if (text) ImGui::TextDisabled("%s", text); }
void ModUI_BulletText(const char* text) { if (text) ImGui::BulletText("%s", text); }
void ModUI_ProgressBar(float fraction, float width, float height, const char* overlay) { ImGui::ProgressBar(fraction, ImVec2(width, height), overlay); }
void ModUI_PlotLines(const char* label, const float* values, int32_t count, float minimum, float maximum, float width, float height) { if (label && values && count > 0) ImGui::PlotLines(label, values, count, 0, nullptr, minimum, maximum, ImVec2(width,height)); }
void ModUI_PlotHistogram(const char* label, const float* values, int32_t count, float minimum, float maximum, float width, float height) { if (label && values && count > 0) ImGui::PlotHistogram(label, values, count, 0, nullptr, minimum, maximum, ImVec2(width,height)); }
void ModUI_Image(uintptr_t texture, float width, float height) { ImGui::Image((ImTextureID)texture, ImVec2(width,height)); }
bool ModUI_ImageButton(const char* id, uintptr_t texture, float width, float height) { return id && ImGui::ImageButton(id, (ImTextureID)texture, ImVec2(width,height)); }
bool ModUI_BeginWindow(const char* name, bool* open, uint32_t flags) { return ImGui::Begin(name ? name : "Mod window", open, flags); }
void ModUI_EndWindow(void) { ImGui::End(); }
void ModUI_SetNextWindowSize(float width, float height, uint32_t condition) { ImGui::SetNextWindowSize(ImVec2(width,height), condition); }
void ModUI_SetNextWindowPosition(float x, float y, uint32_t condition) { ImGui::SetNextWindowPos(ImVec2(x,y), condition); }
bool ModUI_BeginChild(const char* id, float width, float height, bool border, uint32_t flags) { return ImGui::BeginChild(id ? id : "Mod child", ImVec2(width,height), border ? ImGuiChildFlags_Borders : 0, flags); }
void ModUI_EndChild(void) { ImGui::EndChild(); }

// Same as BeginCombo: the tab theme is popped by EndTabBar once the bar is open.
bool ModUI_BeginTabBar(const char* id, uint32_t flags) {
    if (id == nullptr) {
        return false;
    }
    UIWidgets::PushStyleTabs(THEME_COLOR);
    const bool open = ImGui::BeginTabBar(id, flags);
    if (!open) {
        UIWidgets::PopStyleTabs();
    }
    return open;
}

void ModUI_EndTabBar(void) {
    ImGui::EndTabBar();
    UIWidgets::PopStyleTabs();
}

bool ModUI_BeginTabItem(const char* label, bool* open, uint32_t flags) { return label && ImGui::BeginTabItem(label, open, flags); }
void ModUI_EndTabItem(void) { ImGui::EndTabItem(); }
bool ModUI_TreeNode(const char* label, uint32_t flags) { return label && ImGui::TreeNodeEx(label, flags); }
void ModUI_TreePop(void) { ImGui::TreePop(); }
bool ModUI_CollapsingHeader(const char* label, uint32_t flags) { return label && ImGui::CollapsingHeader(label, flags); }
bool ModUI_BeginTable(const char* id, int32_t columns, uint32_t flags, float width, float height) { return id && columns > 0 && columns <= 64 && ImGui::BeginTable(id, columns, flags, ImVec2(width,height)); }
void ModUI_EndTable(void) { ImGui::EndTable(); }
void ModUI_TableSetupColumn(const char* label, uint32_t flags, float widthOrWeight) { ImGui::TableSetupColumn(label ? label : "", flags, widthOrWeight); }
void ModUI_TableHeadersRow(void) { ImGui::TableHeadersRow(); }
void ModUI_TableNextRow(void) { ImGui::TableNextRow(); }
bool ModUI_TableNextColumn(void) { return ImGui::TableNextColumn(); }
bool ModUI_TableSetColumnIndex(int32_t column) { return column >= 0 && column < ImGui::TableGetColumnCount() && ImGui::TableSetColumnIndex(column); }
void ModUI_OpenPopup(const char* id) { if (id) ImGui::OpenPopup(id); }
bool ModUI_BeginPopup(const char* id, uint32_t flags) { return id && ImGui::BeginPopup(id, flags); }
bool ModUI_BeginPopupModal(const char* name, bool* open, uint32_t flags) { return name && ImGui::BeginPopupModal(name, open, flags); }
bool ModUI_BeginPopupContextItem(const char* id) { return ImGui::BeginPopupContextItem(id); }
void ModUI_EndPopup(void) { ImGui::EndPopup(); }
void ModUI_CloseCurrentPopup(void) { ImGui::CloseCurrentPopup(); }
bool ModUI_BeginMenuBar(void) { return ImGui::BeginMenuBar(); }
void ModUI_EndMenuBar(void) { ImGui::EndMenuBar(); }
bool ModUI_BeginMenu(const char* label, bool enabled) { return label && ImGui::BeginMenu(label, enabled); }
void ModUI_EndMenu(void) { ImGui::EndMenu(); }
bool ModUI_MenuItem(const char* label, const char* shortcut, bool selected, bool enabled) { return label && ImGui::MenuItem(label, shortcut, selected, enabled); }
void ModUI_Separator(void) { ImGui::Separator(); }
void ModUI_Spacing(void) { ImGui::Spacing(); }
void ModUI_NewLine(void) { ImGui::NewLine(); }
void ModUI_BeginGroup(void) { ImGui::BeginGroup(); }
void ModUI_EndGroup(void) { ImGui::EndGroup(); }
void ModUI_EndDisabled(void) { ImGui::EndDisabled(); }
void ModUI_EndTooltip(void) { ImGui::EndTooltip(); }
void ModUI_SetItemDefaultFocus(void) { ImGui::SetItemDefaultFocus(); }
void ModUI_SetNextItemAllowOverlap(void) { ImGui::SetNextItemAllowOverlap(); }
void ModUI_SeparatorText(const char* text) { if (text) ImGui::SeparatorText(text); }
void ModUI_SameLine(float offset, float spacing) { ImGui::SameLine(offset, spacing); }
void ModUI_Dummy(float width, float height) { ImGui::Dummy(ImVec2(width,height)); }
void ModUI_Indent(float width) { ImGui::Indent(width); }
void ModUI_Unindent(float width) { ImGui::Unindent(width); }
void ModUI_SetNextItemWidth(float width) { ImGui::SetNextItemWidth(width); }
void ModUI_PushItemWidth(float width) { ImGui::PushItemWidth(width); }
void ModUI_PopItemWidth(void) { ImGui::PopItemWidth(); }
void ModUI_PushID(const char* id) { ImGui::PushID(id ? id : ""); }
void ModUI_PopID(void) { ImGui::PopID(); }
void ModUI_BeginDisabled(bool disabled) { ImGui::BeginDisabled(disabled); }
bool ModUI_BeginTooltip(void) { return ImGui::BeginTooltip(); }
void ModUI_SetTooltip(const char* text) { if (text) ImGui::SetTooltip("%s", text); }
bool ModUI_IsItemHovered(void) { return ImGui::IsItemHovered(); }
bool ModUI_IsItemActive(void) { return ImGui::IsItemActive(); }
bool ModUI_IsItemEdited(void) { return ImGui::IsItemEdited(); }
bool ModUI_IsItemDeactivatedAfterEdit(void) { return ImGui::IsItemDeactivatedAfterEdit(); }
bool ModUI_IsItemVisible(void) { return ImGui::IsItemVisible(); }
bool ModUI_IsWindowAppearing(void) { return ImGui::IsWindowAppearing(); }
bool ModUI_IsWindowCollapsed(void) { return ImGui::IsWindowCollapsed(); }
bool ModUI_IsItemClicked(int32_t button) { return button >= 0 && button < ImGuiMouseButton_COUNT && ImGui::IsItemClicked(button); }
void ModUI_SetKeyboardFocusHere(int32_t offset) { ImGui::SetKeyboardFocusHere(offset); }
void ModUI_SetScrollHereY(float ratio) { ImGui::SetScrollHereY(ratio); }
float ModUI_GetScrollX(void) { return ImGui::GetScrollX(); }
float ModUI_GetScrollY(void) { return ImGui::GetScrollY(); }
float ModUI_GetScrollMaxX(void) { return ImGui::GetScrollMaxX(); }
float ModUI_GetScrollMaxY(void) { return ImGui::GetScrollMaxY(); }
float ModUI_GetTextLineHeightWithSpacing(void) { return ImGui::GetTextLineHeightWithSpacing(); }
float ModUI_GetFrameHeightWithSpacing(void) { return ImGui::GetFrameHeightWithSpacing(); }
void ModUI_SetScrollX(float value) { ImGui::SetScrollX(value); }
void ModUI_SetScrollY(float value) { ImGui::SetScrollY(value); }
void ModUI_GetContentRegionAvailable(float* width, float* height) { ImVec2 size = ImGui::GetContentRegionAvail(); if (width) *width=size.x; if (height) *height=size.y; }
void ModUI_GetCursorPosition(float* x, float* y) { ImVec2 pos = ImGui::GetCursorPos(); if (x) *x=pos.x; if (y) *y=pos.y; }
void ModUI_SetCursorPosition(float x, float y) { ImGui::SetCursorPos(ImVec2(x,y)); }
void ModUI_SetClipboardText(const char* text) { if (text) ImGui::SetClipboardText(text); }
const char* ModUI_GetClipboardText(void) { return ImGui::GetClipboardText(); }
void ModUI_PushStyleColor(int32_t index, const float* rgba) { if (rgba && index >= 0 && index < ImGuiCol_COUNT) ImGui::PushStyleColor(index, ImVec4(rgba[0],rgba[1],rgba[2],rgba[3])); }
void ModUI_PopStyleColor(int32_t count) { if (count > 0) ImGui::PopStyleColor(count); }
void ModUI_PushStyleFloat(int32_t index, float value) { if (index >= 0 && index < ImGuiStyleVar_COUNT) ImGui::PushStyleVar(index, value); }
void ModUI_PushStyleVec2(int32_t index, float x, float y) { if (index >= 0 && index < ImGuiStyleVar_COUNT) ImGui::PushStyleVar(index, ImVec2(x,y)); }
void ModUI_PopStyleVar(int32_t count) { if (count > 0) ImGui::PopStyleVar(count); }

bool ModUI_CheckboxFlags(const char* label, uint32_t* value, uint32_t flags) { return label && value && ImGui::CheckboxFlags(label,value,flags); }
bool ModUI_InvisibleButton(const char* id, float width, float height, uint32_t flags) { return id && width > 0 && height > 0 && ImGui::InvisibleButton(id,ImVec2(width,height),flags); }
bool ModUI_ArrowButton(const char* id, int32_t direction) { return id && direction >= 0 && direction < ImGuiDir_COUNT && ImGui::ArrowButton(id,static_cast<ImGuiDir>(direction)); }
bool ModUI_DragIntN(const char* label, int32_t* values, int32_t count, float speed, int32_t minimum, int32_t maximum) { return label && values && count > 0 && count <= 4 && ImGui::DragScalarN(label,ImGuiDataType_S32,values,count,speed,&minimum,&maximum); }
bool ModUI_InputIntN(const char* label, int32_t* values, int32_t count) { return label && values && count > 0 && count <= 4 && ImGui::InputScalarN(label,ImGuiDataType_S32,values,count); }
bool ModUI_VSliderInt(const char* label, float width, float height, int32_t* value, int32_t minimum, int32_t maximum) { return label && value && minimum <= maximum && ImGui::VSliderInt(label,ImVec2(width,height),value,minimum,maximum); }
bool ModUI_DragFloatN(const char* label, float* values, int32_t count, float speed, float minimum, float maximum) { return label && values && count > 0 && count <= 4 && ImGui::DragScalarN(label,ImGuiDataType_Float,values,count,speed,&minimum,&maximum); }
bool ModUI_InputFloatN(const char* label, float* values, int32_t count) { return label && values && count > 0 && count <= 4 && ImGui::InputScalarN(label,ImGuiDataType_Float,values,count); }
bool ModUI_VSliderFloat(const char* label, float width, float height, float* value, float minimum, float maximum) { return label && value && minimum <= maximum && ImGui::VSliderFloat(label,ImVec2(width,height),value,minimum,maximum); }
bool ModUI_SliderAngle(const char* label, float* radians, float minDegrees, float maxDegrees) { return label && radians && minDegrees <= maxDegrees && ImGui::SliderAngle(label,radians,minDegrees,maxDegrees); }
void ModUI_LabelText(const char* label, const char* text) { if(label && text) ImGui::LabelText(label,"%s",text); }
void ModUI_Bullet(void) { ImGui::Bullet(); }
bool ModUI_BeginMainMenuBar(void) { return ImGui::BeginMainMenuBar(); }
void ModUI_EndMainMenuBar(void) { ImGui::EndMainMenuBar(); }
bool ModUI_TabItemButton(const char* label, uint32_t flags) { return label && ImGui::TabItemButton(label,flags); }
void ModUI_SetTabItemClosed(const char* label) { if(label)ImGui::SetTabItemClosed(label); }
void ModUI_SetNextItemOpen(bool open, uint32_t condition) { ImGui::SetNextItemOpen(open,condition); }
bool ModUI_BeginDragDropSource(uint32_t flags) { return ImGui::BeginDragDropSource(flags); }
bool ModUI_SetDragDropPayload(const char* type, const void* data, uint32_t size, uint32_t condition) { return type && (!size || data) && ImGui::SetDragDropPayload(type,data,size,condition); }
void ModUI_EndDragDropSource(void) { ImGui::EndDragDropSource(); }
bool ModUI_BeginDragDropTarget(void) { return ImGui::BeginDragDropTarget(); }
uint32_t ModUI_AcceptDragDropPayload(const char* type, void* output, uint32_t capacity, bool* delivered) { if(delivered)*delivered=false; if(!type)return 0; const ImGuiPayload* p=ImGui::AcceptDragDropPayload(type); if(!p)return 0; if(delivered)*delivered=p->IsDelivery(); if(output && capacity >= uint32_t(p->DataSize)) std::memcpy(output,p->Data,p->DataSize); return uint32_t(p->DataSize); }
void ModUI_EndDragDropTarget(void) { ImGui::EndDragDropTarget(); }
void ModUI_GetItemRectangle(float* x1, float* y1, float* x2, float* y2) { ImVec2 a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax(); if(x1)*x1=a.x;if(y1)*y1=a.y;if(x2)*x2=b.x;if(y2)*y2=b.y; }
void ModUI_GetCursorScreenPosition(float* x, float* y) { ImVec2 p=ImGui::GetCursorScreenPos();if(x)*x=p.x;if(y)*y=p.y; }
void ModUI_SetCursorScreenPosition(float x, float y) { ImGui::SetCursorScreenPos(ImVec2(x,y)); }
void ModUI_DrawLine(float x1, float y1, float x2, float y2, uint32_t rgba, float thickness) { ImGui::GetWindowDrawList()->AddLine(ImVec2(x1,y1),ImVec2(x2,y2),rgba,thickness); }
void ModUI_DrawRect(float x1, float y1, float x2, float y2, uint32_t rgba, float rounding, float thickness) { ImGui::GetWindowDrawList()->AddRect(ImVec2(x1,y1),ImVec2(x2,y2),rgba,rounding,0,thickness); }
void ModUI_DrawRectFilled(float x1, float y1, float x2, float y2, uint32_t rgba, float rounding) { ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x1,y1),ImVec2(x2,y2),rgba,rounding); }
void ModUI_DrawCircle(float x, float y, float radius, uint32_t rgba, float thickness) { if(radius>0)ImGui::GetWindowDrawList()->AddCircle(ImVec2(x,y),radius,rgba,0,thickness); }
void ModUI_DrawCircleFilled(float x, float y, float radius, uint32_t rgba) { if(radius>0)ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(x,y),radius,rgba); }
void ModUI_DrawText(float x, float y, uint32_t rgba, const char* text) { if(text)ImGui::GetWindowDrawList()->AddText(ImVec2(x,y),rgba,text); }
