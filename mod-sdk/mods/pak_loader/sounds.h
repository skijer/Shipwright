#pragma once
#include "archive.h"
#include <cstdlib>
#include <map>
#include <mutex>

extern "C" int stb_vorbis_decode_memory(const unsigned char* data, int length, int* channels, int* sampleRate,
                                        short** output);

namespace pak {
using Pcm = std::vector<int16_t>;
static constexpr int kMixRate = 32000;

static Pcm DecodeOgg(const Bytes& ogg) {
    int channels = 0;
    int rate = 0;
    short* samples = nullptr;
    int frames = stb_vorbis_decode_memory(ogg.data(), int(ogg.size()), &channels, &rate, &samples);
    Pcm clip;
    if (frames <= 0 || samples == nullptr || channels <= 0 || rate <= 0) {
        free(samples);
        return clip;
    }
    size_t length = size_t(double(frames) * kMixRate / rate);
    clip.resize(length);
    for (size_t i = 0; i < length; ++i) {
        double source = double(i) * rate / kMixRate;
        size_t frame = std::min<size_t>(size_t(source), size_t(frames - 1));
        int mixed = 0;
        for (int channel = 0; channel < channels; ++channel) {
            mixed += samples[frame * channels + channel];
        }
        clip[i] = int16_t(mixed / channels);
    }
    free(samples);
    return clip;
}

struct VoicePack {
    std::string key;
    std::string label;
    std::string author;
    std::map<uint16_t, std::vector<Bytes>> sources;
    std::map<uint16_t, std::vector<Pcm>> clips;
    bool decoded = false;

    void Decode() {
        if (decoded) {
            return;
        }
        for (const auto& [id, files] : sources) {
            for (const Bytes& file : files) {
                Pcm clip = DecodeOgg(file);
                if (!clip.empty()) {
                    clips[id].push_back(std::move(clip));
                }
            }
        }
        decoded = true;
    }

    const Pcm* Pick(uint16_t id, uint32_t roll) const {
        auto found = clips.find(id);
        if (found == clips.end() || found->second.empty()) {
            return nullptr;
        }
        return &found->second[roll % found->second.size()];
    }
};

// Runs on the audio thread: it only reads clips the game thread handed over and never frees.
class Mixer {
  public:
    void Play(const Pcm* clip, float volume) {
        if (clip == nullptr || clip->empty()) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        voices.push_back({ clip, 0, volume });
    }

    void StopAll() {
        std::lock_guard<std::mutex> lock(mutex);
        voices.clear();
    }

    void Mix(int16_t* stereo, uint32_t frames) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& voice : voices) {
            for (uint32_t frame = 0; frame < frames && voice.position < voice.clip->size(); ++frame) {
                int sample = int(float((*voice.clip)[voice.position++]) * voice.volume);
                for (int channel = 0; channel < 2; ++channel) {
                    int value = stereo[frame * 2 + channel] + sample;
                    stereo[frame * 2 + channel] = int16_t(std::clamp(value, -32768, 32767));
                }
            }
        }
        voices.erase(std::remove_if(voices.begin(), voices.end(),
                                    [](const Voice& voice) { return voice.position >= voice.clip->size(); }),
                     voices.end());
    }

  private:
    struct Voice {
        const Pcm* clip;
        size_t position;
        float volume;
    };
    std::mutex mutex;
    std::vector<Voice> voices;
};
} // namespace pak
