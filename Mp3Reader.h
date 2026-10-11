#pragma once
#include <string_view>
#include <variant>
#include <expected>
#include "AudioReader.h"

namespace audio {

// Minimal MP3 parser that extracts basic info; full decode not implemented.
std::expected<std::variant<AudioData, MidiData>, AudioReadError> readMp3(std::string_view filepath);

} // namespace audio
