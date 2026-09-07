/**
 * sensor_rune.c — the Sheikah Sensor rune (Skijer's NEI). Included from item_sheikah_slate.c.
 *
 * The old Desire Sensor, rehoused: casting asks the slate where one of the player's five wished-for
 * items is hiding. The wish list is consulted in slot order and the first item still out there is
 * the one answered for, so the list doubles as a priority order.
 *
 * The answer costs a HEART CONTAINER, permanently. Nothing is charged until the player says yes to
 * the prompt, and a cast that has nothing to answer refuses before the prompt ever opens.
 */

// One heart of MAXIMUM health, and the capacity the rune refuses to drop the player below.
#define SENSOR_HEART_COST 0x10
#define SENSOR_CAPACITY_FLOOR 0x30

#define SENSOR_SENSING_FRAMES 50 // ~2.5s at the 20Hz update rate
#define SENSOR_REVEAL_FRAMES 15

// Built by the randomizer side (OTRGlobals.cpp); the textbox reads the cached string back.
#define TEXT_SENSOR_HINT 0x9300
#define TEXT_SENSOR_PROMPT 0x9301

extern u8 Randomizer_SensorBuildHint(void);

typedef enum {
    SENSOR_PHASE_IDLE,
    SENSOR_PHASE_PROMPT,  // Yes/No on paying the heart container
    SENSOR_PHASE_SENSING, // paid: the slate reaches out
    SENSOR_PHASE_REVEAL,
    SENSOR_PHASE_HINT
} SensorPhase;

static u8 sSensorPhase = SENSOR_PHASE_IDLE;
static s16 sSensorTimer = 0;

static void Sensor_SpawnSparkles(PlayState* play, Player* player) {
    Vec3f accel = { 0.0f, 0.05f, 0.0f };
    Color_RGBA8 primColor = { 180, 120, 255, 255 };
    Color_RGBA8 envColor = { 80, 40, 200, 255 };
    s32 i;

    for (i = 0; i < 3; i++) {
        s16 angle = (s16)(Rand_ZeroOne() * 0xFFFF);
        f32 dist = 15.0f + (Rand_ZeroOne() * 25.0f);
        Vec3f pos;
        Vec3f vel;

        pos.x = player->actor.world.pos.x + (Math_SinS(angle) * dist);
        pos.y = player->actor.world.pos.y + 20.0f + Rand_CenteredFloat(40.0f);
        pos.z = player->actor.world.pos.z + (Math_CosS(angle) * dist);
        vel.x = Math_SinS(angle) * 0.3f;
        vel.y = 1.5f + (Rand_ZeroOne() * 1.0f);
        vel.z = Math_CosS(angle) * 0.3f;

        EffectSsKiraKira_SpawnFocused(play, &pos, &vel, &accel, &primColor, &envColor, 500, 18);
    }
}

static void Sensor_SpawnAnswerBurst(PlayState* play, Player* player) {
    Color_RGBA8 primColor = { 255, 255, 100, 255 };
    Color_RGBA8 envColor = { 255, 200, 0, 255 };
    Vec3f accel = { 0.0f, -0.1f, 0.0f };
    s32 i;

    for (i = 0; i < 16; i++) {
        s16 angle = (s16)(Rand_ZeroOne() * 0xFFFF);
        f32 dist = 5.0f + (Rand_ZeroOne() * 40.0f);
        Vec3f pos;
        Vec3f vel;

        pos.x = player->actor.world.pos.x + (Math_SinS(angle) * dist);
        pos.y = player->actor.world.pos.y + 30.0f + Rand_CenteredFloat(30.0f);
        pos.z = player->actor.world.pos.z + (Math_CosS(angle) * dist);
        vel.x = Math_SinS(angle) * 3.0f;
        vel.y = 2.0f + (Rand_ZeroOne() * 4.0f);
        vel.z = Math_CosS(angle) * 3.0f;

        EffectSsKiraKira_SpawnFocused(play, &pos, &vel, &accel, &primColor, &envColor, 1000, 30);
    }
}

// Held every frame of the consultation: the player watches, he does not walk out of it.
static void Sensor_HoldPlayer(Player* player) {
    player->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED;
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
}

static void Sensor_Release(Player* player) {
    player->stateFlags1 &= ~(PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED);
    sSensorPhase = SENSOR_PHASE_IDLE;
    sSensorTimer = 0;
}

// The price of an answer: one heart of capacity, gone for good.
static void Sensor_PayHeartContainer(Player* player) {
    gSaveContext.healthCapacity -= SENSOR_HEART_COST;
    if (gSaveContext.health > gSaveContext.healthCapacity) {
        gSaveContext.health = gSaveContext.healthCapacity;
    }
    Audio_PlaySoundGeneral(NA_SE_VO_LI_DAMAGE_S, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    Rumble_Request(200.0f, 180, 20, 40);
}

/**
 * Open the consultation. Every refusal returns 0 and the slate's own cast path plays the error
 * sound for it. The hint is built FIRST: a wish list with nothing outstanding on it must cost
 * nothing, so that refusal has to happen before the prompt.
 */
s32 Sensor_Cast(PlayState* play, Player* player) {
    if (sSensorPhase != SENSOR_PHASE_IDLE) {
        return 0;
    }
    if (!IS_RANDO || !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return 0;
    }
    if (gSaveContext.healthCapacity < (SENSOR_CAPACITY_FLOOR + SENSOR_HEART_COST)) {
        return 0;
    }
    if (!Randomizer_SensorBuildHint()) {
        return 0;
    }

    Sensor_HoldPlayer(player);
    Message_StartTextbox(play, TEXT_SENSOR_PROMPT, NULL);
    sSensorPhase = SENSOR_PHASE_PROMPT;
    sSensorTimer = 0;
    return 1;
}

static void Sensor_UpdatePrompt(PlayState* play, Player* player) {
    sSensorTimer++;

    // B or a long timeout backs out, so a mis-parsed textbox can never trap the player (the same
    // hard escape the Time Gate prompt carries).
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) || (sSensorTimer > 1200)) {
        Message_CloseTextbox(play);
        play->msgCtx.msgMode = MSGMODE_TEXT_DONE;
        Sensor_Release(player);
        return;
    }
    if ((Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE) || !Message_ShouldAdvance(play)) {
        return;
    }

    Message_CloseTextbox(play);
    play->msgCtx.msgMode = MSGMODE_TEXT_DONE;

    if (play->msgCtx.choiceIndex != 0) {
        Sensor_Release(player);
        return;
    }

    Sensor_PayHeartContainer(player);
    sSensorPhase = SENSOR_PHASE_SENSING;
    sSensorTimer = 0;
}

static void Sensor_UpdateSensing(PlayState* play, Player* player) {
    sSensorTimer++;

    if ((sSensorTimer >= 5) && ((sSensorTimer % ((sSensorTimer < 25) ? 4 : 2)) == 0)) {
        Sensor_SpawnSparkles(play, player);
    }
    if ((sSensorTimer > 30) && ((sSensorTimer % 6) == 0)) {
        Rumble_Request(50.0f, 80, 8, 4);
    }

    if (sSensorTimer >= SENSOR_SENSING_FRAMES) {
        sSensorPhase = SENSOR_PHASE_REVEAL;
        sSensorTimer = 0;
    }
}

static void Sensor_UpdateReveal(PlayState* play, Player* player) {
    sSensorTimer++;

    if (sSensorTimer == 1) {
        Sensor_SpawnAnswerBurst(play, player);
        Audio_PlaySoundGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        Rumble_Request(300.0f, 255, 40, 80);
    }

    if (sSensorTimer >= SENSOR_REVEAL_FRAMES) {
        Message_StartTextbox(play, TEXT_SENSOR_HINT, NULL);
        sSensorPhase = SENSOR_PHASE_HINT;
        sSensorTimer = 0;
    }
}

static void Sensor_UpdateHint(PlayState* play, Player* player) {
    u8 msgState = Message_GetState(&play->msgCtx);

    sSensorTimer++;
    if (sSensorTimer <= 5) {
        return; // the textbox has not opened yet — TEXT_STATE_NONE here would end it instantly
    }
    if ((msgState == TEXT_STATE_CLOSING) || (msgState == TEXT_STATE_NONE)) {
        Sensor_Release(player);
    }
}

/**
 * Runs before the slate's own input handling and OWNS the frame while a consultation is up —
 * returns 1 for the caller to bail out, so nothing can stow the tablet or fire a second cast
 * out from under the textbox.
 */
u8 Sensor_Tick(PlayState* play, Player* player) {
    if (sSensorPhase == SENSOR_PHASE_IDLE) {
        return 0;
    }

    // The scene took the player away mid-consultation (a warp, a death): drop it, textbox and all.
    if (player->actor.update == NULL) {
        sSensorPhase = SENSOR_PHASE_IDLE;
        sSensorTimer = 0;
        return 0;
    }

    Sensor_HoldPlayer(player);

    switch (sSensorPhase) {
        case SENSOR_PHASE_PROMPT:
            Sensor_UpdatePrompt(play, player);
            break;
        case SENSOR_PHASE_SENSING:
            Sensor_UpdateSensing(play, player);
            break;
        case SENSOR_PHASE_REVEAL:
            Sensor_UpdateReveal(play, player);
            break;
        case SENSOR_PHASE_HINT:
            Sensor_UpdateHint(play, player);
            break;
        default:
            Sensor_Release(player);
            break;
    }
    return 1;
}
