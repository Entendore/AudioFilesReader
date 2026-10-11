#include "Mp3Reader.h"
#include <fstream>
#include <vector>
#include <cstdint>
#include <iostream>

namespace audio {

// Lookup table for MPEG version and layer
static const char* mpegVersions[] = { "2.5", "reserved", "2", "1" };
static const char* layers[] = { "reserved", "III", "II", "I" };

// Simplified bitrate lookup for MPEG1 Layer III (in kbps)
static const int bitrates[16] = {
    0, 32, 40, 48, 56, 64, 80, 96,
    112, 128, 160, 192, 224, 256, 320, 0
};

static const int sampleRates[4] = { 44100, 48000, 32000, 0 };

// Reads a 4-byte MP3 frame header, returns false if invalid
static bool parseFrameHeader(uint32_t header, int& bitrate, int& sampleRate, int& channels) {
    // Check sync bits (11 bits set)
    if ((header & 0xFFE00000) != 0xFFE00000) return false;

    int versionBits = (header >> 19) & 3;
    int layerBits = (header >> 17) & 3;
    int bitrateBits = (header >> 12) & 0xF;
    int sampleRateBits = (header >> 10) & 3;
    int channelMode = (header >> 6) & 3;

    if (versionBits == 1 || layerBits == 0 || bitrateBits == 0xF || sampleRateBits == 3)
        return false;

    // MPEG Version 1
    if (versionBits != 3) return false; // Only MPEG1 supported here

    if (layerBits != 1) return false; // Only Layer III supported here

    bitrate = bitrates[bitrateBits] * 1000;
    sampleRate = sampleRates[sampleRateBits];
    channels = (channelMode == 3) ? 1 : 2;

    return true;
}

std::expected<std::variant<AudioData, MidiData>, AudioReadError> readMp3(std::string_view filepath) {
    std::ifstream file(std::string(filepath), std::ios::binary);
    if (!file) return std::unexpected(AudioReadError::FileNotFound);

    // Skip ID3v2 header if present
    char id3header[3];
    file.read(id3header, 3);
    if (std::string(id3header, 3) == "ID3") {
        // Skip ID3 tag size (next 6 bytes ignored here)
        file.seekg(6, std::ios::cur);
    } else {
        file.seekg(0);
    }

    int bitrate = 0, sampleRate = 0, channels = 0;
    std::vector<float> samples; // Placeholder, no actual decode

    // Search for first frame header
    while (true) {
        uint8_t buf[4];
        if (!file.read(reinterpret_cast<char*>(buf), 4)) break;
        uint32_t header = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
        if (parseFrameHeader(header, bitrate, sampleRate, channels)) {
            // Found frame header
            break;
        } else {
            // Move back 3 bytes to search next possible header byte-aligned
            file.seekg(-3, std::ios::cur);
        }
    }

    if (bitrate == 0 || sampleRate == 0) {
        return std::unexpected(AudioReadError::InvalidData);
    }

    // Since full MP3 decoding is complex, we just create empty AudioData with header info
    AudioData audio;
    audio.sampleRate = sampleRate;
    audio.channels = channels;
    // No samples decoded here
    audio.samples.clear();

    return std::expected<std::variant<AudioData, MidiData>, AudioReadError>{std::in_place, std::move(audio)};
}

} // namespace audio
