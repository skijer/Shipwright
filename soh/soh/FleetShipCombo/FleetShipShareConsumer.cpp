// Fleet Ship Combo - picture-in-picture CONSUMER (Ship side).
//
// Opens the D3D11 shared texture that 2ship publishes (its rendered game image) and
// draws it in a resizable ImGui window, so MM appears picture-in-picture inside Ship.
// Uses ONLY public libultraship APIs + D3D11 COM, so there are NO submodule changes.
//
// Getting Ship's render device without submodule changes: Window::GetGfxFrameBuffer()
// returns Ship's own game framebuffer SRV (when internal res != 1.0x); srv->GetDevice()
// gives the ID3D11Device that ImGui renders with. We only need it once, then cache it.

#include "FleetShipCombo.h"

#include <libultraship/libultraship.h>
#include <libultraship/bridge.h>
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <memory>
#include <cstdio>

#ifdef _WIN32
#include <d3d11.h>
#include <dxgi.h>

namespace {
ID3D11Device* sDevice = nullptr;
ID3D11DeviceContext* sCtx = nullptr;
ID3D11Texture2D* sSharedTex = nullptr; // opened shared (keyed-mutex) texture from 2ship
IDXGIKeyedMutex* sMutex = nullptr;
ID3D11Texture2D* sPrivateTex = nullptr; // our own copy that ImGui samples
ID3D11ShaderResourceView* sPrivateSrv = nullptr;
unsigned long long sOpenedHandle = 0;
unsigned int sTexW = 0;
unsigned int sTexH = 0;

// Grab Ship's render device once (the same device ImGui draws with). Returns null until
// the game renders to an offscreen fb (we nudge the internal resolution to make it so).
ID3D11Device* GetShipDevice() {
    if (sDevice) {
        return sDevice;
    }
    auto window = Ship::Context::GetRawInstance()->GetWindow();
    if (!window) {
        return nullptr;
    }
    uintptr_t fb = window->GetGfxFrameBuffer();
    if (fb == 0) {
        // Coax the game to render to an fb so we can read the device next frame.
        window->SetResolutionMultiplier(1.01f);
        return nullptr;
    }
    ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(fb);
    srv->GetDevice(&sDevice); // AddRef; cached for the app's lifetime
    if (sDevice) {
        sDevice->GetImmediateContext(&sCtx);
    }
    return sDevice;
}

void ReleaseOpened() {
    if (sPrivateSrv) {
        sPrivateSrv->Release();
        sPrivateSrv = nullptr;
    }
    if (sPrivateTex) {
        sPrivateTex->Release();
        sPrivateTex = nullptr;
    }
    if (sMutex) {
        sMutex->Release();
        sMutex = nullptr;
    }
    if (sSharedTex) {
        sSharedTex->Release();
        sSharedTex = nullptr;
    }
    sOpenedHandle = 0;
    sTexW = sTexH = 0;
}

// Reopen 2ship's shared texture on handle change and (re)create our private copy + SRV.
void EnsureTexture() {
    unsigned long long handle = 0;
    unsigned int w = 0, h = 0, fmt = 0, frame = 0;
    if (!FleetShipCombo_GetSharedTexture(&handle, &w, &h, &fmt, &frame) || handle == 0) {
        return;
    }
    ID3D11Device* dev = GetShipDevice();
    if (!dev) {
        return;
    }
    if (handle == sOpenedHandle && sPrivateSrv) {
        return; // already opened this handle
    }

    ReleaseOpened();

    ID3D11Resource* res = nullptr;
    if (FAILED(dev->OpenSharedResource(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle)),
                                       __uuidof(ID3D11Resource), reinterpret_cast<void**>(&res))) ||
        !res) {
        return;
    }
    res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&sSharedTex));
    res->Release();
    if (!sSharedTex) {
        return;
    }
    sSharedTex->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&sMutex));

    D3D11_TEXTURE2D_DESC sd;
    sSharedTex->GetDesc(&sd);
    D3D11_TEXTURE2D_DESC pd = {};
    pd.Width = sd.Width;
    pd.Height = sd.Height;
    pd.MipLevels = 1;
    pd.ArraySize = 1;
    pd.Format = sd.Format;
    pd.SampleDesc.Count = 1;
    pd.Usage = D3D11_USAGE_DEFAULT;
    pd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (SUCCEEDED(dev->CreateTexture2D(&pd, nullptr, &sPrivateTex)) && sPrivateTex &&
        SUCCEEDED(dev->CreateShaderResourceView(sPrivateTex, nullptr, &sPrivateSrv)) && sPrivateSrv) {
        sOpenedHandle = handle;
        sTexW = sd.Width;
        sTexH = sd.Height;
        SPDLOG_INFO("[FleetShipCombo] Consumer opened 2ship texture {}x{}", sd.Width, sd.Height);
    } else {
        ReleaseOpened();
    }
}

// Copy 2ship's shared texture into our private copy under the keyed mutex (cross-process
// GPU sync). If the producer holds the lock, skip this frame (keep the last copy) instead
// of reading a half-written / stale surface -> no "stuck"/torn image.
void CopyFrameLocked() {
    if (!sMutex || !sSharedTex || !sPrivateTex || !sCtx) {
        return;
    }
    if (sMutex->AcquireSync(0, 8) == S_OK) {
        sCtx->CopyResource(sPrivateTex, sSharedTex);
        sMutex->ReleaseSync(0);
    }
}

// ---------------- 2ship UI-overlay texture (trackers etc., transparent background) ----------------
// Same open/copy pattern as the game texture, but for the SECOND shared texture 2ship publishes
// with only its ImGui windows. Drawn stretched over the full window (both windows overlay the
// same rect, so it maps 1:1) on top of whichever game is active.
ID3D11Texture2D* sUiSharedTex = nullptr;
IDXGIKeyedMutex* sUiMutex = nullptr;
ID3D11Texture2D* sUiPrivateTex = nullptr;
ID3D11ShaderResourceView* sUiPrivateSrv = nullptr;
unsigned long long sUiOpenedHandle = 0;
unsigned int sUiTexW = 0;
unsigned int sUiTexH = 0;

void ReleaseUiOpened() {
    if (sUiPrivateSrv) {
        sUiPrivateSrv->Release();
        sUiPrivateSrv = nullptr;
    }
    if (sUiPrivateTex) {
        sUiPrivateTex->Release();
        sUiPrivateTex = nullptr;
    }
    if (sUiMutex) {
        sUiMutex->Release();
        sUiMutex = nullptr;
    }
    if (sUiSharedTex) {
        sUiSharedTex->Release();
        sUiSharedTex = nullptr;
    }
    sUiOpenedHandle = 0;
    sUiTexW = sUiTexH = 0;
}

void EnsureUiTexture() {
    unsigned long long handle = 0;
    unsigned int w = 0, h = 0, fmt = 0, frame = 0;
    if (!FleetShipCombo_GetUiTexture(&handle, &w, &h, &fmt, &frame) || handle == 0) {
        return;
    }
    ID3D11Device* dev = GetShipDevice();
    if (!dev) {
        return;
    }
    if (handle == sUiOpenedHandle && sUiPrivateSrv) {
        return; // already opened this handle
    }

    ReleaseUiOpened();

    ID3D11Resource* res = nullptr;
    if (FAILED(dev->OpenSharedResource(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle)),
                                       __uuidof(ID3D11Resource), reinterpret_cast<void**>(&res))) ||
        !res) {
        return;
    }
    res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&sUiSharedTex));
    res->Release();
    if (!sUiSharedTex) {
        return;
    }
    sUiSharedTex->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&sUiMutex));

    D3D11_TEXTURE2D_DESC sd;
    sUiSharedTex->GetDesc(&sd);
    D3D11_TEXTURE2D_DESC pd = {};
    pd.Width = sd.Width;
    pd.Height = sd.Height;
    pd.MipLevels = 1;
    pd.ArraySize = 1;
    pd.Format = sd.Format;
    pd.SampleDesc.Count = 1;
    pd.Usage = D3D11_USAGE_DEFAULT;
    pd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (SUCCEEDED(dev->CreateTexture2D(&pd, nullptr, &sUiPrivateTex)) && sUiPrivateTex &&
        SUCCEEDED(dev->CreateShaderResourceView(sUiPrivateTex, nullptr, &sUiPrivateSrv)) && sUiPrivateSrv) {
        sUiOpenedHandle = handle;
        sUiTexW = sd.Width;
        sUiTexH = sd.Height;
        SPDLOG_INFO("[FleetShipCombo] Consumer opened 2ship UI-overlay texture {}x{}", sd.Width, sd.Height);
    } else {
        ReleaseUiOpened();
    }
}

void CopyUiFrameLocked() {
    if (!sUiMutex || !sUiSharedTex || !sUiPrivateTex || !sCtx) {
        return;
    }
    if (sUiMutex->AcquireSync(0, 8) == S_OK) {
        sCtx->CopyResource(sUiPrivateTex, sUiSharedTex);
        sUiMutex->ReleaseSync(0);
    }
}

// Pull + draw 2ship's UI overlay stretched over the whole viewport. Hidden while the overlay
// looks STALE (2ship stopped publishing: mid-load, killed, or overlay disabled on its side) so
// a frozen ghost UI never lingers on screen.
void DrawUiOverlay(ImDrawList* fg, ImGuiViewport* vp) {
    static unsigned int sLastUiFrame = 0;
    static int sUiStall = 0;
    unsigned long long handle = 0;
    unsigned int w = 0, h = 0, fmt = 0, frame = 0;
    if (!FleetShipCombo_GetUiTexture(&handle, &w, &h, &fmt, &frame) || handle == 0) {
        return;
    }
    if (frame == sLastUiFrame) {
        if (sUiStall < 100000) {
            sUiStall++;
        }
    } else {
        sUiStall = 0;
        sLastUiFrame = frame;
    }
    if (sUiStall >= 30) {
        return; // stale: 2ship isn't refreshing its UI right now
    }
    EnsureUiTexture();
    CopyUiFrameLocked();
    if (!sUiPrivateSrv || sUiTexW == 0 || sUiTexH == 0) {
        return;
    }
    ImVec2 p0 = vp->Pos;
    ImVec2 p1 = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);
    fg->AddImage(reinterpret_cast<ImTextureID>(sUiPrivateSrv), p0, p1);
}

class FleetShip2ShipWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {
    }
    void UpdateElement() override {
    }
    void DrawElement() override {
    }

    // Full-window overlay: when MM (2ship) is the ACTIVE game, draw its image covering
    // the whole Ship window (OoT is frozen behind it). When OoT is active, draw nothing
    // so Ship's own game shows. Layout = "active game big, inactive not shown".
    // Draw on the FOREGROUND draw list: SoH renders its own game inside a "Main Game"
    // ImGui window, which sits ABOVE the background draw list — so background-list MM was
    // hidden behind OoT. Foreground is above the game. To keep the menu usable we SKIP
    // the MM overlay while the menu is open (you then see frozen OoT + the menu to switch).
    void Draw() override {
        int active = FleetShipCombo_GetActiveGame();
        // Persist the active game as isPlayerIn2Ship no matter WHO flipped it (our menu button,
        // 2ship's menu button, a cross-game warp), so the next boot resumes in the right game.
        if (active >= 0 && CVarGetInteger("isPlayerIn2Ship", 0) != active) {
            CVarSetInteger("isPlayerIn2Ship", active);
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        static unsigned int sLastTexFrame = 0; // last seen MM shared-texture frame index
        static int sTexStall = 0;              // frames the index has NOT advanced (MM mid-load)
        static int sLastActive = -1;
        static int sPostFlipHold = 0; // hold black for a few frames right after flipping TO MM
        if (active != sLastActive) {
            // Just flipped between games: the newly-active game renders a few frames of its OLD (frozen)
            // scene before its warp load kicks in (and the frame index still advances on those, so
            // stall-detection misses them). Force black over them so we never glimpse the previous state.
            sPostFlipHold = 10;
        }
        sLastActive = active;
        bool postFlipHold = (sPostFlipHold > 0);
        if (sPostFlipHold > 0) {
            sPostFlipHold--;
        }

        auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
        auto menu = gui ? gui->GetMenu() : nullptr;
        bool menuOpen = menu && menu->IsVisible();
        bool guestUiFront = (FleetShipCombo_GetUiFocus() == 1); // 2ship window is on top for config

        // Peek OoT's menu while MM is the ACTIVE game: the "Ship of Harkinian" tab set uiFocus=0 while MM
        // is active, so MM's window parks itself and this Ship window surfaces. Open OoT's SohMenu so the
        // user actually lands in it (symmetric to peeking MM's BenGui while playing OoT). When they close
        // it, hand focus back to MM (uiFocus=1) so play resumes.
        static int sPeekOoTFrames = 0;
        if (active == 1 && !guestUiFront) {
            if (sPeekOoTFrames == 0 && menu && !menu->IsVisible()) {
                menu->Show();
            }
            sPeekOoTFrames++;
            if (sPeekOoTFrames > 3 && menu && !menu->IsVisible()) {
                FleetShipCombo_SetUiFocus(1); // menu closed -> return to MM
                sPeekOoTFrames = 0;
            }
            menuOpen = menu && menu->IsVisible(); // refresh: a Show() above suppresses the MM composite
        } else {
            sPeekOoTFrames = 0;
        }

        if (active == 1 && !menuOpen && !guestUiFront) {
            EnsureTexture();
            CopyFrameLocked(); // pull a fresh frame under the keyed mutex

            // MM mid-load detection: while MM is loading a scene (warp arrival, cold boot) it BLOCKS
            // and stops publishing, so its shared-texture frame index stalls. Showing that stale frame
            // is the "frozen game for ~1.5s" glitch. When the frame hasn't advanced for a few ticks,
            // treat MM as loading and draw BLACK instead of its last frame -> the teleport stays hidden
            // until MM resumes (its fade-in) and the index starts advancing again.
            {
                unsigned long long sh = 0;
                unsigned int sw = 0, shh = 0, sfmt = 0, sframe = 0;
                FleetShipCombo_GetSharedTexture(&sh, &sw, &shh, &sfmt, &sframe);
                if (sframe == sLastTexFrame) {
                    if (sTexStall < 100000) {
                        sTexStall++;
                    }
                } else {
                    sTexStall = 0;
                    sLastTexFrame = sframe;
                }
            }
            bool mmLoading = (sTexStall >= 2);

            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            ImVec2 p0 = vp->Pos;
            ImVec2 p1 = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);

            // Black fill so the frozen OoT frame never peeks through the letterbox bars (and so the
            // whole window is black while MM is mid-load).
            fg->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));

            if (sPrivateSrv && sTexW != 0 && sTexH != 0 && !mmLoading && !postFlipHold) {
                // Fit MM to the window, preserving aspect ratio (letterboxed).
                float winW = vp->Size.x, winH = vp->Size.y;
                float scale = winW / (float)sTexW;
                float scaleY = winH / (float)sTexH;
                if (scaleY < scale) {
                    scale = scaleY;
                }
                float w = (float)sTexW * scale, h = (float)sTexH * scale;
                float x = vp->Pos.x + (winW - w) * 0.5f, y = vp->Pos.y + (winH - h) * 0.5f;
                fg->AddImage(reinterpret_cast<ImTextureID>(sPrivateSrv), ImVec2(x, y), ImVec2(x + w, y + h));

                // 2ship's UI (trackers etc.) on top of its game image. The game framebuffer is
                // pre-ImGui, so without this MM's floating windows would never be visible here.
                if (CVarGetInteger("gFleetCombo.ShowMmUiOverlay", 1)) {
                    DrawUiOverlay(fg, vp);
                }
            }

            // MM (guest) SENDING fade-out (MM->OoT): the active MM ramps the SHARED alpha while Link
            // walks into its door; draw black over its live image so it fades out, then MM flips to OoT
            // at full black (OoT then shows black during its load + fades in).
            int mmSendAlpha = FleetShipCombo_GetSendFadeAlpha();
            if (mmSendAlpha > 0) {
                if (mmSendAlpha > 255) {
                    mmSendAlpha = 255;
                }
                fg->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, mmSendAlpha));
            }
        }

        // Playing OoT: still show 2ship's UI overlay (its trackers) on top, so BOTH games'
        // trackers can be open at once. 2ship keeps its ImGui alive while inactive (empty-DL
        // render) and publishes just its floating windows over a transparent background. The
        // overlay is a non-interactive image (use the "2 Ship 2 Harkinian" tab to click MM's UI);
        // it hides while our menu is open / 2ship's real window is up, and the stall check in
        // DrawUiOverlay hides it if 2ship stops publishing (e.g. its overlay was turned off).
        if (active == 0 && !menuOpen && !guestUiFront && !postFlipHold &&
            CVarGetInteger("gFleetCombo.ShowMmUiOverlay", 1)) {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            DrawUiOverlay(ImGui::GetForegroundDrawList(), vp);
        }

        // OoT (host) SENDING fade: a black overlay whose alpha the OoT warp logic ramps up while Link
        // walks into the door. There's NO scene transition, so OoT never reloads/exits and Link stays
        // controllable; this overlay gives the real fade-out, then the logic flips to MM at full black.
        if (active == 0) {
            int sendAlpha = FleetShipCombo_GetSendFadeAlpha();
            if (postFlipHold) {
                sendAlpha = 255; // just flipped to OoT -> hold black over OoT's first (frozen) frames
            }
            if (sendAlpha > 0) {
                if (sendAlpha > 255) {
                    sendAlpha = 255;
                }
                ImGuiViewport* vp = ImGui::GetMainViewport();
                ImVec2 p0 = vp->Pos;
                ImVec2 p1 = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);
                ImGui::GetForegroundDrawList()->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, sendAlpha));
            }
        }
    }
};

std::shared_ptr<FleetShip2ShipWindow> sWindow;
} // namespace
#endif // _WIN32

void FleetShipCombo_RegisterConsumerWindow(void) {
#ifdef _WIN32
    if (!CVarGetInteger("isFleetShipCombo.Enabled", 0)) {
        return; // only when the combo is on
    }
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }
    static const char* kName = "2ship (MM)";
    if (gui->GetGuiWindow(kName) != nullptr) {
        return;
    }
    sWindow = std::make_shared<FleetShip2ShipWindow>("gOpenWindows.FleetShip2Ship", kName);
    gui->AddGuiWindow(sWindow);
    sWindow->Show();
    SPDLOG_INFO("[FleetShipCombo] Consumer PiP window registered");
#endif
}
