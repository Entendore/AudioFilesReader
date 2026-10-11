#include "FlacReader.h"
#include <fstream>
#include <vector>
#include <cstdint>
#include <iostream>

namespace audio {

std::expected<std::variant<AudioData, MidiData>, AudioReadError> readFlac(std::string_view filepath) {
    std::ifstream file(std::string(filepath), std::ios::binary);
    if (!file) return std::unexpected(AudioReadError::FileNotFound);

    char signature[4];
    file.read(signature, 4);
    if (std::string_view(signature, 4) != "fLaC")
        return std::unexpected(AudioReadError::InvalidData);

    // Read metadata blocks until STREAMINFO found (type 0)
    bool lastBlock = false;
    uint8_t blockType = 0;
    uint32_t blockLength = 0;

    AudioData audio;

    while (!lastBlock) {
        uint8_t header;
        if (!file.read(reinterpret_cast<char*>(&header), 1)) break;
        lastBlock = (header & 0x80) != 0;
        blockType = header & 0x7F;

        uint8_t lenBuf[3];
        if (!file.read(reinterpret_cast<char*>(lenBuf), 3)) break;
        blockLength = (lenBuf[0] << 16) | (lenBuf[1] << 8) | lenBuf[2];

        if (blockType == 0) { // STREAMINFO
            if (blockLength < 34) return std::unexpected(AudioReadError::InvalidData);
            std::vector<uint8_t> buf(blockLength);
            if (!file.read(reinterpret_cast<char*>(buf.data()), blockLength))
                return std::unexpected(AudioReadError::InvalidData);

            // Parse STREAMINFO:
            // sample rate: 20 bits starting at byte 10 bits 4-23
            // channels: 3 bits starting at byte 12 bits 0-2
            // bits per sample: 5 bits starting at byte 12 bits 3-7 + byte 13 bits 0-1

            uint32_t sampleRate = ((buf[10] << 12) | (buf[11] << 4) | (buf[12] >> 4)) & 0xFFFFF;
            uint8_t channels = ((buf[12] >> 1) & 0x07) + 1;
            uint8_t bitsPerSample = (((buf[12] & 0x01) << 4) | ((buf[13] >> 4) & 0x0F)) + 1;

            audio.sampleRate = sampleRate;
            audio.channels = channels;
            audio.samples.clear(); // No decoding implemented
            return std::expected<std::variant<AudioData, MidiData>, AudioReadError>{std::in_place, std::move(audio)};
        } else {
            // Skip block
            file.seekg(blockLength, std::ios::cur);
        }
    }

    return std::unexpected(AudioReadError::UnsupportedFormat);
}

} // namespace audio
