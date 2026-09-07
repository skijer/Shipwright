// ---------------------------------------------------------------------------
// Tomahawk Throw — first-person aim + equipped-C launch, exactly like the magic rods.
//
// Flow (mirrors FireRod_UpdateFirstPerson in item_rod_fire.c):
//   1. While the hammer/axe is held, C-Up enters first-person aim (the aim camera + pose).
//   2. In aim, pressing the EQUIPPED C-button (the hammer's own button) LAUNCHES the axe in the
//      aimed direction — not B. ItemInput_Update gives us that button + its press, like the rods.
//   3. The axe flies 1:1 like the boomerang: spawned with the aim yaw/pitch and moveTo = NULL so
//      EnBoom_Fly carries it straight in the AIMED direction (Actor_SetProjectileSpeed uses
//      world.rot), then returns to Link when returnTimer hits 0. It draws the axe DL and deals
//      hammer damage (En_Boom params 99).
//   4. C-Up toggles aim off; any other action button / damage / cutscene cancels it.
// ---------------------------------------------------------------------------

static void IKAxe_UpdateThrow(Player* player, PlayState* play) {
    static s16 sFlyingFrames = 0;

    // Recover from FLYING: either the axe was caught (En_Boom clears BOOMERANG_THROWN), or it never
    // came back (despawned on a scene change / killed) and the flag is stuck. Either way → IDLE.
    if (sIKAxeThrowState == IKAXE_THROW_FLYING) {
        u8 caught = !(player->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN);
        u8 lost = (++sFlyingFrames > 480); // ~16s — the axe should have long since returned
        if (caught || lost) {
            sFlyingFrames = 0;
            sIKAxeThrowState = IKAXE_THROW_IDLE;
            if (lost) {
                player->stateFlags1 &= ~PLAYER_STATE1_BOOMERANG_THROWN; // never leave it stuck
            }
            // The boomerang catch chain leaves the RANGED-item upper-action (func_80835800), so the
            // next C-press fires a bow. Restore the hammer's melee upper-action — but ONLY if the
            // hammer is STILL held. If the player swapped weapons mid-flight, forcing the hammer
            // back desyncs heldItemAction (the new weapon shows but acts as the hammer) and
            // permanently breaks its collisions (no recoil/bonk). Leave a swapped weapon alone.
            if (player->heldItemAction == PLAYER_IA_HAMMER) {
                Player_EndIKAxeThrow(player);
            }
        }
    } else {
        sFlyingFrames = 0;
    }

    // Aim only while the hammer/axe is the drawn weapon (like the rods only aim while held).
    if (player->heldItemAction != PLAYER_IA_HAMMER && sIKAxeThrowState != IKAXE_THROW_FLYING) {
        if (sIKAxeThrowState == IKAXE_THROW_CHARGING) {
            FirstPerson_Exit(player, play);
        }
        sIKAxeThrowState = IKAXE_THROW_IDLE;
        return;
    }

    // Which C-button the hammer is on + whether it was pressed this frame (rod-style input).
    ItemInputState in;
    ItemInput_Update(&in, ITEM_HAMMER, player, play);

    switch (sIKAxeThrowState) {
        case IKAXE_THROW_IDLE:
            // C-Up enters first-person aim (camera + pose).
            if (!(player->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN) && player->meleeWeaponState == 0 &&
                CHECK_BTN_ALL(play->state.input[0].press.button, BTN_CUP)) {
                FirstPerson_Init(player, play);
                sIKAxeThrowState = IKAXE_THROW_CHARGING;
            }
            break;

        case IKAXE_THROW_CHARGING:
            FirstPerson_Update(player, play);

            // Launch on the EQUIPPED C-button press (the hammer's button), exactly like the rod fires.
            if (in.isPressed) {
                s16 aimYaw = FirstPerson_GetAimYaw(player);
                s16 aimPitch = FirstPerson_GetAimPitch(player);

                f32 posX = player->actor.world.pos.x + (Math_SinS(aimYaw) * 10.0f);
                f32 posZ = player->actor.world.pos.z + (Math_CosS(aimYaw) * 10.0f);
                EnBoom* axe =
                    (EnBoom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOOM, posX, player->actor.world.pos.y + 30.0f,
                                         posZ, aimPitch, aimYaw, 0, IKAXE_THROW_PARAMS);
                if (axe != NULL) {
                    axe->moveTo = NULL; // no Z-target homing → flies in the AIMED direction, then returns to Link
                    axe->returnTimer = IKAXE_THROW_RETURN;
                    player->boomerangActor = &axe->actor;
                    player->stateFlags1 |= PLAYER_STATE1_BOOMERANG_THROWN;
                    sIKAxeThrown = 1;
                    Audio_PlaySoundGeneral(NA_SE_IT_BOOMERANG_THROW, &player->actor.world.pos, 4,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultReverb);
                }
                FirstPerson_Exit(player, play);
                // Play the real boomerang throw animation and go handsfree (Link can move while
                // the axe is in the air), exactly like the boomerang.
                Player_StartIKAxeThrow(player, play);
                sIKAxeThrowState = IKAXE_THROW_FLYING;
                break;
            }

            // C-Up toggles aim off; any OTHER action button (besides the hammer's) / damage / cutscene cancels.
            {
                u16 exitButtons = BTN_A | BTN_B | BTN_CLEFT | BTN_CRIGHT | BTN_CDOWN | BTN_CUP;
                if (in.equippedButton) {
                    exitButtons &= ~in.equippedButton;
                }
                if (CHECK_BTN_ANY(play->state.input[0].press.button, exitButtons) ||
                    (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED))) {
                    FirstPerson_Exit(player, play);
                    sIKAxeThrowState = IKAXE_THROW_IDLE;
                }
            }
            break;

        case IKAXE_THROW_FLYING:
            // Wait for the axe to come back (handled by the FLYING→IDLE sync at the top).
            break;
    }
}

// Optional reticle while aiming (orange, axe-themed), drawn from the draw dispatch.
void IKAxe_DrawReticle(Player* player, PlayState* play) {
    if (sIKAxeThrowState == IKAXE_THROW_CHARGING) {
        FirstPerson_DrawReticle(player, play, 0.0f, 255, 140, 0);
    }
}
