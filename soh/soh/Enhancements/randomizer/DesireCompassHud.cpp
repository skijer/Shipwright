// =============================================================================
// DesireCompassHud (OoT side) — ImGui overlay for the Quartz of Motion sensor.
//
//   1. The category list, drawn while the kaleido's Quartz modal is open. The
//      kaleido owns the INPUT (quartz_kaleido.cpp); this owns the PIXELS.
//   2. The sensor readout while the 5-minute session runs: category + time, a
//      "something here" badge that flashes when you walk into a room holding a
//      target, and a proximity meter that fills as you close in.
//
// Nothing is projected into the world — the Quartz is a sensor, not a compass.
//
// Implemented as a Ship::GuiWindow so Draw() runs inside the Gui's ImGui frame
// (same constraint as pikachu_hud.cpp / HarpoonGamemodeHud.cpp).
// =============================================================================

#include "DesireCompass.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include <libultraship/libultraship.h>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
extern PlayState* gPlayState;

// Kaleido modal state (quartz_kaleido.cpp).
u8 Quartz_IsListOpen(void);
s32 Quartz_GetListIndex(void);
}

namespace {

constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 200);

void TextShadowed(ImDrawList* dl, ImVec2 p, ImU32 col, const char* s) {
    dl->AddText(ImVec2(p.x + 1.0f, p.y + 1.0f), kShadow, s);
    dl->AddText(p, col, s);
}

// ---- 1. Category list (kaleido modal) ---------------------------------------

void DrawCategoryList(ImDrawList* dl, const ImVec2& disp) {
    const int selected = Quartz_GetListIndex();
    const float rowH = std::max(18.0f, disp.y * 0.042f);
    const float panelW = std::max(260.0f, disp.x * 0.26f);
    const float panelH = rowH * (float)DCOMPASS_CAT_MAX + 66.0f;
    const float x0 = disp.x * 0.5f - panelW * 0.5f;
    const float y0 = disp.y * 0.5f - panelH * 0.5f;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + panelW, y0 + panelH), IM_COL32(10, 10, 18, 225), 6.0f);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + panelW, y0 + panelH), IM_COL32(255, 215, 90, 140), 6.0f, 0, 2.0f);

    TextShadowed(dl, ImVec2(x0 + 14.0f, y0 + 10.0f), IM_COL32(255, 240, 170, 255), "Quartz of Motion");

    for (int i = 0; i < DCOMPASS_CAT_MAX; i++) {
        const float ry = y0 + 34.0f + rowH * (float)i;
        const bool isSel = (i == selected);
        if (isSel) {
            dl->AddRectFilled(ImVec2(x0 + 6.0f, ry - 2.0f), ImVec2(x0 + panelW - 6.0f, ry + rowH - 4.0f),
                              IM_COL32(255, 215, 90, 45), 3.0f);
        }

        const int remaining = Rando_DesireCompass_CountRemaining((DesireCompassCategory)i, DCOMPASS_SUBCAT_ANY);
        char row[96];
        snprintf(row, sizeof(row), "%s %s", isSel ? ">" : " ",
                 Rando_DesireCompass_CategoryName((DesireCompassCategory)i));
        // Categories with nothing left are dimmed — still selectable, but the
        // player can see the hearts would be wasted.
        const ImU32 col = (remaining > 0) ? (isSel ? IM_COL32(255, 240, 170, 255) : IM_COL32(220, 220, 230, 215))
                                          : IM_COL32(130, 130, 140, 190);
        TextShadowed(dl, ImVec2(x0 + 14.0f, ry), col, row);

        char cnt[24];
        snprintf(cnt, sizeof(cnt), "%d", remaining);
        const ImVec2 cs = ImGui::CalcTextSize(cnt);
        TextShadowed(dl, ImVec2(x0 + panelW - 16.0f - cs.x, ry), col, cnt);
    }

    const char* hint = "A: sense (costs 3 hearts)   B: cancel";
    const ImVec2 hs = ImGui::CalcTextSize(hint);
    TextShadowed(dl, ImVec2(disp.x * 0.5f - hs.x * 0.5f, y0 + panelH - 22.0f), IM_COL32(200, 200, 215, 225), hint);
}

// ---- 2. Sensor readout ------------------------------------------------------

void DrawSensor(ImDrawList* dl, ImGuiViewport* vp) {
    const DesireCompassCategory cat = Rando_DesireCompass_GetActiveCategory();
    const int secs = Rando_DesireCompass_GetRemainingSeconds();
    const bool here = Rando_DesireCompass_RoomHasTarget() != 0;
    const int alert = Rando_DesireCompass_RoomAlertFrames();
    const float prox = Rando_DesireCompass_GetProximity();

    const float panelW = std::max(200.0f, vp->Size.x * 0.19f);
    const float panelH = 62.0f;
    const float x0 = vp->Pos.x + vp->Size.x - panelW - 18.0f;
    const float y0 = vp->Pos.y + vp->Size.y * 0.13f;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + panelW, y0 + panelH), IM_COL32(10, 10, 18, 170), 5.0f);

    char head[96];
    snprintf(head, sizeof(head), "%s  %d:%02d", Rando_DesireCompass_CategoryName(cat), secs / 60, secs % 60);
    TextShadowed(dl, ImVec2(x0 + 10.0f, y0 + 7.0f), IM_COL32(235, 235, 245, 230), head);

    // Signal 1: "something in this room". Pulses while the entry alert runs.
    const char* status = here ? "SOMETHING HERE" : "nothing here";
    ImU32 statusCol = IM_COL32(140, 140, 150, 200);
    if (here) {
        float pulse = 1.0f;
        if (alert > 0) {
            // 4 blinks over the alert window.
            pulse = 0.55f + 0.45f * std::sin((float)alert * 0.35f);
        }
        statusCol = IM_COL32(255, (int)(200 * pulse + 40), 90, 240);
    }
    TextShadowed(dl, ImVec2(x0 + 10.0f, y0 + 25.0f), statusCol, status);

    // Signal 2: proximity meter (empty when nothing is loaded).
    const float barX = x0 + 10.0f;
    const float barY = y0 + 46.0f;
    const float barW = panelW - 20.0f;
    const float barH = 7.0f;
    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(40, 40, 50, 200), 3.0f);
    if (here && prox > 0.0f) {
        // Cold (blue) far -> hot (red) close.
        const int r = (int)(60 + 195 * prox);
        const int g = (int)(180 - 120 * prox);
        const int b = (int)(230 - 190 * prox);
        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * prox, barY + barH), IM_COL32(r, g, b, 235), 3.0f);
    }
}

class DesireCompassHudWindow final : public Ship::GuiWindow {
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

void DesireCompassHudWindow::Draw() {
    if (gPlayState == nullptr) {
        return;
    }
    const bool listOpen = Quartz_IsListOpen() != 0;
    const bool sensorOn = Rando_DesireCompass_IsActive() != 0;
    if (!listOpen && !sensorOn) {
        return;
    }
    // The list draws OVER the kaleido, so it must survive the paused check that
    // the sensor readout is subject to.
    if (!listOpen && gPlayState->pauseCtx.state != 0) {
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
    if (gui == nullptr || gui->GetMenuOrMenubarVisible()) {
        return;
    }
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp == nullptr || vp->Size.x < 1.0f || vp->Size.y < 1.0f) {
        return;
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList(vp);
    if (dl == nullptr) {
        return;
    }

    if (listOpen) {
        DrawCategoryList(dl, vp->Size);
    } else {
        DrawSensor(dl, vp);
    }
}

std::shared_ptr<DesireCompassHudWindow> sHudWindow = nullptr;

void EnsureRegistered() {
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
    sHudWindow = std::make_shared<DesireCompassHudWindow>("gDesireCompassHud", "Quartz of Motion HUD");
    gui->AddGuiWindow(sHudWindow);
    sHudWindow->Show();
}

void RegisterDesireCompassHud() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([]() { EnsureRegistered(); });
}

static RegisterShipInitFunc dcHudInitFunc(RegisterDesireCompassHud, {});

} // namespace
