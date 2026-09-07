/**
 * form_song_jingle.inc.c - the OoT song jingle, sung by the active form.
 *
 * After a song is played, OoT answers with a jingle: sequence 068..073, ONE channel,
 * `instr 53` (the ocarina) bound to soundfont 0. MM makes that instrument per-form by
 * reading it off seq player io port 7, but OoT's sequences neither read that port nor
 * could use the answer — the form voices live in MM's soundfont 0, and instrument 120
 * (Igos) does not exist in OoT's, which only has 92.
 *
 * So the melody is played rather than re-voiced: the note lists below were extracted from
 * oot.o2r by tools/gen_jingle_table.py and are fed to the same MM_FONT synth that already
 * voices the form's own notes, while Audio_PlayFanfare is skipped.
 *
 * Only the six single-channel ocarina jingles are here. The six warp songs are full
 * orchestral arrangements on soundfont 18 and are left alone.
 */

#define JINGLE_REST 127

// All six channels carry the same settings, so they are constants rather than a column:
// vol 127, pan 64 (centre), reverb 40, no vibrato.
#define JINGLE_PAN 64
#define JINGLE_REVERB (40.0f / 127.0f)

typedef struct {
    s8 semitone; // from C4, matching OcarinaPitch; JINGLE_REST for a silence
    u16 delay;   // sequence ticks before the NEXT event
    u8 velocity; // 0..127
    // How much of `delay` is silence at the end, in 256ths — the sequence player releases
    // the note once the remaining delay reaches (gate * delay) >> 8. 0 means fully legato,
    // and it is what separates Saria's staccato F and A from Zelda's held notes.
    u8 gate;
} FormJingleNote;

static const FormJingleNote sJingleSaria[] = {
    { 5, 24, 120, 171 }, { 9, 24, 127, 182 },       { 11, 48, 120, 43 },  { 5, 24, 111, 203 },  { 9, 24, 120, 171 },
    { 11, 48, 123, 43 }, { 5, 24, 111, 96 },        { 9, 24, 105, 203 },  { 11, 24, 126, 192 }, { 16, 24, 117, 171 },
    { 14, 48, 117, 38 }, { 11, 24, 111, 214 },      { 12, 24, 126, 203 }, { 11, 24, 111, 43 },  { 7, 24, 120, 171 },
    { 4, 115, 120, 0 },  { JINGLE_REST, 29, 0, 0 },
};

static const FormJingleNote sJingleEpona[] = {
    { 14, 24, 108, 0 }, { 11, 24, 122, 0 },        { 9, 96, 124, 6 },          { 14, 48, 127, 128 },
    { 9, 96, 120, 24 }, { 14, 48, 118, 123 },      { 9, 48, 120, 0 },          { 11, 48, 112, 16 },
    { 9, 108, 124, 0 }, { JINGLE_REST, 36, 0, 0 }, { JINGLE_REST, 168, 0, 0 }, { 11, 144, 114, 214 },
    { 11, 23, 116, 0 }, { JINGLE_REST, 1, 0, 0 },
};

static const FormJingleNote sJingleZelda[] = {
    { 11, 96, 123, 24 }, { 14, 48, 125, 64 }, { 9, 144, 115, 34 }, { 11, 96, 125, 27 },
    { 14, 48, 127, 64 }, { 9, 144, 115, 24 }, { 11, 96, 122, 16 }, { 14, 48, 120, 86 },
    { 21, 96, 124, 11 }, { 19, 48, 127, 70 }, { 14, 135, 122, 0 }, { JINGLE_REST, 3, 0, 0 },
};

static const FormJingleNote sJingleSuns[] = {
    { 9, 12, 99, 150 },  { 5, 12, 93, 128 },   { 14, 36, 124, 200 },       { 9, 12, 99, 150 },
    { 5, 12, 93, 128 },  { 14, 42, 124, 208 }, { 9, 12, 107, 107 },        { 14, 12, 120, 86 },
    { 17, 7, 127, 110 }, { 19, 8, 127, 128 },  { 19, 8, 118, 160 },        { 19, 8, 110, 160 },
    { 19, 8, 102, 160 }, { 19, 8, 98, 160 },   { 19, 8, 88, 192 },         { 19, 7, 76, 147 },
    { 19, 8, 66, 160 },  { 19, 4, 58, 0 },     { JINGLE_REST, 106, 0, 0 }, { JINGLE_REST, 132, 0, 0 },
    { 12, 12, 116, 64 }, { 16, 6, 118, 0 },    { JINGLE_REST, 4, 0, 0 },
};

static const FormJingleNote sJingleTime[] = {
    { 9, 48, 110, 0 },        { 2, 96, 127, 0 },          { 5, 48, 112, 64 },   { 9, 48, 112, 0 },
    { 2, 96, 122, 0 },        { 5, 48, 95, 0 },           { 9, 24, 117, 22 },   { 12, 72, 127, 143 },
    { 7, 72, 91, 86 },        { 7, 72, 119, 153 },        { 2, 72, 109, 72 },   { 4, 29, 109, 0 },
    { JINGLE_REST, 1, 0, 0 }, { JINGLE_REST, 432, 0, 0 }, { 11, 96, 121, 123 }, { 5, 48, 103, 107 },
    { 9, 96, 113, 128 },      { 0, 48, 117, 123 },        { 2, 142, 106, 0 },   { JINGLE_REST, 2, 0, 0 },
};

static const FormJingleNote sJingleStorm[] = {
    { 2, 12, 127, 0 },   { 5, 12, 119, 0 },    { 14, 48, 127, 32 }, { 2, 12, 125, 0 },         { 5, 12, 119, 22 },
    { 14, 48, 127, 32 }, { 16, 36, 120, 86 },  { 17, 12, 120, 0 },  { 16, 12, 114, 64 },       { 17, 12, 112, 86 },
    { 16, 12, 120, 86 }, { 12, 12, 120, 107 }, { 9, 54, 112, 0 },   { JINGLE_REST, 42, 0, 0 },
};

typedef struct {
    const FormJingleNote* notes;
    u16 count;
    u8 tempo; // the sequence's own `tempo` command, in quarter notes per minute
} FormJingle;

#define JINGLE_ROW(name, tempo) \
    { name, (u16)(sizeof(name) / sizeof(name[0])), tempo }

// Indexed by OcarinaSongId. The warp songs (0..5) hold no row.
static const FormJingle sFormJingles[] = {
    { NULL, 0, 0 },
    { NULL, 0, 0 },
    { NULL, 0, 0 },
    { NULL, 0, 0 },
    { NULL, 0, 0 },
    { NULL, 0, 0 },
    JINGLE_ROW(sJingleSaria, 135), // OCARINA_SONG_SARIAS
    JINGLE_ROW(sJingleEpona, 110), // OCARINA_SONG_EPONAS
    JINGLE_ROW(sJingleZelda, 145), // OCARINA_SONG_ZELDAS
    JINGLE_ROW(sJingleSuns, 70),   // OCARINA_SONG_SUNS
    JINGLE_ROW(sJingleTime, 125),  // OCARINA_SONG_TIME
    JINGLE_ROW(sJingleStorm, 105), // OCARINA_SONG_STORMS
};

static struct {
    const FormJingleNote* notes;
    u16 count;
    u16 index;
    f32 ticksToNext;    // until the next event in the list
    f32 ticksToRelease; // until the sounding note reaches its gate point
    f32 ticksPerUpdate;
    u8 instrument; // latched at the start, so losing the form mid-song cannot re-voice it
    u8 active;
    u8 sounding;
} sFormJingle = {};

// A quarter note is 48 ticks, so a sequence at `tempo` runs tempo*48/60 ticks per second.
// R_UPDATE_RATE is how many 60ths of a second one game update covers.
static f32 FormJingle_TicksPerUpdate(u8 tempo) {
    return ((f32)tempo * 48.0f / 60.0f) * ((f32)R_UPDATE_RATE / 60.0f);
}

void FormJingle_Stop(void) {
    if (sFormJingle.active) {
        MmGakki_StopNote();
    }
    sFormJingle.active = 0;
    sFormJingle.sounding = 0;
    sFormJingle.notes = NULL;
}

// Returns 1 when it has taken the song over, so the caller skips Audio_PlayFanfare.
s32 FormJingle_Start(s32 songId) {
    // Every form that names a song voice sings, not just the MM_FONT ones: the Gerudo's
    // Malon and Keaton's flute are OoT instruments the engine would pick for the ocarina,
    // but a jingle WE synthesise has to come out of MM's soundfont either way.
    s32 instrument = MmGakki_GetSongInstrument(gFormState.currentForm);
    if (instrument <= 0) {
        return 0; // no song voice — vanilla's jingle is the right one
    }
    if (songId < 0 || songId >= (s32)(sizeof(sFormJingles) / sizeof(sFormJingles[0])) ||
        sFormJingles[songId].notes == NULL) {
        return 0;
    }

    const FormJingle* jingle = &sFormJingles[songId];
    FormJingle_Stop();
    sFormJingle.notes = jingle->notes;
    sFormJingle.count = jingle->count;
    sFormJingle.index = 0;
    sFormJingle.ticksToNext = 0.0f;
    sFormJingle.ticksToRelease = 0.0f;
    sFormJingle.ticksPerUpdate = FormJingle_TicksPerUpdate(jingle->tempo);
    sFormJingle.instrument = (u8)instrument;
    sFormJingle.active = 1;
    sFormJingle.sounding = 0;
    MMFORM_LOG("[MmForm] jingle: song=%d form=%d inst=%d notes=%d tempo=%d", songId, gFormState.currentForm, instrument,
               jingle->count, jingle->tempo);
    return 1;
}

static void FormJingle_StartNote(const FormJingleNote* note) {
    Player* player = (gPlayState != NULL) ? GET_PLAYER(gPlayState) : NULL;
    Vec3f* pos = (player != NULL) ? &player->actor.projectedPos : NULL;

    MmGakki_PlayInstrumentPitch(sFormJingle.instrument, (u8)note->semitone, 1.0f, pos);

    MmGakkiNoteShape shape;
    shape.volumeScale = (f32)note->velocity / 127.0f;
    shape.reverb = JINGLE_REVERB;
    shape.pan = JINGLE_PAN;
    shape.sustain = 1;
    MmGakki_ShapeActiveNote(&shape);

    sFormJingle.ticksToRelease = (f32)note->delay - (f32)(((u32)note->gate * note->delay) >> 8);
    sFormJingle.sounding = 1;
}

void FormJingle_Update(void) {
    if (!sFormJingle.active) {
        return;
    }
    // The form can be dropped mid-jingle (damage, a mask change); its voice goes with it.
    if (MmGakki_GetSongInstrument(gFormState.currentForm) != (s32)sFormJingle.instrument) {
        FormJingle_Stop();
        return;
    }

    if (sFormJingle.sounding) {
        sFormJingle.ticksToRelease -= sFormJingle.ticksPerUpdate;
        if (sFormJingle.ticksToRelease <= 0.0f) {
            MmGakki_ReleaseNote();
            sFormJingle.sounding = 0;
        }
    }

    sFormJingle.ticksToNext -= sFormJingle.ticksPerUpdate;
    while (sFormJingle.ticksToNext <= 0.0f) {
        if (sFormJingle.index >= sFormJingle.count) {
            FormJingle_Stop();
            return;
        }

        const FormJingleNote* note = &sFormJingle.notes[sFormJingle.index];
        sFormJingle.index++;
        sFormJingle.ticksToNext += (f32)note->delay;

        if (note->semitone == JINGLE_REST) {
            MmGakki_ReleaseNote();
            sFormJingle.sounding = 0;
            continue;
        }
        FormJingle_StartNote(note);
    }
}
