// Compile the complete production bank engine; only audio output is a boundary.
#include "mods/sound_translator/mm_audio_sfx.h"
// The engine installs its own MM aliases for these OoT macros.
#undef SFX_FLAG
#undef SFX_BANK_SHIFT
#undef SFX_BANK_MASK
#undef SFX_INDEX
#undef SFX_BANK
#include "mods/sound_translator/mm_audio_sfx.cpp"

extern "C" void Audio_SetVolScale(u8, u8, u8, u8) {}
extern "C" void MmSfxDispatch_StopEntry(u8, MmSfxBankEntry*, u8) {}
