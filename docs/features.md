# Features

What Drum Box does, from the point of view of the musician. The code map is in
`docs/architecture.md`; each behaviour below is a `DBX-` contract in `docs/contracts.md`.

## Transport and sequencer

Play and stop, tempo, swing, 16 steps per pattern (off, normal, accent), a moving playhead.

## Tracks

Twelve tracks: eight drums, a bass, a four-voice synth (PAD), a four-voice PLUCK and an FX track
(six sounds). Each has mute, solo, volume, tune (±24 semitones) and detune (±100 cents), an effect
send and presets (factory and user). Bass, pad and pluck notes go in step by step or in a piano roll.

## Patterns and chain

16 patterns, copy, paste, clear and swap; a chain of 1 to 128 positions plays in order then loops;
a new chain length applies at once while playing.

## Effects

Reverb, delay, distortion, chorus and compressor, plus a master chain.

## Songs and export

A song is saved to and opened from a `.dbox` JSON file (a damaged file is refused and changes
nothing). WAV export: stereo 24-bit 44.1 kHz mix with effect tails, or the mix plus one file per
track. A demo song loads at first launch. Undo and redo cover the whole song.

## Interface

Resizable window (size remembered), Sequence view of the twelve tracks, detail panel of the
selected track, dark flat skin by default and a 70's funk skin as an option.

## Verified in real conditions

Engine behaviour is verified by the automated tests of the `DBX-` contracts. Real use of the
standalone app on Linux, Windows and macOS is not verified here.
