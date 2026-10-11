#pragma once
#include <string_view>
#include <variant>
#include <expected>
#include "AudioReader.h"

namespace audio {

// Reads a WAV file and returns AudioData or error
std::expected<std::variant<AudioData, MidiData>, AudioReadError> readWav(std::string_view filepath);

} // namespace audio
