#include "test_require.h"
#include <imgui.h>
#include <cstring>
#include <map>
#include <string>

// Exercise the production map Combobox template with real, headless ImGui.
// Only engine-owned styling/options are stubbed; the lookup and draw are real.
namespace UIWidgets {
enum class ComponentAlignments { Left, Right };
enum class LabelPositions { None, Above, Near, Far };
struct ComboboxOptions {
    bool disabled = false;
    int color = 0;
    ImGuiComboFlags flags = 0;
    ComponentAlignments alignment = ComponentAlignments::Left;
    LabelPositions labelPosition = LabelPositions::Far;
    std::string disabledTooltip;
    std::string tooltip;
};
void PushStyleCombobox(int) {
}
void PopStyleCombobox() {
}
float CalcComboWidth(const char* text, ImGuiComboFlags) {
    REQUIRE(text != nullptr);
    return ImGui::CalcTextSize(text).x + 40.0f;
}
std::string WrappedText(const std::string& text) {
    return text;
}
#include "pak_menu_combobox.inc"
} // namespace UIWidgets

static void Draw(const std::map<int, const char*>& options, int value, bool disabled = false) {
    const int saved = value;
    ImGui::NewFrame();
    ImGui::Begin("PAK selection regression");
    UIWidgets::ComboboxOptions ui;
    ui.disabled = disabled;
    const bool changed = UIWidgets::Combobox("Model", &value, options, ui);
    REQUIRE(!changed);
    REQUIRE(value == saved); // Drawing a stale selection must not silently choose another model.
    ImGui::End();
    ImGui::Render();
}

int main() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // The user's exact failure: saved Child=59 now names adult-only Thic Ruto.
    const std::map<int, const char*> children = { { -1, "Default Link" },
                                                  { 1, "Child model" },
                                                  { 56, "Definitive Child Saria" } };
    Draw(children, 59);
    Draw(children, 59, true); // Disabled widgets still render their previews.
    Draw(children, -2);
    Draw(children, -1);
    Draw(children, 56);
    Draw({}, 0);
    Draw({ { 0, "" } }, 0);
    Draw({ { 0, nullptr } }, 0);
    Draw({ { 0, "A" } }, 0);
    ImGui::DestroyContext();
    puts("PASS production map Combobox: stale/disabled/empty/valid selections");
}
