from mido import Message, MidiFile, MidiTrack, bpm2tempo

# MIDI note numbers for C major scale as base reference
# We'll build all scales by transposing these base scales

# Note numbers for C major scale (C D E F G A B)
C_MAJOR = [60, 62, 64, 65, 67, 69, 71]
# Note numbers for C natural minor scale (C D Eb F G Ab Bb)
C_MINOR = [60, 62, 63, 65, 67, 68, 70]

# Scale intervals in semitones for major and minor scales
MAJOR_INTERVALS = [2, 2, 1, 2, 2, 2, 1]
MINOR_INTERVALS = [2, 1, 2, 2, 1, 2, 2]

# Names of all 12 keys with sharps for simplicity
KEYS_SHARP = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

def build_scale(root_note, intervals):
    """Build scale starting at root_note applying intervals."""
    scale = [root_note]
    current_note = root_note
    for interval in intervals[:-1]:  # 7 notes total, 6 intervals
        current_note += interval
        scale.append(current_note)
    return scale

def add_scale_to_track(track, scale, velocity=64, duration=480):
    """Add ascending and descending notes of the scale to the track."""
    # Ascending
    for note in scale:
        track.append(Message('note_on', note=note, velocity=velocity, time=0))
        track.append(Message('note_off', note=note, velocity=velocity, time=duration))
    # Descending
    for note in reversed(scale[:-1]):  # exclude last note to avoid duplicate
        track.append(Message('note_on', note=note, velocity=velocity, time=0))
        track.append(Message('note_off', note=note, velocity=velocity, time=duration))

def main():
    mid = MidiFile()
    track = MidiTrack()
    mid.tracks.append(track)

    # Set tempo (120 bpm)
    tempo = bpm2tempo(120)
    track.append(Message('program_change', program=0, time=0))

    # We'll start each scale 1 measure apart (assuming 480 ticks per beat, 4 beats per measure)
    # So after each scale, insert a pause (time delta)
    pause_duration = 480 * 4  # one measure

    # MIDI note for C4 = 60, keys range from C to B (12 semitones)
    root_C = 60

    time_elapsed = 0

    for i, key in enumerate(KEYS_SHARP):
        root_note = root_C + i

        # Add major scale
        major_scale = build_scale(root_note, MAJOR_INTERVALS)
        add_scale_to_track(track, major_scale)
        # Add pause
        track.append(Message('note_off', note=0, velocity=0, time=pause_duration))

        # Add minor scale
        minor_scale = build_scale(root_note, MINOR_INTERVALS)
        add_scale_to_track(track, minor_scale)
        # Add pause
        track.append(Message('note_off', note=0, velocity=0, time=pause_duration))

    track.append(Message('note_off', note=0, velocity=0, time=pause_duration))
    mid.save('all_major_minor_scales.mid')

if __name__ == '__main__':
    main()
