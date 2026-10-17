#ifndef SOH_MOD_UI_H
#define SOH_MOD_UI_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// General immediate-mode UI for every mod, within a UI draw callback.
// Flags/index values follow the ImGui version returned by ModUI_GetImGuiVersion.
// Zero flags give defaults. No callbacks or C++ types cross this ABI.
// BeginWindow/BeginChild ALWAYS require End, even when false.
// Other Begin functions require End only when true. Match all Push/Pop pairs.
// Caller owns writable buffers and must provide their actual capacity.
uint32_t ModUI_GetVersion(void);
const char* ModUI_GetImGuiVersion(void);
bool ModUI_Button(const char* label, float width, float height);
bool ModUI_SmallButton(const char* label);
bool ModUI_RadioButton(const char* label, int32_t* value, int32_t choice);
bool ModUI_Combo(const char* label, int32_t* selected, const char* const* items, int32_t count);
bool ModUI_BeginCombo(const char* label, const char* preview, uint32_t flags);
void ModUI_EndCombo(void);
bool ModUI_Selectable(const char* label, bool selected, uint32_t flags, float width, float height);
bool ModUI_ListBox(const char* label, int32_t* selected, const char* const* items, int32_t count, int32_t rows);
bool ModUI_BeginListBox(const char* label, float width, float height);
void ModUI_EndListBox(void);
bool ModUI_InputText(const char* label, char* buffer, uint32_t capacity, uint32_t flags);
bool ModUI_InputTextMultiline(const char* label, char* buffer, uint32_t capacity, float width, float height, uint32_t flags);
bool ModUI_InputTextWithHint(const char* label, const char* hint, char* buffer, uint32_t capacity, uint32_t flags);
bool ModUI_InputFloat(const char* label, float* value, float step, float fastStep);
bool ModUI_InputDouble(const char* label, double* value, double step, double fastStep);
bool ModUI_SliderInt(const char* label, int32_t* value, int32_t minimum, int32_t maximum);
bool ModUI_DragInt(const char* label, int32_t* value, float speed, int32_t minimum, int32_t maximum);
bool ModUI_SliderIntN(const char* label, int32_t* values, int32_t count, int32_t minimum, int32_t maximum);
bool ModUI_SliderFloat(const char* label, float* value, float minimum, float maximum);
bool ModUI_DragFloat(const char* label, float* value, float speed, float minimum, float maximum);
bool ModUI_SliderFloatN(const char* label, float* values, int32_t count, float minimum, float maximum);
bool ModUI_ColorEdit3(const char* label, float* rgba, uint32_t flags);
bool ModUI_ColorEdit4(const char* label, float* rgba, uint32_t flags);
bool ModUI_ColorPicker3(const char* label, float* rgba, uint32_t flags);
bool ModUI_ColorPicker4(const char* label, float* rgba, uint32_t flags);
bool ModUI_ColorButton(const char* label, const float* rgba, uint32_t flags, float width, float height);
void ModUI_TextWrapped(const char* text);
void ModUI_TextColored(const float* rgba, const char* text);
void ModUI_TextDisabled(const char* text);
void ModUI_BulletText(const char* text);
void ModUI_ProgressBar(float fraction, float width, float height, const char* overlay);
void ModUI_PlotLines(const char* label, const float* values, int32_t count, float minimum, float maximum, float width, float height);
void ModUI_PlotHistogram(const char* label, const float* values, int32_t count, float minimum, float maximum, float width, float height);
void ModUI_Image(uintptr_t texture, float width, float height);
bool ModUI_ImageButton(const char* id, uintptr_t texture, float width, float height);
bool ModUI_BeginWindow(const char* name, bool* open, uint32_t flags);
void ModUI_EndWindow(void);
void ModUI_SetNextWindowSize(float width, float height, uint32_t condition);
void ModUI_SetNextWindowPosition(float x, float y, uint32_t condition);
bool ModUI_BeginChild(const char* id, float width, float height, bool border, uint32_t flags);
void ModUI_EndChild(void);
bool ModUI_BeginTabBar(const char* id, uint32_t flags);
void ModUI_EndTabBar(void);
bool ModUI_BeginTabItem(const char* label, bool* open, uint32_t flags);
void ModUI_EndTabItem(void);
bool ModUI_TreeNode(const char* label, uint32_t flags);
void ModUI_TreePop(void);
bool ModUI_CollapsingHeader(const char* label, uint32_t flags);
bool ModUI_BeginTable(const char* id, int32_t columns, uint32_t flags, float width, float height);
void ModUI_EndTable(void);
void ModUI_TableSetupColumn(const char* label, uint32_t flags, float widthOrWeight);
void ModUI_TableHeadersRow(void);
void ModUI_TableNextRow(void);
bool ModUI_TableNextColumn(void);
bool ModUI_TableSetColumnIndex(int32_t column);
void ModUI_OpenPopup(const char* id);
bool ModUI_BeginPopup(const char* id, uint32_t flags);
bool ModUI_BeginPopupModal(const char* name, bool* open, uint32_t flags);
bool ModUI_BeginPopupContextItem(const char* id);
void ModUI_EndPopup(void);
void ModUI_CloseCurrentPopup(void);
bool ModUI_BeginMenuBar(void);
void ModUI_EndMenuBar(void);
bool ModUI_BeginMenu(const char* label, bool enabled);
void ModUI_EndMenu(void);
bool ModUI_MenuItem(const char* label, const char* shortcut, bool selected, bool enabled);
void ModUI_Separator(void);
void ModUI_Spacing(void);
void ModUI_NewLine(void);
void ModUI_BeginGroup(void);
void ModUI_EndGroup(void);
void ModUI_EndDisabled(void);
void ModUI_EndTooltip(void);
void ModUI_SetItemDefaultFocus(void);
void ModUI_SetNextItemAllowOverlap(void);
void ModUI_SeparatorText(const char* text);
void ModUI_SameLine(float offset, float spacing);
void ModUI_Dummy(float width, float height);
void ModUI_Indent(float width);
void ModUI_Unindent(float width);
void ModUI_SetNextItemWidth(float width);
void ModUI_PushItemWidth(float width);
void ModUI_PopItemWidth(void);
void ModUI_PushID(const char* id);
void ModUI_PopID(void);
void ModUI_BeginDisabled(bool disabled);
bool ModUI_BeginTooltip(void);
void ModUI_SetTooltip(const char* text);
bool ModUI_IsItemHovered(void);
bool ModUI_IsItemActive(void);
bool ModUI_IsItemEdited(void);
bool ModUI_IsItemDeactivatedAfterEdit(void);
bool ModUI_IsItemVisible(void);
bool ModUI_IsWindowAppearing(void);
bool ModUI_IsWindowCollapsed(void);
bool ModUI_IsItemClicked(int32_t button);
void ModUI_SetKeyboardFocusHere(int32_t offset);
void ModUI_SetScrollHereY(float ratio);
float ModUI_GetScrollX(void);
float ModUI_GetScrollY(void);
float ModUI_GetScrollMaxX(void);
float ModUI_GetScrollMaxY(void);
float ModUI_GetTextLineHeightWithSpacing(void);
float ModUI_GetFrameHeightWithSpacing(void);
void ModUI_SetScrollX(float value);
void ModUI_SetScrollY(float value);
void ModUI_GetContentRegionAvailable(float* width, float* height);
void ModUI_GetCursorPosition(float* x, float* y);
void ModUI_SetCursorPosition(float x, float y);
void ModUI_SetClipboardText(const char* text);
const char* ModUI_GetClipboardText(void);
void ModUI_PushStyleColor(int32_t index, const float* rgba);
void ModUI_PopStyleColor(int32_t count);
void ModUI_PushStyleFloat(int32_t index, float value);
void ModUI_PushStyleVec2(int32_t index, float x, float y);
void ModUI_PopStyleVar(int32_t count);
bool ModUI_CheckboxFlags(const char* label, uint32_t* value, uint32_t flags);
bool ModUI_InvisibleButton(const char* id, float width, float height, uint32_t flags);
bool ModUI_ArrowButton(const char* id, int32_t direction);
bool ModUI_DragIntN(const char* label, int32_t* values, int32_t count, float speed, int32_t minimum, int32_t maximum);
bool ModUI_InputIntN(const char* label, int32_t* values, int32_t count);
bool ModUI_VSliderInt(const char* label, float width, float height, int32_t* value, int32_t minimum, int32_t maximum);
bool ModUI_DragFloatN(const char* label, float* values, int32_t count, float speed, float minimum, float maximum);
bool ModUI_InputFloatN(const char* label, float* values, int32_t count);
bool ModUI_VSliderFloat(const char* label, float width, float height, float* value, float minimum, float maximum);
bool ModUI_SliderAngle(const char* label, float* radians, float minDegrees, float maxDegrees);
void ModUI_LabelText(const char* label, const char* text);
void ModUI_Bullet(void);
bool ModUI_BeginMainMenuBar(void);
void ModUI_EndMainMenuBar(void);
bool ModUI_TabItemButton(const char* label, uint32_t flags);
void ModUI_SetTabItemClosed(const char* label);
void ModUI_SetNextItemOpen(bool open, uint32_t condition);
bool ModUI_BeginDragDropSource(uint32_t flags);
bool ModUI_SetDragDropPayload(const char* type, const void* data, uint32_t size, uint32_t condition);
void ModUI_EndDragDropSource(void);
bool ModUI_BeginDragDropTarget(void);
uint32_t ModUI_AcceptDragDropPayload(const char* type, void* output, uint32_t capacity, bool* delivered);
void ModUI_EndDragDropTarget(void);
void ModUI_GetItemRectangle(float* x1, float* y1, float* x2, float* y2);
void ModUI_GetCursorScreenPosition(float* x, float* y);
void ModUI_SetCursorScreenPosition(float x, float y);
void ModUI_DrawLine(float x1, float y1, float x2, float y2, uint32_t rgba, float thickness);
void ModUI_DrawRect(float x1, float y1, float x2, float y2, uint32_t rgba, float rounding, float thickness);
void ModUI_DrawRectFilled(float x1, float y1, float x2, float y2, uint32_t rgba, float rounding);
void ModUI_DrawCircle(float x, float y, float radius, uint32_t rgba, float thickness);
void ModUI_DrawCircleFilled(float x, float y, float radius, uint32_t rgba);
void ModUI_DrawText(float x, float y, uint32_t rgba, const char* text);
#ifdef __cplusplus
}
#endif
#endif
