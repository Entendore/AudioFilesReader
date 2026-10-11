#pragma once
#include <string>
#include <vector>
#include <optional>
#include <span>
#include "AudioReader.h"

namespace audio {

// Read MIDI file, return AudioReadResult containing MidiData or error
std::expected<std::variant<AudioData, MidiData>, AudioReadError> readMidi(std::string_view filepath);

// Parse MIDI data from a memory buffer (raw .mid file data)
// Returns MidiData or error
std::expected<std::variant<AudioData, MidiData>, AudioReadError> parseMidiData(std::span<const uint8_t> data);

// Convert delta times in MidiData events to absolute times (ticks), per track
void convertDeltaToAbsoluteTimes(MidiData& midi);

// Extract the tempo (microseconds per quarter note) from MIDI meta events
// Returns the first tempo found or std::nullopt if none found
std::optional<uint32_t> getTempoMicrosecondsPerQuarterNote(const MidiData& midi);

// Extract the number of tracks in the MidiData
size_t getTrackCount(const MidiData& midi);

// Simple debug print of MIDI events to stdout (for development)
void printMidiDebug(const MidiData& midi);

} // namespace audio
