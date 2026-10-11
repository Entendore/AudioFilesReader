#include "OggReader.h"
#include <fstream>
#include <vector>
#include <cstdint>

namespace audio {

std::expected<std::variant<AudioData, MidiData>, AudioReadError> readOgg(std::string_view filepath) {
    std::ifstream file(std::string(filepath), std::ios::binary);
    if (!file) return std::unexpected(AudioReadError::FileNotFound);

    char header[4];
    file.read(header, 4);
    if (std::string_view(header, 4) != "OggS") return std::unexpected(AudioReadError::InvalidData);

    // For simplicity: parse only the first Ogg page header, no decoding.

    // Ogg page header is 27 bytes + segment table
    std::vector<uint8_t> pageHeader(27);
    std::copy(header, header + 4, pageHeader.begin());
    if (!file.read(reinterpret_cast<char*>(pageHeader.data() + 4), 23))
        return std::unexpected(AudioReadError::InvalidData);

    uint8_t segmentCount = 0;
    if (!file.read(reinterpret_cast<char*>(&segmentCount), 1)) return std::unexpected(AudioReadError::InvalidData);

    // Skip segment table and page segments
    file.seekg(segmentCount, std::ios::cur);

    // No actual decoding or audio data extraction implemented here
    AudioData audio;
    audio.sampleRate = 0;
    audio.channels = 0;
    audio.samples.clear();

    return std::expected<std::variant<AudioData, MidiData>, AudioReadError>{std::in_place, std::move(audio)};
}

} // namespace audio
