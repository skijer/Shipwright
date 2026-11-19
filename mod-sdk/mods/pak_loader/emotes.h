#pragma once
#include "sounds.h"

namespace pak {
static constexpr size_t kFrameWords = 22 * 3 + 1;

struct Emote {
    std::string key;
    std::string label;
    std::string author;
    std::vector<int16_t> frames;
    size_t frameCount = 0;
    bool loops = false;
    Bytes soundSource;
    Pcm sound;
    bool decoded = false;

    void Decode() {
        if (!decoded && !soundSource.empty()) {
            sound = DecodeOgg(soundSource);
        }
        decoded = true;
    }
};

static bool ReadEmoteFrames(const Bytes& bin, Emote& emote) {
    size_t frameBytes = kFrameWords * 2;
    if (bin.empty() || bin.size() % frameBytes != 0) {
        return false;
    }
    emote.frameCount = bin.size() / frameBytes;
    emote.frames.resize(bin.size() / 2);
    for (size_t i = 0; i < emote.frames.size(); ++i) {
        emote.frames[i] = Be16(bin.data() + i * 2);
    }
    return true;
}

class EmotePlayer {
  public:
    bool IsPlaying() const {
        return emote != nullptr;
    }

    void Start(Emote* next, Mixer& mixer, float volume) {
        next->Decode();
        emote = next;
        frame = 0;
        mixer.Play(&next->sound, volume);
    }

    void Stop() {
        emote = nullptr;
    }

    void Advance() {
        if (emote == nullptr) {
            return;
        }
        if (++frame < emote->frameCount) {
            return;
        }
        if (emote->loops) {
            frame = 0;
        } else {
            emote = nullptr;
        }
    }

    // Assumes jointTable holds at least 23 entries: 22 joints and the face word Link's draw reads after them.
    void Pose(Vec3s* jointTable) {
        if (emote == nullptr || jointTable == nullptr) {
            return;
        }
        memcpy(saved, jointTable, sizeof(saved));
        posed = jointTable;
        const int16_t* source = emote->frames.data() + frame * kFrameWords;
        int16_t* target = reinterpret_cast<int16_t*>(jointTable);
        memcpy(target, source, kFrameWords * sizeof(int16_t));
    }

    void Unpose() {
        if (posed != nullptr) {
            memcpy(posed, saved, sizeof(saved));
            posed = nullptr;
        }
    }

  private:
    Emote* emote = nullptr;
    size_t frame = 0;
    Vec3s saved[23];
    Vec3s* posed = nullptr;
};
} // namespace pak
