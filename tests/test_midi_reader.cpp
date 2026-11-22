#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <variant>
#include <filesystem>

#include "AudioReader.h"   // for readMidi
#include "MidiReader.h"    // for parseMidiData, convertDeltaToAbsoluteTimes, printMidiDebug

namespace fs = std::filesystem;

std::string noteName(uint8_t noteNumber) {
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (noteNumber / 12) - 1;
    int note = noteNumber % 12;
    return std::string(names[note]) + std::to_string(octave);
}

bool testMidiFile(const fs::path& path, bool verbose = false) {
    std::cout << "Testing: " << path.filename().string() << "\n";

    auto result = audio::readMidi(path.string());
    if (!result) {
        std::cerr << "  Failed to read MIDI file: error code " << static_cast<int>(result.error()) << "\n";
        return false;
    }

    audio::ReadSuccess& data = *result;
    if (auto* midiData = std::get_if<audio::MidiData>(&data)) {
        bool pass = true;

        // Basic checks
        if (midiData->ticksPerQuarterNote != 480) {
            std::cerr << "  Unexpected ticks per quarter note: " << midiData->ticksPerQuarterNote << "\n";
            pass = false;
        }
        if (midiData->events.size() < 10) {
            std::cerr << "  Too few MIDI events: " << midiData->events.size() << "\n";
            pass = false;
        }

        // Print first 10 note events
        int printed = 0;
        for (const auto& ev : midiData->events) {
            if (ev.data.empty()) continue;
            uint8_t status = ev.data[0];
            uint8_t eventType = status & 0xF0;
            if (eventType == 0x90 || eventType == 0x80) {
                uint8_t note = ev.data.size() > 1 ? ev.data[1] : 0;
                uint8_t velocity = ev.data.size() > 2 ? ev.data[2] : 0;
                std::cout << "    " << (eventType == 0x90 ? "Note On " : "Note Off")
                          << " delta=" << ev.deltaTime
                          << " note=" << noteName(note) << " (" << (int)note << ")"
                          << " velocity=" << (int)velocity << "\n";
                if (++printed >= 10) break;
            }
        }

        // Last event check
        if (!midiData->events.empty()) {
            const auto& ev = midiData->events.back();
            if (verbose) {
                std::cout << "    Last event delta=" << ev.deltaTime << " data=[ ";
                for (auto b : ev.data)
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
                std::cout << std::dec << "]\n";
            }
            if (!(ev.data.size() >= 2 && ev.data[0] == 0xFF && ev.data[1] == 0x2F)) {
                std::cerr << "  Warning: Last event is not End of Track meta event\n";
                pass = false;
            }
        }

        // Extra debug if requested
        if (verbose) {
            audio::MidiData midiCopy = *midiData;
            audio::convertDeltaToAbsoluteTimes(midiCopy);
            audio::printMidiDebug(midiCopy);
        }

        if (pass) {
            std::cout << "  ✅ Passed\n";
        } else {
            std::cout << "  ❌ Failed\n";
        }
        return pass;
    } else {
        std::cerr << "  File does not contain MIDI data\n";
        return false;
    }
}

int main(int argc, char* argv[]) {
    fs::path folder = "testaudiofiles";
    if (argc > 1) {
        folder = argv[1]; // Allow custom folder
    }

    bool allPass = true;
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".mid") continue;

        // Optional pattern filter
        auto fname = entry.path().filename().string();
        if (fname.find("all_major_minor_scales") != 0) continue;

        bool pass = testMidiFile(entry.path(), /*verbose=*/false);
        allPass = allPass && pass;
    }

    return allPass ? 0 : 1;
}
