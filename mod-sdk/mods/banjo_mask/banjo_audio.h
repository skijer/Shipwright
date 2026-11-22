#ifndef BANJO_AUDIO_H
#define BANJO_AUDIO_H

#include "banjo_host.h"

void BanjoAudio_Register(const SOHModApi* api);
// Game thread only: decodes what BK asked for this frame and hands it to the audio thread.
void BanjoAudio_PlayQueued(void);

#endif
