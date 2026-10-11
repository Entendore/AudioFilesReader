#pragma once

#include <string_view>
#include <vector>
#include <variant>
#include <expected>
#include <cstdint>
#include <optional>

namespace audio {

// ===== Audio data (raw PCM samples) =====
struct AudioData {
    std::vector<float> samples; // interleaved floating point samples [-1.0f, 1.0f]
    int sampleRate = 0;         // samples per second
    int channels = 0;           // number of channels
};

// ===== MIDI data =====
struct MidiEvent {
    uint32_t deltaTime;             // delta or absolute time in ticks
    std::vector<uint8_t> data;      // raw MIDI message bytes
};

struct MidiData {
    int ticksPerQuarterNote = 0;
    std::vector<std::vector<MidiEvent>> tracks;  // multiple tracks
};

// ===== Read result variants =====
enum class AudioReadError {
    FileNotFound,
    UnsupportedFormat,
    DecodeError,
    InvalidData,
    Unknown
};

// ===== Audio formats =====
enum class AudioFormat {
    WAV,
    MP3,
    FLAC,
    OGG,
    MIDI,
    UNKNOWN
};

// Detect audio format by file extension or content (basic)
std::optional<AudioFormat> detectAudioFormat(std::string_view filepath);

// General read function that auto-detects format and reads
std::expected<std::variant<AudioData, MidiData>, AudioReadError> readAudioFile(std::string_view filepath);

} // namespace audio
