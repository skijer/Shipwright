#include "Microphone.h"

#include <atomic>
#include <memory>
#include <string>

#include <imgui.h>
#include <libultraship/libultraship.h>
#include <ship/window/gui/IconsFontAwesome4.h>
#include <spdlog/spdlog.h>
#include <SDL2/SDL.h>

#include "soh/ModApi/ModPermissions/ModPermissions.h"
#include "soh/cvar_prefixes.h"

namespace {

// Drawn whatever its visibility CVar says: a mod must not be able to record without the player seeing it.
class MicrophoneIndicatorWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void Draw() override {
        DrawElement();
    }
    void InitElement() override {
    }
    void DrawElement() override;
    void UpdateElement() override {
    }
};

std::atomic<SDL_AudioDeviceID> sDevice{ 0 };
const ModIdentity* sOwner = nullptr;
std::string sOwnerName;
std::shared_ptr<MicrophoneIndicatorWindow> sIndicator;

void MicrophoneIndicatorWindow::DrawElement() {
    if (sDevice.load(std::memory_order_acquire) == 0) {
        return;
    }
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float fontSize = ImGui::GetFontSize();
    const float radius = fontSize;
    const ImVec2 center(viewport->Pos.x + viewport->Size.x - radius * 2.0f, viewport->Pos.y + radius * 2.0f);
    drawList->AddCircleFilled(center, radius, IM_COL32(200, 30, 30, 230));

    const ImVec2 iconSize = ImGui::CalcTextSize(ICON_FA_MICROPHONE);
    drawList->AddText(ImVec2(center.x - iconSize.x * 0.5f, center.y - iconSize.y * 0.5f), IM_COL32_WHITE,
                      ICON_FA_MICROPHONE);
    const ImVec2 labelSize = ImGui::CalcTextSize(sOwnerName.c_str());
    drawList->AddText(ImVec2(center.x - radius * 1.5f - labelSize.x, center.y - labelSize.y * 0.5f),
                      IM_COL32(255, 255, 255, 220), sOwnerName.c_str());
}

SOHMicrophoneStatus OpenFor(const ModIdentity* mod, uint32_t sampleRate) {
    if (mod == nullptr) {
        SPDLOG_ERROR("[Microphone] Only a mod can open the microphone");
        return SOH_MICROPHONE_UNAVAILABLE;
    }
    if (sDevice.load() != 0) {
        return sOwner == mod ? SOH_MICROPHONE_OPEN : SOH_MICROPHONE_UNAVAILABLE;
    }
    if (!ModPermissions_HasDeclared(*mod, ModPermission::Microphone)) {
        return SOH_MICROPHONE_DENIED;
    }
    const PermissionAnswer answer = ModPermissions_AskOnce(
        *mod, ModPermission::Microphone,
        "wants to use your microphone.\n\nWhile it listens, a microphone icon shows in the top right corner.");
    if (answer != PermissionAnswer::Granted) {
        return answer == PermissionAnswer::Pending ? SOH_MICROPHONE_PENDING : SOH_MICROPHONE_DENIED;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SPDLOG_ERROR("[Microphone] SDL audio is unavailable: {}", SDL_GetError());
        return SOH_MICROPHONE_UNAVAILABLE;
    }
    SDL_AudioSpec wanted = {};
    wanted.freq = static_cast<int>(sampleRate);
    wanted.format = AUDIO_F32SYS;
    wanted.channels = 1;
    wanted.samples = 512;
    SDL_AudioSpec obtained = {};
    const SDL_AudioDeviceID device = SDL_OpenAudioDevice(nullptr, 1, &wanted, &obtained, 0);
    if (device == 0) {
        SPDLOG_ERROR("[Microphone] Could not open the default capture device: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return SOH_MICROPHONE_UNAVAILABLE;
    }
    sOwner = mod;
    sOwnerName = mod->name;
    sDevice.store(device, std::memory_order_release);
    SDL_PauseAudioDevice(device, 0);
    SPDLOG_INFO("[Microphone] '{}' started recording", mod->name);
    return SOH_MICROPHONE_OPEN;
}

void CloseFor(const ModIdentity* mod) {
    const SDL_AudioDeviceID device = sDevice.load();
    if (device == 0 || mod != sOwner) {
        return;
    }
    sDevice.store(0, std::memory_order_release);
    SDL_CloseAudioDevice(device);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    sOwner = nullptr;
    SPDLOG_INFO("[Microphone] '{}' stopped recording", mod->name);
}

} // namespace

void Microphone_Init() {
    auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
    sIndicator = std::make_shared<MicrophoneIndicatorWindow>(CVAR_WINDOW("MicrophoneIndicator"), "Microphone");
    gui->AddGuiWindow(sIndicator);
}

extern "C" SOHMicrophoneStatus Microphone_Open(uint32_t sampleRate) {
    return OpenFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), sampleRate);
}

extern "C" uint32_t Microphone_Read(float* samples, uint32_t capacity) {
    const SDL_AudioDeviceID device = sDevice.load(std::memory_order_acquire);
    if (device == 0 || samples == nullptr || ModPermissions_FindCaller(MOD_CALLER_ADDRESS()) != sOwner) {
        return 0;
    }
    return SDL_DequeueAudio(device, samples, capacity * sizeof(float)) / sizeof(float);
}

extern "C" void Microphone_Close(void) {
    CloseFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()));
}
