#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MidnaAudioEvent {
    MIDNA_AUDIO_DASH,
    MIDNA_AUDIO_APPEAR,
    MIDNA_AUDIO_VANISH,
    MIDNA_AUDIO_TARGET_NPC,
    MIDNA_AUDIO_TARGET_ENEMY,
    MIDNA_AUDIO_TARGET_OTHER,
    MIDNA_AUDIO_CALL,
    MIDNA_AUDIO_HINT,
    MIDNA_AUDIO_TALK,
    MIDNA_AUDIO_YAWN,
    MIDNA_AUDIO_EVENT_COUNT
} MidnaAudioEvent;

// Load optional private clips on startup, including while the option is off so
// enabling needs no archive access from the mixer. Native banks never change.
void MidnaAudio_Init(void);
// False means the caller must play its original sound, with its original arguments.
// True includes an available movement cue intentionally suppressed by its cooldown.
bool MidnaAudio_TryPlay(MidnaAudioEvent event);
// One call per 20 Hz Navi update. Only eligible stationary/visible time counts.
// Ineligible updates cancel idle playback; other cues always take priority.
void MidnaAudio_UpdateIdle(bool eligible);
// Adds dry, centered mono clips to the engine's 32 kHz stereo output.
void MidnaAudio_Mix(int16_t* interleavedStereo, size_t frameCount);
// Stop voices at scene/game teardown and whenever the checkbox changes. The
// latter also handles off/on while paused. Keep clips and remaining intervals.
void MidnaAudio_Reset(void);

#ifdef __cplusplus
}

#include <string>
#include <vector>

// Menu operations use the decoded archive bank; no I/O is performed during playback.
std::vector<std::string> MidnaAudio_GetAvailableClips();
const char* MidnaAudio_GetEventLabel(MidnaAudioEvent event);
std::string MidnaAudio_GetAssignment(MidnaAudioEvent event);
// An empty path restores the native cue (silence for the optional idle yawn).
bool MidnaAudio_Assign(MidnaAudioEvent event, const std::string& path);
void MidnaAudio_ResetAssignments();
#endif
