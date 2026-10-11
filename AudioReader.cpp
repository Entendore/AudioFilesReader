#include "AudioReader.h"
#include "WavReader.h"
#include "Mp3Reader.h"
#include "FlacReader.h"
#include "OggReader.h"
#include "MidiReader.h"

#include <string_view>
#include <algorithm>
#include <fstream>
#include <cstring>

namespace audio {

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::optional<audio::AudioFormat> detectAudioFormat(std::string_view filepath) {
    // --- First: try extension detection ---
    auto extPos = filepath.rfind('.');
    if (extPos != std::string_view::npos) {
        std::string_view ext = filepath.substr(extPos + 1);
        std::string extLower;
        extLower.reserve(ext.size());
        std::transform(ext.begin(), ext.end(), std::back_inserter(extLower),
                       [](char c) { return char(std::tolower(c)); });

        if (iequals(ext, "wav"))  return AudioFormat::WAV;
        if (iequals(ext, "mp3"))  return AudioFormat::MP3;
        if (iequals(ext, "flac")) return AudioFormat::FLAC;
        if (iequals(ext, "ogg"))  return AudioFormat::OGG;
        if (iequals(ext, "mid") || iequals(ext, "midi")) return AudioFormat::MIDI;
    }

    // --- Second: check magic numbers in the file header ---
    std::ifstream file(std::string(filepath), std::ios::binary);
    if (!file) return std::nullopt;  // Could not open file => no format detected

    std::array<char, 12> header{};
    file.read(header.data(), header.size());
    size_t bytesRead = static_cast<size_t>(file.gcount());
    if (bytesRead < 4) return std::nullopt;

    // WAV: "RIFF" .... "WAVE"
    if (bytesRead >= 12 &&
        memcmp(header.data(), "RIFF", 4) == 0 &&
        memcmp(header.data() + 8, "WAVE", 4) == 0) {
        return AudioFormat::WAV;
    }

    // MP3: ID3 tag or MPEG frame sync
    if (bytesRead >= 3 && memcmp(header.data(), "ID3", 3) == 0) {
        return AudioFormat::MP3;
    }
    if (bytesRead >= 2 && 
        (static_cast<uint8_t>(header[0]) == 0xFF) &&
        ((static_cast<uint8_t>(header[1]) & 0xE0) == 0xE0)) {
        return AudioFormat::MP3;
    }

    // FLAC: "fLaC"
    if (bytesRead >= 4 && memcmp(header.data(), "fLaC", 4) == 0) {
        return AudioFormat::FLAC;
    }

    // OGG: "OggS"
    if (bytesRead >= 4 && memcmp(header.data(), "OggS", 4) == 0) {
        return AudioFormat::OGG;
    }

    // MIDI: "MThd"
    if (bytesRead >= 4 && memcmp(header.data(), "MThd", 4) == 0) {
        return AudioFormat::MIDI;
    }

    return std::nullopt;  // Unknown format
}


std::expected<std::variant<AudioData, MidiData>, AudioReadError> readAudioFile(std::string_view filepath) {
    auto formatOpt = detectAudioFormat(filepath);
    if (!formatOpt) {
        return std::unexpected(AudioReadError::FileNotFound);
    }
    AudioFormat format = *formatOpt;

    switch (format) {
       
    case AudioFormat::MIDI: {
        auto result = readMidi(filepath);
        if (!result) return std::unexpected(result.error());
        return std::variant<AudioData, MidiData>(std::move(result.value()));
    }
        // Uncomment once implemented
        /*
        case AudioFormat::WAV: {
            auto result = readWav(filepath);
            if (!result) return std::unexpected(result.error());
            return std::variant<AudioData, MidiData>(std::move(result.value()));
        }
        case AudioFormat::MP3: {
            auto result = readMp3(filepath);
            if (!result) return std::unexpected(result.error());
            return std::variant<AudioData, MidiData>(std::move(result.value()));
        }
        case AudioFormat::FLAC: {
            auto result = readFlac(filepath);
            if (!result) return std::unexpected(result.error());
            return std::variant<AudioData, MidiData>(std::move(result.value()));
        }
        case AudioFormat::OGG: {
            auto result = readOgg(filepath);
            if (!result) return std::unexpected(result.error());
            return std::variant<AudioData, MidiData>(std::move(result.value()));
        }
        */
        default:
            return std::unexpected(AudioReadError::UnsupportedFormat);
    }
}

} // namespace audio
