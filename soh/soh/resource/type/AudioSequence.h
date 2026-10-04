#pragma once

#include <stdint.h>
#include <ship/resource/Resource.h>
#include "audio_sequence_data.h"

namespace SOH {

typedef AudioSequenceData Sequence;

class AudioSequence : public Ship::Resource<Sequence> {
  public:
    using Resource::Resource;

    AudioSequence() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }
    ~AudioSequence();

    Sequence* GetPointer();
    size_t GetPointerSize();

    Sequence sequence;
};
}; // namespace SOH
