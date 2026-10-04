#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MidnaAudioResources {
bool Enabled();
bool HasModel();
bool ReadClip(const char* path, std::vector<uint8_t>& bytes);
std::vector<std::string> ListClips();
std::string ReadAssignment(const char* event, const char* fallback);
void WriteAssignment(const char* event, const std::string& path);
float Gain();
} // namespace MidnaAudioResources
