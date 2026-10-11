#include "MidiReader.h"
#include <fstream>
#include <iostream>
#include <cstring>

namespace audio {

// --- Helper functions for parsing ---

static uint32_t readBE32(std::ifstream& f) {
    uint8_t bytes[4];
    f.read(reinterpret_cast<char*>(bytes), 4);
    return (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) | (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
}

static uint16_t readBE16(std::ifstream& f) {
    uint8_t bytes[2];
    f.read(reinterpret_cast<char*>(bytes), 2);
    return (uint16_t(bytes[0]) << 8) | uint16_t(bytes[1]);
}

// --- VarLen integer reading from stream ---
static std::optional<uint32_t> readVarLen(std::istream& stream) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t byte;
        if (!stream.read(reinterpret_cast<char*>(&byte), 1)) return std::nullopt;
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) break;
    }
    return value;
}

// --- Parsing MIDI from memory buffer ---

class BufferReader {
    const uint8_t* data;
    size_t size;
    size_t pos = 0;

public:
    BufferReader(std::span<const uint8_t> buffer) : data(buffer.data()), size(buffer.size()) {}

    bool read(uint8_t* out, size_t count) {
        if (pos + count > size) return false;
        memcpy(out, data + pos, count);
        pos += count;
        return true;
    }

    bool readByte(uint8_t& out) {
        if (pos >= size) return false;
        out = data[pos++];
        return true;
    }

    std::optional<uint32_t> readVarLen() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            uint8_t byte;
            if (!readByte(byte)) return std::nullopt;
            value = (value << 7) | (byte & 0x7F);
            if ((byte & 0x80) == 0) break;
        }
        return value;
    }

    size_t tell() const { return pos; }
    void seek(size_t p) { pos = p <= size ? p : size; }
    size_t remaining() const { return size - pos; }
};

std::expected<std::variant<AudioData, MidiData>, AudioReadError>  parseMidiData(std::span<const uint8_t> data) {
    BufferReader br(data);

    char header[4];
    if (!br.read(reinterpret_cast<uint8_t*>(header), 4) || std::string_view(header, 4) != "MThd")
        return std::unexpected(AudioReadError::InvalidData);

    uint8_t sizeBuf[4];
    if (!br.read(sizeBuf, 4)) return std::unexpected(AudioReadError::InvalidData);
    uint32_t headerSize = (uint32_t(sizeBuf[0]) << 24) | (uint32_t(sizeBuf[1]) << 16) | (uint32_t(sizeBuf[2]) << 8) | sizeBuf[3];
    if (headerSize != 6) return std::unexpected(AudioReadError::InvalidData);

    uint8_t buf16[2];

    if (!br.read(buf16, 2)) return std::unexpected(AudioReadError::InvalidData);
    uint16_t formatType = (uint16_t(buf16[0]) << 8) | buf16[1];

    if (!br.read(buf16, 2)) return std::unexpected(AudioReadError::InvalidData);
    uint16_t trackCount = (uint16_t(buf16[0]) << 8) | buf16[1];

    if (!br.read(buf16, 2)) return std::unexpected(AudioReadError::InvalidData);
    uint16_t division = (uint16_t(buf16[0]) << 8) | buf16[1];

    if (division & 0x8000) return std::unexpected(AudioReadError::UnsupportedFormat);

    MidiData midiData;
    midiData.ticksPerQuarterNote = division;

    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
        if (!br.read(reinterpret_cast<uint8_t*>(header), 4) || std::string_view(header, 4) != "MTrk")
            return std::unexpected(AudioReadError::InvalidData);

        if (!br.read(sizeBuf, 4)) return std::unexpected(AudioReadError::InvalidData);
        uint32_t trackSize = (uint32_t(sizeBuf[0]) << 24) | (uint32_t(sizeBuf[1]) << 16) | (uint32_t(sizeBuf[2]) << 8) | sizeBuf[3];

        size_t trackEnd = br.tell() + trackSize;
        uint8_t runningStatus = 0;

        std::vector<MidiEvent> trackEvents;

        while (br.tell() < trackEnd) {
            auto deltaTimeOpt = br.readVarLen();
            if (!deltaTimeOpt) return std::unexpected(AudioReadError::DecodeError);
            uint32_t deltaTime = *deltaTimeOpt;

            uint8_t statusByte;
            if (!br.readByte(statusByte)) return std::unexpected(AudioReadError::DecodeError);

            if (statusByte < 0x80) {
                // running status
                if (runningStatus == 0) return std::unexpected(AudioReadError::DecodeError);
                br.seek(br.tell() - 1);
                statusByte = runningStatus;
            } else {
                runningStatus = statusByte;
            }

            std::vector<uint8_t> eventData;
            eventData.push_back(statusByte);

            if (statusByte == 0xFF) {
                // Meta event
                uint8_t metaType;
                if (!br.readByte(metaType)) return std::unexpected(AudioReadError::DecodeError);
                eventData.push_back(metaType);

                auto lengthOpt = br.readVarLen();
                if (!lengthOpt) return std::unexpected(AudioReadError::DecodeError);
                uint32_t length = *lengthOpt;

                for (uint32_t i = 0; i < length; ++i) {
                    uint8_t b;
                    if (!br.readByte(b)) return std::unexpected(AudioReadError::DecodeError);
                    eventData.push_back(b);
                }
            } else if ((statusByte & 0xF0) == 0xC0 || (statusByte & 0xF0) == 0xD0) {
                // Program change or channel pressure - 1 data byte
                uint8_t dataByte;
                if (!br.readByte(dataByte)) return std::unexpected(AudioReadError::DecodeError);
                eventData.push_back(dataByte);
            } else {
                // All other MIDI events with 2 data bytes
                uint8_t dataBytes[2];
                if (!br.read(dataBytes, 2)) return std::unexpected(AudioReadError::DecodeError);
                eventData.push_back(dataBytes[0]);
                eventData.push_back(dataBytes[1]);
            }

            MidiEvent event;
            event.deltaTime = deltaTime;
            event.data = std::move(eventData);
            trackEvents.push_back(std::move(event));
        }

        midiData.tracks.push_back(std::move(trackEvents));
    }

    return std::expected<std::variant<AudioData, MidiData>, AudioReadError>{std::in_place, std::move(midiData)};
}

std::expected<std::variant<AudioData, MidiData>, AudioReadError>  readMidi(std::string_view filepath) {
    std::ifstream file(std::string(filepath), std::ios::binary);
    if (!file.is_open()) return std::unexpected(AudioReadError::FileNotFound);

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

    return parseMidiData(buffer);
}

void convertDeltaToAbsoluteTimes(MidiData& midi) {
    for (auto& track : midi.tracks) {
        uint32_t absTime = 0;
        for (auto& event : track) {
            absTime += event.deltaTime;
            event.deltaTime = absTime;
        }
    }
}

std::optional<uint32_t> getTempoMicrosecondsPerQuarterNote(const MidiData& midi) {
    for (const auto& track : midi.tracks) {
        for (const auto& event : track) {
            if (event.data.size() >= 5 && event.data[0] == 0xFF && event.data[1] == 0x51) {
                // Tempo meta event, next 3 bytes = tempo (microseconds per quarter note)
                uint32_t tempo = (uint32_t(event.data[2]) << 16) | (uint32_t(event.data[3]) << 8) | uint32_t(event.data[4]);
                return tempo;
            }
        }
    }
    return std::nullopt;
}

size_t getTrackCount(const MidiData& midi) {
    return midi.tracks.size();
}

void printMidiDebug(const MidiData& midi) {
    std::cout << "Ticks per quarter note: " << midi.ticksPerQuarterNote << "\n";
    for (size_t trackIndex = 0; trackIndex < midi.tracks.size(); ++trackIndex) {
        const auto& track = midi.tracks[trackIndex];
        std::cout << "Track " << trackIndex << " events:\n";
        for (size_t i = 0; i < track.size(); ++i) {
            const auto& e = track[i];
            std::cout << "  Event #" << i << " Time: " << e.deltaTime << " Data:";
            for (auto b : e.data) {
                std::cout << " " << std::hex << int(b);
            }
            std::cout << std::dec << "\n";
        }
    }
}

} // namespace audio
