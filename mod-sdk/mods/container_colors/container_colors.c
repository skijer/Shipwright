#include <stdbool.h>
#include <stdint.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModMenu/ModUI.h"
#include "soh/Enhancements/randomizer/item_category_adj.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "objects/object_st/object_st.h"
#include "overlays/actors/ovl_En_Cow/z_en_cow.h"
#include "overlays/actors/ovl_En_Sw/z_en_sw.h"

#define STYLE_SERVICE "rando.container_style"
#define PREVIEW_SERVICE "container_colors.preview_style"
#define COLORS_SIDEBAR "Container Colors"
#define FULL_TINT 255
#define COW_LIMB_HEAD 2
#define COW_LIMB_NOSE_RING 5
#define SKULLTULA_LIMB_THORAX 1
#define SKULLTULA_LIMB_ABDOMEN 4
#define SKULLTULA_LIMB_HEAD 5

typedef uint8_t (*ContainerStyleFunc)(Actor* actor);

typedef struct {
    const char* label;
    const char* cvar;
    uint32_t defaultColor;
} StyleTint;

typedef struct {
    s32 limbIndex;
    Gfx* goldDList;
} GoldLimb;

static const StyleTint sTints[] = {
    [CONTAINER_STYLE_LESSER] = { "Lesser Item", "gMods.ContainerColors.Lesser", 0xFA7828 },
    [CONTAINER_STYLE_MAJOR] = { "Major Item", "gMods.ContainerColors.Major", 0xFAB928 },
    [CONTAINER_STYLE_HEALTH] = { "Heart", "gMods.ContainerColors.Health", 0xFA283C },
    [CONTAINER_STYLE_SMALL_KEY] = { "Small Key", "gMods.ContainerColors.SmallKey", 0xBEBEC8 },
    [CONTAINER_STYLE_BOSS_KEY] = { "Boss Key", "gMods.ContainerColors.BossKey", 0x00C8FF },
    [CONTAINER_STYLE_TOKEN] = { "Skulltula Token", "gMods.ContainerColors.Token", 0xFFFABE },
};

static const GoldLimb sGoldLimbs[] = {
    { 23, (Gfx*)object_st_DL_004788 }, { 8, (Gfx*)object_st_DL_0046F0 },  { 14, (Gfx*)object_st_DL_004658 },
    { 11, (Gfx*)object_st_DL_0045C0 }, { 26, (Gfx*)object_st_DL_004820 }, { 20, (Gfx*)object_st_DL_0048B8 },
    { 17, (Gfx*)object_st_DL_004950 }, { 29, (Gfx*)object_st_DL_0049E8 }, { 5, (Gfx*)object_st_DL_003FB0 },
    { 4, (Gfx*)object_st_DL_0043D8 },
};

static const char* const sRequiredHooks[] = { "OnActorDraw", "OnActorResolveGrayscale" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static ContainerStyleFunc sGetContainerStyle;
static ContainerStyleFunc sGetPreviewStyle;
static bool sAreServicesResolved;
static Color_RGB8 sLimbTint;
// Vanilla EnCow focus point, in head limb space.
static Vec3f sCowFocusOffset = { 0.0f, -1300.0f, 1100.0f };
// Vanilla EnSw anchors, in thorax limb space, that its update reads back.
static Vec3f sSkulltulaAnchors[] = {
    { 1400.0f, -2600.0f, -800.0f }, { 1400.0f, -1600.0f, 0.0f }, { -1400.0f, -2600.0f, -800.0f },
    { -1400.0f, -1600.0f, 0.0f },   { 0.0f, 0.0f, -600.0f },
};
static Vec3f sSkulltulaFocusOffset = { 0.0f, 0.0f, 0.0f };

static bool IsTintedStyle(uint8_t style) {
    return style < ARRAY_COUNT(sTints) && sTints[style].cvar != NULL;
}

static Color_RGB8 GetStyleColor(uint8_t style) {
    uint32_t packed = (uint32_t)CVarGetInteger(sTints[style].cvar, (int32_t)sTints[style].defaultColor);
    Color_RGB8 color = { (u8)(packed >> 16), (u8)(packed >> 8), (u8)packed };
    return color;
}

static void SetStyleColor(uint8_t style, const float* rgb) {
    uint32_t red = (uint32_t)(rgb[0] * 255.0f + 0.5f);
    uint32_t green = (uint32_t)(rgb[1] * 255.0f + 0.5f);
    uint32_t blue = (uint32_t)(rgb[2] * 255.0f + 0.5f);
    CVarSetInteger(sTints[style].cvar, (int32_t)((red << 16) | (green << 8) | blue));
    CVarSave();
}

// Looked up on the first draw, once every mod has published its services.
static void ResolveStyleServices(void) {
    sGetContainerStyle = (ContainerStyleFunc)sApi->FindService(STYLE_SERVICE);
    sGetPreviewStyle = (ContainerStyleFunc)sApi->FindService(PREVIEW_SERVICE);
    sAreServicesResolved = true;
}

static uint8_t GetActorStyle(Actor* actor) {
    if (!sAreServicesResolved) {
        ResolveStyleServices();
    }
    uint8_t preview = sGetPreviewStyle != NULL ? sGetPreviewStyle(actor) : CONTAINER_STYLE_NONE;
    if (preview != CONTAINER_STYLE_NONE) {
        return preview;
    }
    return sGetContainerStyle != NULL ? sGetContainerStyle(actor) : CONTAINER_STYLE_NONE;
}

static bool ResolveTint(Actor* actor, Color_RGB8* tint) {
    uint8_t style = GetActorStyle(actor);

    if (!IsTintedStyle(style)) {
        return false;
    }
    *tint = GetStyleColor(style);
    return true;
}

static void BeginOpaTint(PlayState* play, Color_RGB8 tint) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, tint.r, tint.g, tint.b, FULL_TINT);
    gSPGrayscale(POLY_OPA_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EndOpaTint(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 ShowCowNoseRing(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnCow* cow = (EnCow*)thisx;

    if (limbIndex == COW_LIMB_HEAD) {
        rot->y += cow->someRot.y;
        rot->x += cow->someRot.x;
    }
    if (limbIndex == COW_LIMB_NOSE_RING) {
        BeginOpaTint(play, sLimbTint);
    }
    return false;
}

static void FinishCowLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnCow* cow = (EnCow*)thisx;

    if (limbIndex == COW_LIMB_HEAD) {
        Matrix_MultVec3f(&sCowFocusOffset, &cow->actor.focus.pos);
    }
    if (limbIndex == COW_LIMB_NOSE_RING) {
        EndOpaTint(play);
    }
}

// Vanilla hides the nose ring; a cow holding an item wears it in the item's color.
static void DrawStyledCow(Actor* actor, PlayState* play, bool* drawVanilla) {
    EnCow* cow = (EnCow*)actor;

    if (!*drawVanilla || actor->params != 0 || !ResolveTint(actor, &sLimbTint)) {
        return;
    }
    *drawVanilla = false;
    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &cow->skelAnime, ShowCowNoseRing, FinishCowLimb, cow);
}

static bool IsGoldSkulltula(const Actor* actor) {
    return ((actor->params & 0xE000) >> 0xD) != 0;
}

static bool IsSkulltulaBody(s32 limbIndex) {
    return limbIndex == SKULLTULA_LIMB_ABDOMEN || limbIndex == SKULLTULA_LIMB_HEAD;
}

static void SwapGoldLimb(s32 limbIndex, Gfx** dList) {
    for (s32 i = 0; i < ARRAY_COUNT(sGoldLimbs); i++) {
        if (sGoldLimbs[i].limbIndex == limbIndex) {
            *dList = sGoldLimbs[i].goldDList;
            return;
        }
    }
}

// Mirrors the gold path of EnSw_OverrideLimbDraw, which also feeds the collider and the anchors its update reads.
static s32 DressGoldSkulltula(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnSw* skulltula = (EnSw*)thisx;

    SwapGoldLimb(limbIndex, dList);
    if (limbIndex == SKULLTULA_LIMB_THORAX) {
        Matrix_MultVec3f(&sSkulltulaAnchors[0], &skulltula->unk_454);
        Matrix_MultVec3f(&sSkulltulaAnchors[1], &skulltula->unk_460);
        Matrix_MultVec3f(&sSkulltulaAnchors[2], &skulltula->subCamId);
        Matrix_MultVec3f(&sSkulltulaAnchors[3], &skulltula->unk_478);
        Matrix_MultVec3f(&sSkulltulaAnchors[4], &skulltula->unk_484);
    }
    if (limbIndex == SKULLTULA_LIMB_HEAD) {
        Matrix_MultVec3f(&sSkulltulaFocusOffset, &skulltula->actor.focus.pos);
    }
    if (limbIndex == SKULLTULA_LIMB_ABDOMEN) {
        OPEN_DISPS(play->state.gfxCtx);
        gDPSetEnvColor(POLY_OPA_DISP++, skulltula->unk_1F4.r, skulltula->unk_1F4.g, skulltula->unk_1F4.b, 0);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    Collider_UpdateSpheres(limbIndex, &skulltula->collider);
    if (IsSkulltulaBody(limbIndex)) {
        BeginOpaTint(play, sLimbTint);
    }
    return false;
}

static void FinishSkulltulaLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    if (IsSkulltulaBody(limbIndex)) {
        EndOpaTint(play);
    }
}

// Mirrors the gold path of EnSw_Draw.
static void DrawStyledSkulltula(Actor* actor, PlayState* play, bool* drawVanilla) {
    EnSw* skulltula = (EnSw*)actor;

    if (!*drawVanilla || !IsGoldSkulltula(actor) || !ResolveTint(actor, &sLimbTint)) {
        return;
    }
    *drawVanilla = false;
    Matrix_RotateX(DEGF_TO_RADF(-80), MTXMODE_APPLY);
    if (actor->colChkInfo.health != 0) {
        Matrix_Translate(0.0f, 0.0f, 200.0f, MTXMODE_APPLY);
    }
    func_8002EBCC(actor, play, 0);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &skulltula->skelAnime, DressGoldSkulltula, FinishSkulltulaLimb, skulltula);
}

// Another mod's tint (a time stop) wins over the container color.
static void TintBeehive(Actor* actor, PlayState* play, Color_RGBA8* grayscale) {
    Color_RGB8 tint;

    if (grayscale->a != 0 || !ResolveTint(actor, &tint)) {
        return;
    }
    grayscale->r = tint.r;
    grayscale->g = tint.g;
    grayscale->b = tint.b;
    grayscale->a = FULL_TINT;
}

static void ResetColors(void) {
    for (uint8_t style = 0; style < ARRAY_COUNT(sTints); style++) {
        if (IsTintedStyle(style)) {
            CVarClear(sTints[style].cvar);
        }
    }
    CVarSave();
}

static void DrawColorMenu(void) {
    ModUI_Text("Shown once Containers Match Contents is on. Junk keeps the vanilla look.");
    for (uint8_t style = 0; style < ARRAY_COUNT(sTints); style++) {
        if (!IsTintedStyle(style)) {
            continue;
        }
        Color_RGB8 color = GetStyleColor(style);
        float rgb[3] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f };

        if (ModUI_ColorEdit3(sTints[style].label, rgb, 0)) {
            SetStyleColor(style, rgb);
        }
    }
    if (ModUI_Button("Reset Colors", 0.0f, 0.0f)) {
        ResetColors();
    }
}

static void RegisterMenu(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterMenuSidebar)) {
        return;
    }
    sApi->RegisterMenuSidebar("Mods", COLORS_SIDEBAR, "gMods.Menu.Page", 1);

    SOHModMenuWidget colors = { sizeof(SOHModMenuWidget) };
    colors.section = "Mods";
    colors.sidebar = COLORS_SIDEBAR;
    colors.type = SOH_MOD_MENU_CUSTOM;
    colors.label = COLORS_SIDEBAR;
    colors.onChange = DrawColorMenu;
    sApi->RegisterMenuWidget(&colors);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    RegisterMenu();
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_EN_COW, DrawStyledCow);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_EN_SW, DrawStyledSkulltula);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorResolveGrayscale, ACTOR_OBJ_COMB, TintBeehive);
}
