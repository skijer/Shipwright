static void DrawTridentThunder(Actor* thisx, PlayState* play2) {
    static f32 sSpinChargeScale[] = { 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.25f, 0.2f, 0.15f };
    PlayState* play = play2;
    EnMThunder* this = (EnMThunder*)thisx;
    Player* player = GET_PLAYER(play);
    f32 phi_f14;
    s32 phi_t1;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    switch (this->attackStrength) {
        case 0:
        case 1:
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       (uintptr_t)Gfx_TwoTexScrollEx(
                           play->state.gfxCtx, 0, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 30) & 0xFF), 0, 0x40,
                           0x20, 1, 0xFF - ((u8)(s32)(this->spinTrailTexScroll * 20) & 0xFF), 0, 8, 8, -30, 0, -20, 0));
            break;
    }

    switch (this->attackStrength) {
        case 0:
            if (CVarGetInteger("gCosmetics.SpinAttack.Level2Primary.Changed", 0)) {
                Color_RGB8 color =
                    CVarGetColor24("gCosmetics.SpinAttack.Level2Primary.Value", (Color_RGB8){ 255, 255, 170 });
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, (u8)(this->spinAttackAlpha * 255));
            } else {
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, (u8)(this->spinAttackAlpha * 255));
            }
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gSpinAttack3DL);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gSpinAttack4DL);
            break;
        case 1:
            if (CVarGetInteger("gCosmetics.SpinAttack.Level1Primary.Changed", 0)) {
                Color_RGB8 color =
                    CVarGetColor24("gCosmetics.SpinAttack.Level1Primary.Value", (Color_RGB8){ 170, 255, 255 });
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, (u8)(this->spinAttackAlpha * 255));
            } else {
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, (u8)(this->spinAttackAlpha * 255));
            }
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gSpinAttack1DL);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gSpinAttack2DL);
            break;
    }

    Matrix_Mult(&player->mf_9E0, MTXMODE_NEW);

    ApplyLanceMatrix();
    Matrix_RotateY(-M_PI/2.0f,MTXMODE_APPLY);
    Matrix_Translate(2000.0f,0.0f,0.0f,MTXMODE_APPLY);
    Matrix_Scale(-2.4f,-2.0f,-1.4f,MTXMODE_APPLY);
    Matrix_RotateX(16384.0f,MTXMODE_APPLY);

    if (this->spinChargePercent >= 0.85f) {
        phi_f14 = (sSpinChargeScale[(play->gameplayFrames & 7)] * 6.0f) + 1.0f;
        if (CVarGetInteger("gCosmetics.SpinAttack.Level2Primary.Changed", 0)) {
            Color_RGB8 color =
                CVarGetColor24("gCosmetics.SpinAttack.Level2Primary.Value", (Color_RGB8){ 255, 255, 170 });
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, this->chargeAlpha);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, this->chargeAlpha);
        }
        if (CVarGetInteger("gCosmetics.SpinAttack.Level2Secondary.Changed", 0)) {
            Color_RGB8 color =
                CVarGetColor24("gCosmetics.SpinAttack.Level2Secondary.Value", (Color_RGB8){ 255, 100, 0 });
            gDPSetEnvColor(POLY_XLU_DISP++, color.r, color.g, color.b, 128);
        } else {
            gDPSetEnvColor(POLY_XLU_DISP++, 255, 100, 0, 128);
        }
        phi_t1 = 0x28;
    } else {
        phi_f14 = (sSpinChargeScale[play->gameplayFrames & 7] * 2.0f) + 1.0f;
        if (CVarGetInteger("gCosmetics.SpinAttack.Level1Primary.Changed", 0)) {
            Color_RGB8 color =
                CVarGetColor24("gCosmetics.SpinAttack.Level1Primary.Value", (Color_RGB8){ 170, 255, 255 });
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, this->chargeAlpha);
        } else {
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, this->chargeAlpha);
        }
        if (CVarGetInteger("gCosmetics.SpinAttack.Level1Secondary.Changed", 0)) {
            Color_RGB8 color =
                CVarGetColor24("gCosmetics.SpinAttack.Level1Secondary.Value", (Color_RGB8){ 0, 100, 255 });
            gDPSetEnvColor(POLY_XLU_DISP++, color.r, color.g, color.b, 128);
        } else {
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
        }
        phi_t1 = 0x14;
    }
    Matrix_Scale(1.0f, phi_f14, phi_f14, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gSPSegment(POLY_XLU_DISP++, 0x09,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (play->gameplayFrames * 5) & 0xFF, 0, 0x20, 0x20, 1,
                                  (play->gameplayFrames * 20) & 0xFF, (play->gameplayFrames * phi_t1) & 0xFF, 8, 8, 5,
                                  0, 20, phi_t1));

    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gSpinAttackChargingDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void ReplaceTridentThunder(Actor* actor,PlayState* play,bool* draw) {
    if(!*draw || !IsWielding(GET_PLAYER(play)))return;
    *draw=false;Matrix_Push();DrawTridentThunder(actor,play);Matrix_Pop();
}
