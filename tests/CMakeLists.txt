add_executable(Tests
    test_midi_reader.cpp
)

target_link_libraries(Tests PRIVATE AudioReaderLib)

add_test(NAME MidiReaderTest COMMAND Tests)
