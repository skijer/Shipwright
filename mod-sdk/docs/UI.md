# General mod UI

Include `soh/ModApi/ModMenu/ModMenu.h` for menu registration and `soh/ModApi/ModMenu/ModUI.h` for the general C controls shared by all mods. Register a `SOH_MOD_MENU_CUSTOM` widget and draw controls in its callback. No Randomizer option or seed is required.

Existing menu structs, enum values and ModApi table offsets are unchanged. Old mods retain their existing interface. New mods importing these symbols require a host that exports them. `ModUI_GetVersion()` returns 1; `ModUI_GetImGuiVersion()` identifies the host UI version.

## Controls

- Actions: Button, SmallButton, RadioButton, MenuItem, ImageButton; existing Checkbox remains available.
- Selection: Combo, BeginCombo/EndCombo, Selectable, ListBox, BeginListBox/EndListBox.
- Editing/search: InputText, InputTextWithHint, InputTextMultiline, InputFloat, InputDouble; existing InputInt remains available.
- Numeric controls: SliderInt/Float, SliderIntN/FloatN, DragInt/Float. N variants accept 1–4 components.
- Colors: ColorEdit3/4, ColorPicker3/4, ColorButton.
- Output: TextWrapped, TextColored, TextDisabled, BulletText, ProgressBar, PlotLines, PlotHistogram, Image; existing Text remains available.
- Containers: windows, child regions (scrolling), tab bars/items, trees, collapsing headers, tables, popups, context popups, modal popups and menu bars/menus.
- Layout: SameLine, Separator, SeparatorText, Spacing, NewLine, Dummy, groups, Indent/Unindent, cursor position, item widths and available region size.
- Interaction: disabled scopes, focus, item activity/edit/hover/click queries, tooltips, scroll position, style colors/variables, IDs and clipboard.
- Drag and drop: source/target scopes, copied payload data and delivery query. AcceptDragDropPayload returns the required byte count; output is copied only when capacity suffices.
- Canvas: lines, rectangles, circles and text, including filled shapes, plus screen-space cursor/item rectangles. Packed colors follow ImGui (0xAABBGGRR).

All functions have the `ModUI_` prefix; exact C signatures and contracts are in `ModUI.h`. Texture handles refer to host-renderer textures; they are not OTR paths or raw pixel pointers. Flags and style indices use the host ImGui version; zero flags select default behavior. No C++ objects or callbacks cross this ABI.

## Lifetime and balanced scopes

Call UI functions during a UI draw callback only. Always call EndWindow/EndChild after BeginWindow/BeginChild, even if Begin returns false. For other Begin/End pairs, call End only if Begin returned true. Balance Push/Pop, Indent/Unindent and BeginDisabled/EndDisabled calls. TreeNode with the NoTreePushOnOpen flag does not require TreePop, matching ImGui.

Text buffers belong to the caller, must be null-terminated, and capacity must describe the actual writable buffer. InputText callback flags (completion/history/always/filter/resize/edit) are rejected because the C interface does not accept callback functions. No printf strings are interpreted in text/tooltip helpers. Clipboard read pointers are host-owned and temporary; copy them if retaining text.

Example inside a custom widget callback:

```c
static char search[128] = "";
static int32_t selected;
static const char* const choices[] = { "Vault", "Spin", "Slash" };
ModUI_InputTextWithHint("Search", "Animation name...", search, sizeof(search), 0);
ModUI_Combo("Action", &selected, choices, 3);
if (ModUI_Button("Play", 0, 0)) { /* start preview */ }
if (ModUI_BeginChild("Results", 0, 220, true, 0)) {
    if (ModUI_Selectable("Clip", selected == 0, 0, 0, 0)) selected = 0;
}
ModUI_EndChild();
```
