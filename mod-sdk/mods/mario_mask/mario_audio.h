#ifndef MARIO_AUDIO_H
#define MARIO_AUDIO_H

#include "mario_host.h"

void MarioAudio_Register(const SOHModApi* api);
// Game thread only: libsm64's sound engine is not thread safe, so the audio thread just drains what this queues.
void MarioAudio_Refill(void);

#endif
