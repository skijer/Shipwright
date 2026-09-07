// =============================================================================
// KafeiStaminaHud — BotW-style stamina wheel floating above Kafei's head.
//
// A GuiWindow, not an Interface_Draw call: foreground-drawlist additions made
// after the ImGui frame is rendered are discarded, and GetForegroundDrawList()
// crashes there. Same arrangement as Sm64CapsHud.
// =============================================================================

#include <imgui.h>
#include <cmath>
#include <memory>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <fast/Fast3dGui.h>

extern "C" {
#include "z64.h"
extern PlayState* gPlayState;

void Actor_ProjectPos(PlayState* play, Vec3f* worldPos, Vec3f* projectedPos, f32* invW);

u8 KafeiForm_WheelCount(void);
f32 KafeiForm_WheelFill(u8 wheel);
u8 KafeiForm_MeterVisible(void);
u8 KafeiForm_IsWinded(void);
}

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kHeadOffset = 62.0f; // OOT units above the actor origin

std::shared_ptr<Ship::GuiWindow> sHudWindow = nullptr;

class KafeiStaminaHudWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
    void Draw() override;
};

void KafeiStaminaHudWindow::Draw() {
    if (gPlayState == nullptr || gPlayState->pauseCtx.state != 0) {
        return;
    }
    if (!KafeiForm_MeterVisible()) {
        return;
    }

    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if (gui != nullptr && gui->GetMenuOrMenubarVisible()) {
        return;
    }

    Player* player = (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].head;
    if (player == nullptr) {
        return;
    }

    Vec3f world = player->actor.world.pos;
    world.y += kHeadOffset;
    Vec3f proj;
    f32 invW;
    Actor_ProjectPos(gPlayState, &world, &proj, &invW);

    // Behind the camera projects to a mirrored point that would draw on top of you.
    if (invW <= 0.0f) {
        return;
    }

    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return;
    }

    // Anchor to the VIEWPORT, not to DisplaySize. The foreground draw list works in
    // absolute desktop coordinates, and windowed mode gives the viewport a non-zero
    // origin — ignoring it put the wheel outside the window, so it only ever showed
    // in fullscreen, where the origin happens to be (0,0).
    ImVec2 disp = viewport->Size;
    if (disp.x < 1.0f || disp.y < 1.0f) {
        return;
    }
    float sx = viewport->Pos.x + (proj.x * invW * 0.5f + 0.5f) * disp.x;
    float sy = viewport->Pos.y + (proj.y * invW * -0.5f + 0.5f) * disp.y;
    if (sx < viewport->Pos.x || sx > viewport->Pos.x + disp.x || sy < viewport->Pos.y ||
        sy > viewport->Pos.y + disp.y) {
        return;
    }

    // Shrink with distance so it reads as attached to him rather than pasted on.
    float scale = disp.y / 720.0f;
    float radius = 26.0f * scale * (invW * 220.0f);
    if (radius < 9.0f * scale) {
        radius = 9.0f * scale;
    }
    if (radius > 34.0f * scale) {
        radius = 34.0f * scale;
    }
    float thickness = radius * 0.28f;

    ImDrawList* dl = ImGui::GetForegroundDrawList(viewport);
    if (dl == nullptr) {
        return;
    }
    ImVec2 center(sx, sy);

    u8 winded = KafeiForm_IsWinded();
    u8 wheels = KafeiForm_WheelCount();

    for (u8 w = 0; w < wheels; w++) {
        float r = radius - (float)w * (thickness + 2.0f * scale);
        if (r <= 2.0f) {
            break;
        }
        dl->AddCircle(center, r, IM_COL32(0, 0, 0, 140), 64, thickness);

        float fill = KafeiForm_WheelFill(w);
        if (fill <= 0.001f) {
            continue;
        }
        // Red while recovering, so the "you cannot run yet" state is unmistakable.
        ImU32 col = winded ? IM_COL32(230, 70, 60, 235) : IM_COL32(120, 230, 110, 235);
        float a0 = -kPi * 0.5f;
        float a1 = a0 + fill * (kPi * 2.0f);
        dl->PathArcTo(center, r, a0, a1, 64);
        dl->PathStroke(col, 0, thickness);
    }
}

} // namespace

// Called every frame from the Kafei tick; registers the window on first use.
extern "C" void KafeiStaminaHud_DrawImGui(void) {
    if (sHudWindow != nullptr) {
        return;
    }
    auto ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr) {
        return;
    }
    auto window = ctx->GetWindow();
    if (window == nullptr) {
        return;
    }
    auto gui = window->GetGui();
    if (gui == nullptr) {
        return;
    }
    sHudWindow = std::make_shared<KafeiStaminaHudWindow>("gKafeiStaminaHudWindow", "Kafei Stamina");
    gui->AddGuiWindow(sHudWindow);
}
