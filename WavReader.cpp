#include "WavReader.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>

namespace audio {

#pragma pack(push,1)
struct RiffHeader {
    char chunkId[4]; // "RIFF"
    uint32_t chunkSize;
    char format[4];  // "WAVE"
};

struct ChunkHeader {
    char id[4];
    uint32_t size;
};

struct FmtChunk {
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
#pragma pack(pop)

std::expected<std::variant<AudioData, MidiData>, AudioReadError> readWav(std::string_view filepath) {
    std::ifstream file(std::string(filepath), std::ios::binary);
    if (!file.is_open()) return std::unexpected(AudioReadError::FileNotFound);

    RiffHeader riff{};
    file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
    if (!file || std::memcmp(riff.chunkId, "RIFF", 4) != 0 || std::memcmp(riff.format, "WAVE", 4) != 0)
        return std::unexpected(AudioReadError::InvalidData);

    FmtChunk fmt{};
    bool fmtFound = false;
    bool dataFound = false;
    uint32_t dataSize = 0;
    std::vector<uint8_t> rawData;

    // Search chunks until 'fmt ' and 'data' found
    while (!dataFound && file) {
        ChunkHeader chunk{};
        file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));
        if (!file) break;

        uint32_t chunkSize = chunk.size;

        if (std::memcmp(chunk.id, "fmt ", 4) == 0) {
            if (chunkSize < sizeof(FmtChunk)) return std::unexpected(AudioReadError::InvalidData);
            file.read(reinterpret_cast<char*>(&fmt), sizeof(FmtChunk));
            // Skip any extra bytes if chunkSize > sizeof(FmtChunk)
            if (chunkSize > sizeof(FmtChunk)) {
                file.seekg(chunkSize - sizeof(FmtChunk), std::ios::cur);
            }
            fmtFound = true;
        } else if (std::memcmp(chunk.id, "data", 4) == 0) {
            dataSize = chunkSize;
            rawData.resize(dataSize);
            file.read(reinterpret_cast<char*>(rawData.data()), dataSize);
            dataFound = true;
        } else {
            // skip unknown chunk
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (!fmtFound || !dataFound)
        return std::unexpected(AudioReadError::InvalidData);

    if (fmt.audioFormat != 1)
        return std::unexpected(AudioReadError::UnsupportedFormat); // Only PCM supported

    if (fmt.bitsPerSample != 16 && fmt.bitsPerSample != 8)
        return std::unexpected(AudioReadError::UnsupportedFormat); // Support 8 or 16 bit only

    AudioData audio;
    audio.sampleRate = fmt.sampleRate;
    audio.channels = fmt.numChannels;

    size_t sampleCount = dataSize / (fmt.bitsPerSample / 8);
    size_t frameCount = sampleCount / fmt.numChannels;

    audio.samples.resize(sampleCount);

    // Convert samples to float [-1,1]
    if (fmt.bitsPerSample == 16) {
        const int16_t* src = reinterpret_cast<const int16_t*>(rawData.data());
        for (size_t i = 0; i < sampleCount; ++i) {
            audio.samples[i] = src[i] / 32768.0f;
        }
    } else if (fmt.bitsPerSample == 8) {
        // 8-bit unsigned PCM (0..255), convert to [-1,1]
        const uint8_t* src = rawData.data();
        for (size_t i = 0; i < sampleCount; ++i) {
            audio.samples[i] = (src[i] - 128) / 128.0f;
        }
    }

    return std::expected<std::variant<AudioData, MidiData>, AudioReadError>{std::in_place, std::move(audio)};
}

} // namespace audio
