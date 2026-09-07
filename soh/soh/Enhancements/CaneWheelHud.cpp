// =============================================================================
// CaneWheelHud — ImGui overlay for the Dual Cane (Cane of Somaria / Cane of Pacci).
//
// Two pieces:
//   1. The 4-spoke radial wheel, shown while the player HOLDS the cane's button.
//      Spoke UP flips between the two canes; the other three are the active
//      cane's skills. Spokes the player has not unlocked yet are drawn greyed
//      out and cannot be landed on. The whole wheel is tinted with the active
//      cane's colour — red for Somaria, yellow for Pacci.
//   2. A one-line placement hint while a summon is being aimed, so the ghost's
//      blue/red state has words attached the first time you see it.
//
// Implemented as a Ship::GuiWindow so Draw() runs inside the Gui's ImGui frame
// (foreground-drawlist additions made outside it are discarded) — the same
// constraint and structure as expansions/sm64/Sm64CapsHud.cpp. The window
// self-registers on the first call to CaneWheelHud_DrawImGui(), which
// z_parameter.c makes once per frame.
//
// All state lives in C (mods/items/logic/item_cane_of_somaria.c) and is read
// through the Cane_* accessors. Skijer's NEI
// =============================================================================

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/libultraship.h>

extern "C" {
#include "z64.h"
#include "macros.h"
extern PlayState* gPlayState;

// Cane state accessors (mods/items/logic/item_cane_of_somaria.c).
u8 Cane_IsWheelOpen(void);
u8 Cane_GetType(void);
s32 Cane_GetWheelSpoke(void);
u8 Cane_HasSkill(u8 skill);
u8 Cane_GetActiveSkill(void);
u8 Cane_IsAiming(void);
u8 Cane_PreviewValid(void);
}

namespace {

// Mirrors item_cane_of_somaria.h. Kept local so this TU does not have to pull the
// mods headers (and their z_player-side dependencies) in.
constexpr int kSpokeCount = 4;
constexpr int kCaneSomaria = 0;
constexpr int kCanePacci = 1;

// [caneType][slot] — slot 0..2 map to spokes RIGHT, DOWN, LEFT.
// Player-facing names. The code still calls Pacci's first two skills FLIP and
// STONE (CANE_SKILL_PACCI_FLIP / _STONE); these are their in-game names.
const char* kSkillNames[2][3] = {
    { "Statue", "Block", "Platform" },
    { "Cane of Pacci", "Magic Powder", "Ultrahand" },
};

const char* kSkillBlurbs[2][3] = {
    { "Elegy shell at your feet", "Pushable block", "Floating platform" },
    { "Flip an enemy over; hold to lift", "Turn an enemy to stone", "Grab and move an object" },
};

const char* kCaneNames[2] = { "Cane of Somaria", "Cane of Pacci" };

constexpr float kPi = 3.14159265f;

ImU32 CaneAccent(int caneType, int alpha) {
    return (caneType == kCanePacci) ? IM_COL32(255, 215, 70, alpha) : IM_COL32(255, 80, 80, alpha);
}

void DrawTextShadowed(ImDrawList* dl, const ImVec2& pos, ImU32 col, const char* text) {
    dl->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 200), text);
    dl->AddText(pos, col, text);
}

void DrawWheel(ImDrawList* dl, const ImVec2& disp) {
    const float cx = disp.x * 0.5f;
    const float cy = disp.y * 0.5f;
    const float radius = std::min(disp.x, disp.y) * 0.20f;
    const int caneType = (Cane_GetType() == kCanePacci) ? kCanePacci : kCaneSomaria;
    const int selected = Cane_GetWheelSpoke();
    const ImU32 accent = CaneAccent(caneType, 255);

    // Dim backing disc so the spokes read over any scene.
    dl->AddCircleFilled(ImVec2(cx, cy), radius * 1.35f, IM_COL32(0, 0, 0, 115), 64);
    dl->AddCircle(ImVec2(cx, cy), radius * 1.35f, CaneAccent(caneType, 110), 64, 2.0f);

    // Hub: which cane is in hand right now.
    {
        const ImVec2 ts = ImGui::CalcTextSize(kCaneNames[caneType]);
        DrawTextShadowed(dl, ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), accent, kCaneNames[caneType]);
    }

    for (int spoke = 0; spoke < kSpokeCount; spoke++) {
        // Sector 0 = up, clockwise. Screen Y grows downward, hence the -cos.
        const float angle = (float)spoke * (2.0f * kPi / (float)kSpokeCount);
        const float px = cx + std::sin(angle) * radius;
        const float py = cy - std::cos(angle) * radius;
        const bool isSel = (spoke == selected);

        std::string label;
        std::string blurb;
        bool unlocked;

        if (spoke == 0) {
            // UP flips to the OTHER cane — available only once it is owned.
            const int other = caneType ^ 1;
            unlocked = Cane_HasSkill((u8)(other * 3 + 0)) || Cane_HasSkill((u8)(other * 3 + 1)) ||
                       Cane_HasSkill((u8)(other * 3 + 2));
            label = std::string("Switch: ") + kCaneNames[other];
            blurb = unlocked ? "" : "not found yet";
        } else {
            const int slot = spoke - 1;
            unlocked = Cane_HasSkill((u8)(caneType * 3 + slot)) != 0;
            label = kSkillNames[caneType][slot];
            blurb = unlocked ? kSkillBlurbs[caneType][slot] : "locked";
        }

        ImU32 dotCol;
        ImU32 textCol;
        if (!unlocked) {
            dotCol = IM_COL32(110, 110, 120, 130);
            textCol = IM_COL32(140, 140, 150, 150);
        } else if (isSel) {
            dotCol = accent;
            textCol = IM_COL32(255, 250, 220, 255);
        } else {
            dotCol = IM_COL32(200, 200, 210, 180);
            textCol = IM_COL32(220, 220, 230, 215);
        }

        dl->AddCircleFilled(ImVec2(px, py), isSel ? 9.0f : 5.0f, dotCol, 20);
        if (isSel && unlocked) {
            dl->AddCircle(ImVec2(px, py), 13.0f, accent, 20, 2.0f);
        }

        // Push labels outward from the hub so they never overlap their spoke.
        const ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        const float lx = cx + std::sin(angle) * (radius + 30.0f) - ts.x * 0.5f;
        const float ly = cy - std::cos(angle) * (radius + 30.0f) - ts.y * 0.5f;
        DrawTextShadowed(dl, ImVec2(lx, ly), textCol, label.c_str());

        if (!blurb.empty()) {
            const ImVec2 bs = ImGui::CalcTextSize(blurb.c_str());
            DrawTextShadowed(dl, ImVec2(cx + std::sin(angle) * (radius + 30.0f) - bs.x * 0.5f, ly + ts.y + 2.0f),
                             IM_COL32(190, 190, 200, 170), blurb.c_str());
        }
    }

    const char* hint = "Tilt to choose  -  release L to confirm";
    const ImVec2 hs = ImGui::CalcTextSize(hint);
    DrawTextShadowed(dl, ImVec2(cx - hs.x * 0.5f, cy + radius * 1.35f + 14.0f), IM_COL32(235, 235, 245, 230), hint);
}

void DrawAimHint(ImDrawList* dl, const ImVec2& disp) {
    const int caneType = (Cane_GetType() == kCanePacci) ? kCanePacci : kCaneSomaria;
    const int skill = (int)Cane_GetActiveSkill();
    const int slot = skill % 3;
    const bool valid = Cane_PreviewValid() != 0;

    std::string msg = std::string(kSkillNames[caneType][slot]) + (valid ? ": press to place" : ": blocked here");
    const ImU32 col = valid ? IM_COL32(120, 190, 255, 235) : IM_COL32(255, 110, 110, 235);

    const ImVec2 ts = ImGui::CalcTextSize(msg.c_str());
    DrawTextShadowed(dl, ImVec2(disp.x * 0.5f - ts.x * 0.5f, disp.y * 0.13f), col, msg.c_str());
}

class CaneWheelHudWindow final : public Ship::GuiWindow {
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

void CaneWheelHudWindow::Draw() {
    const bool wheel = Cane_IsWheelOpen() != 0;
    const bool aiming = Cane_IsAiming() != 0;
    if (!wheel && !aiming) {
        return;
    }
    if (gPlayState == nullptr || gPlayState->pauseCtx.state != 0) {
        return;
    }
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    if (gui == nullptr || gui->GetMenuOrMenubarVisible()) {
        return;
    }
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return;
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList(viewport);
    if (dl == nullptr) {
        return;
    }
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    if (disp.x < 1.0f || disp.y < 1.0f) {
        return;
    }

    if (wheel) {
        DrawWheel(dl, disp);
    } else {
        DrawAimHint(dl, disp);
    }
}

std::shared_ptr<CaneWheelHudWindow> sHudWindow = nullptr;

} // namespace

// Called once per frame from z_parameter.c. Registering the GuiWindow lazily here
// (rather than at init time) guarantees the Gui exists; after the first call the
// window draws itself at the right point in the ImGui frame, so this is a no-op.
extern "C" void CaneWheelHud_DrawImGui(void) {
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
    sHudWindow = std::make_shared<CaneWheelHudWindow>("gCaneWheelHud", "Dual Cane HUD");
    gui->AddGuiWindow(sHudWindow);
}
