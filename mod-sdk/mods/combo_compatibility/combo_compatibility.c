
#include <stdbool.h>
#include <string.h>

#include "z64items.h"
#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"

#define MM_ICON(name) "__OTR__icon_item_static_yar/gItemIcon" name "Tex"
#define MM_NAME(name) "__OTR__item_name_static/gItemName" name "ENGTex"
#define MM_MODEL(object, displayList) "__OTR__objects/object_" object "/" displayList
#define FROG_ICON(colour) "__OTR__textures/nei_combo_oot/" colour "_frog"
#define FROG_NAME(colour) "__OTR__textures/nei_combo_oot/" colour "_frog_name"
#define TEXTS(label, hint) \
    label, "%r" label "&%wBelongs to Majora's Mask.", "You got %r" label "%w!&Belongs to Majora's Mask. " hint

#define FROG_LIMB_LOWER_BODY 1
#define FROG_LIMB_RIGHT_EYE 7
#define FROG_LIMB_LEFT_EYE 8
#define FROG_LIMB_MAX 24
#define FROG_SCALE 0.03f
#define FROG_HEIGHT_OFFSET -20.0f
#define FROG_BODY_SETBACK 500.0f
#define TEAR_SCROLL_SIZE 32

// Bit 0 of a pointer in a graphics command marks it segmented, so these paths must sit on even addresses.
static const ALIGN_ASSET(2) char sDeedDL[] = MM_MODEL("gi_reserve01", "gGiTitleDeedEmptyDL");
static const ALIGN_ASSET(2) char sLandDeedColorDL[] = MM_MODEL("gi_reserve01", "gGiTitleDeedLandColorDL");
static const ALIGN_ASSET(2) char sSwampDeedColorDL[] = MM_MODEL("gi_reserve01", "gGiTitleDeedSwampColorDL");
static const ALIGN_ASSET(2) char sMountainDeedColorDL[] = MM_MODEL("gi_reserve01", "gGiTitleDeedMountainColorDL");
static const ALIGN_ASSET(2) char sOceanDeedColorDL[] = MM_MODEL("gi_reserve01", "gGiTitleDeedOceanColorDL");
static const ALIGN_ASSET(2) char sMoonsTearDL[] = MM_MODEL("gi_reserve00", "gGiMoonsTearItemDL");
static const ALIGN_ASSET(2) char sMoonsTearGlowDL[] = MM_MODEL("gi_reserve00", "gGiMoonsTearGlowDL");
static const ALIGN_ASSET(2) char sRoomKeyDL[] = MM_MODEL("gi_reserve_b_00", "gGiRoomKeyEmptyDL");
static const ALIGN_ASSET(2) char sRoomKeyTagDL[] = MM_MODEL("gi_reserve_b_00", "gGiRoomKeyDL");
static const ALIGN_ASSET(2) char sKafeiLetterDL[] = MM_MODEL("gi_reserve_c_00", "gGiLetterToKafeiEnvelopeLetterDL");
static const ALIGN_ASSET(2) char sKafeiLetterWritingDL[] = MM_MODEL("gi_reserve_c_00",
                                                                    "gGiLetterToKafeiInscriptionsDL");
static const ALIGN_ASSET(2) char sMamaLetterDL[] = MM_MODEL("gi_reserve_b_01", "gGiLetterToMamaEnvelopeLetterDL");
static const ALIGN_ASSET(2) char sMamaLetterWritingDL[] = MM_MODEL("gi_reserve_b_01", "gGiLetterToMamaInscriptionsDL");
static const ALIGN_ASSET(2) char sFrogSkel[] = MM_MODEL("fr", "gFrogSkel");
static const ALIGN_ASSET(2) char sFrogIdleAnim[] = MM_MODEL("fr", "gFrogIdleAnim");
static const ALIGN_ASSET(2) char sFrogIrisTex[] = MM_MODEL("fr", "gFrogIrisOpenTex");

static const ALIGN_ASSET(2) char sLandDeedIcon[] = MM_ICON("LandDeed");
static const ALIGN_ASSET(2) char sLandDeedName[] = MM_NAME("LandTitleDeed");
static const ALIGN_ASSET(2) char sSwampDeedIcon[] = MM_ICON("SwampDeed");
static const ALIGN_ASSET(2) char sSwampDeedName[] = MM_NAME("SwampTitleDeed");
static const ALIGN_ASSET(2) char sMountainDeedIcon[] = MM_ICON("MountainDeed");
static const ALIGN_ASSET(2) char sMountainDeedName[] = MM_NAME("MountainTitleDeed");
static const ALIGN_ASSET(2) char sOceanDeedIcon[] = MM_ICON("OceanDeed");
static const ALIGN_ASSET(2) char sOceanDeedName[] = MM_NAME("OceanTitleDeed");
static const ALIGN_ASSET(2) char sBlueFrogIcon[] = FROG_ICON("blue");
static const ALIGN_ASSET(2) char sBlueFrogName[] = FROG_NAME("blue");
static const ALIGN_ASSET(2) char sCyanFrogIcon[] = FROG_ICON("cyan");
static const ALIGN_ASSET(2) char sCyanFrogName[] = FROG_NAME("cyan");
static const ALIGN_ASSET(2) char sPinkFrogIcon[] = FROG_ICON("pink");
static const ALIGN_ASSET(2) char sPinkFrogName[] = FROG_NAME("pink");
static const ALIGN_ASSET(2) char sWhiteFrogIcon[] = FROG_ICON("white");
static const ALIGN_ASSET(2) char sWhiteFrogName[] = FROG_NAME("white");
static const ALIGN_ASSET(2) char sMoonsTearIcon[] = MM_ICON("MoonsTear");
static const ALIGN_ASSET(2) char sMoonsTearName[] = MM_NAME("MoonsTear");
static const ALIGN_ASSET(2) char sRoomKeyIcon[] = MM_ICON("RoomKey");
static const ALIGN_ASSET(2) char sRoomKeyName[] = MM_NAME("RoomKey");
static const ALIGN_ASSET(2) char sKafeiLetterIcon[] = MM_ICON("LetterToKafei");
static const ALIGN_ASSET(2) char sKafeiLetterName[] = MM_NAME("LetterToKafei");
static const ALIGN_ASSET(2) char sMamaLetterIcon[] = MM_ICON("LetterToMama");
static const ALIGN_ASSET(2) char sMamaLetterName[] = MM_NAME("SpecialDeliveryToMama");

typedef struct ComboItem ComboItem;
typedef void (*ComboDraw)(PlayState* play, const ComboItem* item);

typedef struct {
    const char* opaque[2];
    const char* translucent;
} ComboModel;

struct ComboItem {
    const char* key;
    const char* icon;
    const char* name;
    ComboDraw draw;
    ComboModel model;
    Color_RGB8 frogColor;
    const char* label;
    const char* pause;
    const char* text;
};

typedef struct {
    SkelAnime skelAnime;
    Vec3s jointTable[FROG_LIMB_MAX];
    Vec3s morphTable[FROG_LIMB_MAX];
    bool isReady;
} FrogModel;

static void DrawLayers(PlayState* play, const ComboItem* item);
static void DrawMoonsTear(PlayState* play, const ComboItem* item);
static void DrawFrog(PlayState* play, const ComboItem* item);

static const ComboItem sItems[] = {
    { "nei.combo.land_deed",
      sLandDeedIcon,
      sLandDeedName,
      DrawLayers,
      { { sDeedDL, sLandDeedColorDL }, NULL },
      { 0, 0, 0 },
      TEXTS("Land Title Deed", "Trade this Clock Town deed to the Southern Swamp Business Scrub.") },
    { "nei.combo.swamp_deed",
      sSwampDeedIcon,
      sSwampDeedName,
      DrawLayers,
      { { sDeedDL, sSwampDeedColorDL }, NULL },
      { 0, 0, 0 },
      TEXTS("Swamp Title Deed", "Trade this swamp deed to the Goron Village Business Scrub.") },
    { "nei.combo.mountain_deed",
      sMountainDeedIcon,
      sMountainDeedName,
      DrawLayers,
      { { sDeedDL, sMountainDeedColorDL }, NULL },
      { 0, 0, 0 },
      TEXTS("Mountain Title Deed", "Trade this mountain deed to the Zora Hall Business Scrub.") },
    { "nei.combo.ocean_deed",
      sOceanDeedIcon,
      sOceanDeedName,
      DrawLayers,
      { { sDeedDL, sOceanDeedColorDL }, NULL },
      { 0, 0, 0 },
      TEXTS("Ocean Title Deed", "Trade this ocean deed to the Ikana Canyon Business Scrub.") },
    { "nei.combo.blue_frog",
      sBlueFrogIcon,
      sBlueFrogName,
      DrawFrog,
      { { NULL }, NULL },
      { 120, 130, 230 },
      TEXTS("Blue Frog", "Reunite this blue singer with the Mountain Village frog choir.") },
    { "nei.combo.cyan_frog",
      sCyanFrogIcon,
      sCyanFrogName,
      DrawFrog,
      { { NULL }, NULL },
      { 0, 170, 200 },
      TEXTS("Cyan Frog", "Reunite this cyan singer with the Mountain Village frog choir.") },
    { "nei.combo.pink_frog",
      sPinkFrogIcon,
      sPinkFrogName,
      DrawFrog,
      { { NULL }, NULL },
      { 210, 120, 100 },
      TEXTS("Pink Frog", "Reunite this pink singer with the Mountain Village frog choir.") },
    { "nei.combo.white_frog",
      sWhiteFrogIcon,
      sWhiteFrogName,
      DrawFrog,
      { { NULL }, NULL },
      { 190, 190, 190 },
      TEXTS("White Frog", "Reunite this white singer with the Mountain Village frog choir.") },
    { "nei.combo.moons_tear",
      sMoonsTearIcon,
      sMoonsTearName,
      DrawMoonsTear,
      { { sMoonsTearDL }, sMoonsTearGlowDL },
      { 0, 0, 0 },
      TEXTS("Moon's Tear", "Trade this fallen moon stone for the Clock Town land deed.") },
    { "nei.combo.room_key",
      sRoomKeyIcon,
      sRoomKeyName,
      DrawLayers,
      { { sRoomKeyDL }, sRoomKeyTagDL },
      { 0, 0, 0 },
      TEXTS("Room Key", "Open your room at the Stock Pot Inn, even after the inn closes.") },
    { "nei.combo.letter_to_kafei",
      sKafeiLetterIcon,
      sKafeiLetterName,
      DrawLayers,
      { { sKafeiLetterDL }, sKafeiLetterWritingDL },
      { 0, 0, 0 },
      TEXTS("Letter to Kafei", "Mail Anju's letter to arrange a meeting with Kafei.") },
    { "nei.combo.express_mail",
      sMamaLetterIcon,
      sMamaLetterName,
      DrawLayers,
      { { sMamaLetterDL }, sMamaLetterWritingDL },
      { 0, 0, 0 },
      TEXTS("Express Mail to Mama", "Deliver Kafei's urgent letter to his mother, Madame Aroma.") },
};

static const SOHModApi* sApi;
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static FrogModel sFrog;

static void DrawLayers(PlayState* play, const ComboItem* item) {
    const ComboModel* model = &item->model;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    for (u32 i = 0; i < ARRAY_COUNT(model->opaque) && model->opaque[i] != NULL; i++) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)model->opaque[i]);
    }
    if (model->translucent != NULL) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)model->translucent);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// OoT has no animated materials, so the two scrolls of MM's gGiMoonsTearTexAnim (segments 9 and 10) are fed by hand.
static void DrawMoonsTear(PlayState* play, const ComboItem* item) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u32 frames = play->gameplayFrames;
    u32 doubleRise = (u32)(-2 * (s32)frames);
    Gfx* firstScroll = Gfx_TwoTexScroll(gfxCtx, 0, frames, frames, TEAR_SCROLL_SIZE, TEAR_SCROLL_SIZE, 1, 0, 0,
                                        TEAR_SCROLL_SIZE, TEAR_SCROLL_SIZE);
    Gfx* secondScroll = Gfx_TwoTexScroll(gfxCtx, 0, 0, 0, TEAR_SCROLL_SIZE, TEAR_SCROLL_SIZE, 1, frames, doubleRise,
                                         TEAR_SCROLL_SIZE, TEAR_SCROLL_SIZE);

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_25Opa(gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)firstScroll);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)secondScroll);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)item->model.opaque[0]);
    Gfx_SetupDL_25Xlu(gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)firstScroll);
    gSPSegment(POLY_XLU_DISP++, 0x0A, (uintptr_t)secondScroll);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)item->model.translucent);
    CLOSE_DISPS(gfxCtx);
}

static bool IsFrogEye(s32 limbIndex) {
    return limbIndex == FROG_LIMB_RIGHT_EYE || limbIndex == FROG_LIMB_LEFT_EYE;
}

static s32 OverrideFrogLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* frog) {
    (void)play;
    (void)rot;
    (void)frog;
    if (limbIndex == FROG_LIMB_LOWER_BODY) {
        pos->z -= FROG_BODY_SETBACK;
    }
    if (IsFrogEye(limbIndex)) {
        *dList = NULL;
    }
    return false;
}

static void DrawFrogEye(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* frog) {
    (void)rot;
    (void)frog;
    if (!IsFrogEye(limbIndex)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, *dList);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Initialised once: every SkelAnime_InitFlex registers the skeleton with the host again.
static bool TryLoadFrog(PlayState* play) {
    if (sFrog.isReady) {
        return true;
    }
    if (!sApi->HasResource(sFrogSkel) || !sApi->HasResource(sFrogIdleAnim)) {
        return false;
    }
    SkelAnime_InitFlex(play, &sFrog.skelAnime, (FlexSkeletonHeader*)sFrogSkel, (AnimationHeader*)sFrogIdleAnim,
                       sFrog.jointTable, sFrog.morphTable, FROG_LIMB_MAX);
    sFrog.isReady = true;
    return true;
}

static void DrawFrog(PlayState* play, const ComboItem* item) {
    if (!TryLoadFrog(play)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, FROG_HEIGHT_OFFSET, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(FROG_SCALE, FROG_SCALE, FROG_SCALE, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sFrogIrisTex);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sFrogIrisTex);
    gDPSetEnvColor(POLY_OPA_DISP++, item->frogColor.r, item->frogColor.g, item->frogColor.b, 255);
    SkelAnime_DrawFlexOpa(play, sFrog.skelAnime.skeleton, sFrog.skelAnime.jointTable, sFrog.skelAnime.dListCount,
                          OverrideFrogLimb, DrawFrogEye, NULL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The host overwrites drawItemId with ITEM_CUSTOM; tableId carries the runtime id instead.
static const ComboItem* FindShownItem(const GetItemEntry* entry) {
    const SOHCustomItemDefinition* shown = sApi->FindCustomItemByRuntimeId(entry->tableId);

    if (shown == NULL) {
        return NULL;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sItems); i++) {
        if (strcmp(shown->key, sItems[i].key) == 0) {
            return &sItems[i];
        }
    }
    return NULL;
}

static void DrawReward(PlayState* play, GetItemEntry* entry) {
    const ComboItem* item = FindShownItem(entry);

    if (item == NULL) {
        return;
    }
    item->draw(play, item);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    for (u16 i = 0; i < ARRAY_COUNT(sItems); i++) {
        const ComboItem* item = &sItems[i];
        SOHCustomItemDefinition definition = Z64Items_Define(item->key, item->icon, item->name);
        SOHCustomItemRandomizer randomizer = { 0 };
        randomizer.structSize = sizeof(randomizer);
        randomizer.type = SOH_CUSTOM_ITEM_TYPE_ITEM;
        randomizer.poolCount = 0;
        randomizer.logicKey = item->key;
        randomizer.hintKey = item->key;
        definition.displayName = item->label;
        definition.pauseText = item->pause;
        definition.getItemText = item->text;
        definition.getItemEntry.drawFunc = DrawReward;
        definition.presentationFlags = SOH_ITEM_HIDE_FROM_KALEIDO;
        definition.randomizer = &randomizer;
        if (!Z64Items_Register(sApi, &definition)) {
            return;
        }
    }
}
