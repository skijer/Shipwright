#include "global.h"
#include "objects/object_gi_hearts/object_gi_hearts.h"
#include "objects/object_gi_magicpot/object_gi_magicpot.h"
#include "objects/object_gi_bomb_1/object_gi_bomb_1.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_Item_B_Heart/z_item_b_heart.h"
#include <stdio.h>
#include <string.h>

// The table is the resource-service boundary; functions below are unmodified
// production bodies. Distinct names expose wrong-item and border tinting.
static struct {
    const char* dlists[2];
} sDrawItemTable[256] = {
    [GID_HEART_PIECE] = { { gGiHeartBorderDL, gGiHeartPieceDL } },
    [GID_HEART_CONTAINER] = { { gGiHeartBorderDL, gGiHeartContainerDL } },
    [GID_MAGIC_SMALL] = { { gGiMagicJarSmallDL } },
    [GID_MAGIC_LARGE] = { { gGiMagicJarLargeDL } },
    [GID_BOMB] = { { gGiBombDL } },
};
static int sCustom, sHeartsChanged, sMagicChanged, sFailures, sCases;
static const char* sFirstAltDL;
static Color_RGB8 sHeartsColor = { 53, 167, 225 }, sMagicColor = { 230, 93, 171 };
static Gfx sOpa[128], sXlu[128];
static GraphicsContext sGfx;
static PlayState sPlay;
static Mtx sMatrix;
static EnItem00 sPickup;
static ItemBHeart sBossHeart;
static Actor sWarp;

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (!strcmp(name, "gCosmetics.Consumable.Hearts.Changed"))
        return sHeartsChanged;
    if (!strcmp(name, "gCosmetics.Consumable.Magic.Changed"))
        return sMagicChanged;
    return fallback;
}
Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 fallback) {
    if (!strcmp(name, "gCosmetics.Consumable.Hearts.Value"))
        return sHeartsColor;
    if (!strcmp(name, "gCosmetics.Consumable.Magic.Value"))
        return sMagicColor;
    return fallback;
}
uint8_t ResourceGetIsCustomByName(const char* name) {
    return sCustom;
}
void gSPDisplayList(Gfx* pkt, Gfx* dl) {
    // The real GBI bridge resolves an uncached Alt before submitting its DL,
    // even when an earlier resource query still sees a cached native asset.
    if (sFirstAltDL && !strcmp((const char*)dl, sFirstAltDL))
        sCustom = 1;
    __gSPDisplayList(pkt, dl);
}
void Gfx_SetupDL_25Opa(GraphicsContext* gfx) {
}
void Gfx_SetupDL_25Xlu(GraphicsContext* gfx) {
}
Mtx* Matrix_NewMtx(GraphicsContext* gfx, char* file, s32 line) {
    return &sMatrix;
}
void FrameInterpolation_RecordOpenChild(const void* a, int b) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void func_8002EBCC(Actor* actor, PlayState* play, s32 flag) {
}
void func_8002ED80(Actor* actor, PlayState* play, s32 flag) {
}

#include "custom_item_color_production.inc"

static void Reset(void) {
    memset(&sGfx, 0, sizeof(sGfx));
    memset(&sPlay, 0, sizeof(sPlay));
    memset(sOpa, 0, sizeof(sOpa));
    memset(sXlu, 0, sizeof(sXlu));
    sGfx.polyOpa.p = sOpa;
    sGfx.polyXlu.p = sXlu;
    sPlay.state.gfxCtx = &sGfx;
}
static void CheckStreamWithBorder(const char* label, Gfx* begin, Gfx* end, const char* body, int tint, Color_RGB8 color,
                                  int whiteBorder) {
    int gray = 0, bodySeen = 0, borderSeen = 0, error = 0;
    uint32_t rgba = 0;
    for (Gfx* g = begin; g < end; ++g) {
        unsigned op = (g->words.w0 >> 24) & 255;
        if (op == G_SETINTENSITY)
            rgba = g->words.w1;
        if (op == G_SETGRAYSCALE)
            gray = g->words.w1;
        if (op == G_DL) {
            const char* path = (const char*)g->words.w1;
            if (!strcmp(path, body)) {
                ++bodySeen;
                error |= gray != tint;
                if (tint)
                    error |= rgba != ((uint32_t)color.r << 24 | (uint32_t)color.g << 16 | (uint32_t)color.b << 8 | 255);
            } else if (whiteBorder && !strcmp(path, gGiHeartBorderDL)) {
                ++borderSeen;
                error |= gray != 1 || rgba != 0xFFFFFFFFu;
            } else {
                error |= gray != 0; // Frame/exterior/next item must not inherit tint.
            }
        }
    }
    error |= bodySeen != 1 || gray != 0 || borderSeen != whiteBorder;
    ++sCases;
    if (error) {
        ++sFailures;
        fprintf(stderr, "FAIL %s (expected tint=%d, body draws=%d, final grayscale=%d)\n", label, tint, bodySeen, gray);
    }
}
static void CheckStream(const char* label, Gfx* begin, Gfx* end, const char* body, int tint, Color_RGB8 color) {
    CheckStreamWithBorder(label, begin, end, body, tint, color, 0);
}
static void DrawCases(int tintHearts, int tintMagic) {
    Reset();
    GetItem_DrawXlu01(&sPlay, GID_HEART_PIECE);
    CheckStream("held/3D heart piece", sXlu, sGfx.polyXlu.p, gGiHeartPieceDL, tintHearts, sHeartsColor);
    Reset();
    GetItem_DrawXlu01(&sPlay, GID_HEART_CONTAINER);
    CheckStream("held heart container", sXlu, sGfx.polyXlu.p, gGiHeartContainerDL, tintHearts, sHeartsColor);
    Reset();
    GetItem_DrawOpa0(&sPlay, GID_MAGIC_SMALL);
    GetItem_DrawOpa0(&sPlay, GID_BOMB);
    CheckStream("small magic jar then bomb", sOpa, sGfx.polyOpa.p, gGiMagicJarSmallDL, tintMagic, sMagicColor);
    Reset();
    GetItem_DrawOpa0(&sPlay, GID_MAGIC_LARGE);
    CheckStream("large magic jar", sOpa, sGfx.polyOpa.p, gGiMagicJarLargeDL, tintMagic, sMagicColor);
    Reset();
    EnItem00_DrawHeartContainer(&sPickup, &sPlay);
    CheckStream("placed container interior", sXlu, sGfx.polyXlu.p, gHeartContainerInteriorDL, tintHearts, sHeartsColor);
    Reset();
    EnItem00_DrawHeartPiece(&sPickup, &sPlay);
    CheckStream("placed piece interior", sXlu, sGfx.polyXlu.p, gHeartPieceInteriorDL, tintHearts, sHeartsColor);
    Reset();
    ItemBHeart_Draw(&sBossHeart.actor, &sPlay);
    CheckStream("boss container opaque", sOpa, sGfx.polyOpa.p, gGiHeartContainerDL, tintHearts, sHeartsColor);
    Reset();
    sWarp.id = ACTOR_DOOR_WARP1;
    sWarp.projectedPos.z = 100;
    sWarp.next = NULL;
    sPlay.actorCtx.actorLists[ACTORCAT_ITEMACTION].head = &sWarp;
    ItemBHeart_Draw(&sBossHeart.actor, &sPlay);
    CheckStream("boss container translucent", sXlu, sGfx.polyXlu.p, gGiHeartContainerDL, tintHearts, sHeartsColor);
    Reset();
    Randomizer_DrawDoubleDefense(&sPlay, NULL);
    CheckStreamWithBorder("Double Defense white border and body", sXlu, sGfx.polyXlu.p, gGiHeartContainerDL, tintHearts,
                          sHeartsColor, 1);
}
int main(void) {
    sCustom = 1;
    sHeartsChanged = 1;
    sMagicChanged = 1;
    DrawCases(1, 1);
    sHeartsColor = (Color_RGB8){ 245, 41, 79 };
    sMagicColor = (Color_RGB8){ 12, 219, 234 };
    DrawCases(1, 1); // Fresh CVar values, including rainbow updates, without resource reload.
    sHeartsChanged = 0;
    DrawCases(0, 1);
    sHeartsChanged = 1;
    sMagicChanged = 0;
    DrawCases(1, 0);
    sHeartsChanged = 0;
    DrawCases(0, 0); // Reset preserves original materials.
    sCustom = 0;
    sHeartsChanged = 1;
    sMagicChanged = 1;
    DrawCases(0, 0);
    sFirstAltDL = gGiHeartPieceDL;
    Reset();
    GetItem_DrawXlu01(&sPlay, GID_HEART_PIECE);
    CheckStream("first Alt heart after warm native", sXlu, sGfx.polyXlu.p, gGiHeartPieceDL, 1, sHeartsColor);
    sCustom = 0;
    sFirstAltDL = gGiMagicJarSmallDL;
    Reset();
    GetItem_DrawOpa0(&sPlay, GID_MAGIC_SMALL);
    CheckStream("first Alt magic jar after warm native", sOpa, sGfx.polyOpa.p, gGiMagicJarSmallDL, 1, sMagicColor);
    printf("%s custom item color draw state: %d cases, %d failures\n", sFailures ? "FAIL" : "PASS", sCases, sFailures);
    return sFailures != 0;
}
