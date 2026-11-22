#pragma once
#include <string_view>
#include <variant>
#include <expected>
#include "AudioReader.h"

namespace audio {

std::expected<std::variant<AudioData, MidiData>, AudioReadError> readFlac(std::string_view filepath);

} // namespace audio
