/**
 * item_voice.c - Form-aware player voice for custom items
 *
 * Unity-included from mods/items/logic/custom_items.c (helpers block), so it is
 * in scope for every item_*.c. See item_voice.h for the rationale.
 */

#include "item_voice.h"
#include "functions.h" // Player_PlaySfx
#include "variables.h"
#include "mods/transformation_masks/transformation_masks.h"

void ItemVoice_Play(Player* p, u16 ootAdult, u16 ootChild) {
    // A transformed form with its own voice bank speaks with the form's voice.
    // The mapping is keyed on the OOT base (adult) action id (0x6800..0x681F);
    // TryPlayMmVoice returns 1 when it handled it (or deliberately stayed silent
    // because the sample is absent), so we must not also play Link's voice.
    if (TransformMasks_IsTransformedAny() && TransformMasks_TryPlayMmVoice(ootAdult, &p->actor.projectedPos)) {
        return;
    }

    // Human / Gerudo / Pikachu (or no mm.o2r): Link's OOT voice, by age.
    Player_PlaySfx(p, LINK_IS_ADULT ? ootAdult : ootChild);
}

void ItemVoice_PlayId(Player* p, u16 ootVoiceId) {
    // Same id for both ages: preserves legacy "always this voice" behavior when
    // not transformed, while still routing through the form voice bank.
    ItemVoice_Play(p, ootVoiceId, ootVoiceId);
}
