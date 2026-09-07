/**
 * object_net.c - Bug-catching Net held model (ITEM_NET / PLAYER_IA_NET)
 *
 * Model lives in soh.o2r (object_nei_net, brought in from the net collection of
 * kite_shield.blend via the blend_to_nei -> c2obj_nei pipeline). Drawn in Link's
 * LEFT hand (Link is left-handed — the sword/item hand), following the forearm->hand
 * direction — so during the Net's spin-attack action (gPlayerAnim_link_fighter_rolling_kiru)
 * it swings exactly like the sword.
 *
 * The in-hand placement is live-tunable via console CVars / the NEI menu sliders:
 *   gNetModel.Scale             uniform scale
 *   gNetModel.X / .Y / .Z       model-local offset
 *   gNetModel.RotX/.RotY/.RotZ  model-local rotation (degrees)
 *
 * Included by object_custom_items.c (unity build).
 */

#ifndef PLAYER_IA_NET
#define PLAYER_IA_NET 0x7E // mirror of extended_player.h to avoid include-order issues
#endif

extern u8 ResourceMgr_FileExists(const char* resName);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);
extern f32 CVarGetFloat(const char* name, f32 defaultValue);

static Gfx* Net_GetCachedDL(const char* otr, Gfx** cache, u8* tried) {
    if (!*tried) {
        *tried = 1;
        if (ResourceMgr_FileExists(otr)) {
            *cache = ResourceMgr_LoadGfxByName(otr);
        }
    }
    return *cache;
}

// Opaque = the handle/rim/wrap. XLU = the semitransparent white netting (fast64
// Transparent layer, drawn on POLY_XLU).
static Gfx* Net_GetDL(void) {
    static Gfx* sDL = NULL;
    static u8 sTried = 0;
    return Net_GetCachedDL("__OTR__objects/object_nei_net/g_net_dl", &sDL, &sTried);
}
static Gfx* Net_GetXluDL(void) {
    static Gfx* sDL = NULL;
    static u8 sTried = 0;
    return Net_GetCachedDL("__OTR__objects/object_nei_net/g_net_xlu_dl", &sDL, &sTried);
}

// World-space catch sample points spanning the WHOLE net DL (grip -> hoop head), recomputed every
// frame in CustomItems_DrawNet with the SAME matrix the model is drawn with (hand bone + gNetModel.*
// CVars) — so the catch volume exactly follows the visible net, tuning included. Model bbox (from
// net_net_mesh vtx_cull): X [-67, 9], Y [-14, 73], Z [-16, 16]; grip at the origin, hoop toward the
// far (-X, +Y) corner. Consumed by Net_CaptureAtBlade (z_player_lib.c).
Vec3f gNetCatchPts[NET_CATCH_PTS];
u8 gNetCatchPtsValid = 0;

void CustomItems_DrawNet(Player* player, PlayState* play) {
    // Called from Player_PostLimbDrawGameplay at PLAYER_LIMB_L_HAND, so the CURRENT matrix is the LEFT
    // HAND BONE. The net therefore inherits the hand's full 3D orientation — including the wrist ROLL
    // — exactly like the sword (a forearm->hand direction reconstruction only had yaw+pitch, so it
    // could not roll during swings/charge). Placement relative to the hand is CVar-tunable in this
    // bone space (gNetModel.*). Identity via heldItemId (the IA is the shared Master Sword IA).
    if (player->heldItemId != ITEM_NET)
        return;

    Gfx* dl = Net_GetDL();
    Gfx* xluDL = Net_GetXluDL();
    if (dl == NULL && xluDL == NULL)
        return;

    OPEN_DISPS(play->state.gfxCtx);

    // The hand-bone matrix bakes in Link's actor scale (~0.01), so a raw scale/offset here would render
    // ~100x too small (invisible). Divide the offset/scale by the actor scale so the constants stay in
    // world magnitude, while inheriting the hand's full rotation (wrist ROLL) from the bone.
    //
    // FINAL in-hand placement (tuned in-game via the old gNetModel.* sliders, now baked): the former
    // CVar editor in SohMenuNEI was removed — these are the canonical values. Skijer's NEI
    f32 as = player->actor.scale.x;
    f32 inv = (as > 0.0001f) ? (1.0f / as) : 100.0f;

    f32 scale = 0.49f * inv;
    f32 ox = 3.2f * inv;
    f32 oy = 4.3f * inv;
    f32 oz = -1.0f * inv;
    f32 rx = -5.0f * (M_PI / 180.0f);
    f32 ry = -169.0f * (M_PI / 180.0f);
    f32 rz = 64.0f * (M_PI / 180.0f);

    Matrix_Push(); // save the hand-bone matrix (restored below so later limbs are unaffected)
    // model-local orientation + offset (tunable), applied on top of the hand-bone matrix
    Matrix_RotateX(rx, MTXMODE_APPLY);
    Matrix_RotateY(ry, MTXMODE_APPLY);
    Matrix_RotateZ(rz, MTXMODE_APPLY);
    Matrix_Translate(ox, oy, oz, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);

    // Catch samples: model-space line from the grip (origin) to the hoop head (~far bbox corner),
    // transformed by the CURRENT matrix — i.e. exactly where the net renders, CVar tuning included.
    {
        static const Vec3f sNetSpan = { -57.0f, 55.0f, 0.0f }; // grip -> hoop-head (model units, see bbox)
        for (s32 i = 0; i < NET_CATCH_PTS; i++) {
            f32 t = (f32)i / (NET_CATCH_PTS - 1);
            Vec3f p = { sNetSpan.x * t, sNetSpan.y * t, sNetSpan.z * t };
            Matrix_MultVec3f(&p, &gNetCatchPts[i]);
        }
        gNetCatchPtsValid = 1;
    }

    // Shared matrix for both layers.
    Mtx* mtx = Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__);

    // Opaque handle/rim/wrap.
    if (dl != NULL) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, dl);
    }

    // Semitransparent white netting.
    if (xluDL != NULL) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, xluDL);
    }

    Matrix_Pop(); // restore the hand-bone matrix
    CLOSE_DISPS(play->state.gfxCtx);
}
