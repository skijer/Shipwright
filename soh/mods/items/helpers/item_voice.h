/**
 * item_voice.h - Form-aware player voice for custom items
 *
 * Custom items must NOT call Player_PlaySfx / Audio_PlaySoundGeneral with a raw
 * NA_SE_VO_LI_* id: that bypasses the transformation-mask voice redirect, so a
 * Deku/Goron/Zora/FD/Garo form would still grunt with Link's human voice.
 *
 * Route every item voice grunt through ItemVoice_Play instead. It is the single
 * decision point for "which voice for the current form + age", which keeps each
 * item's behavior self-contained and gives the MM (2ship) port one place to
 * translate: on MM the form voice is native (this->transformation +
 * ageProperties->voiceSfxIdOffset), so this collapses to Player_PlayVoiceSfx.
 */
#ifndef ITEM_VOICE_H
#define ITEM_VOICE_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Play Link's voice grunt for a custom-item action, accounting for the active
 * transformation form.
 *
 * - Transformed to a form with its own voice bank -> the form's voice plays.
 * - Human / Gerudo / Pikachu (or mm.o2r missing) -> Link's OOT voice, age-picked.
 *
 * @param ootAdult OOT *base* (adult) voice id. Keys the form mapping, so it must
 *                 be in 0x6800..0x681F (NA_SE_VO_LI_*, not the _KID variant).
 * @param ootChild OOT child (_KID) voice id, used only for the human fallback.
 */
void ItemVoice_Play(Player* p, u16 ootAdult, u16 ootChild);

/**
 * Single-id form-aware voice for legacy call sites that never split by age.
 * When not transformed it plays exactly ootVoiceId (behavior unchanged); when
 * transformed it maps to the form voice. Use ItemVoice_Play when a distinct
 * child (_KID) id exists.
 */
void ItemVoice_PlayId(Player* p, u16 ootVoiceId);

#ifdef __cplusplus
}
#endif

#endif // ITEM_VOICE_H
