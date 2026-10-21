# Closed projects journal

Condensed summary of each `docs/projects/` brief once its Status moves to
"done" (the whole brief, not just one point among several of a brief that
otherwise stays open). The detailed brief is then deleted: no more full
history in plain text in the repository, but nothing is lost —
`git log --follow -- docs/projects/<brief>.md` (or the closing commit/PR
cited below) retrieves the full text at any time.

Goal: keep `docs/projects/` light (active briefs only) without sacrificing
traceability, which stays in git anyway. See the recipe
`docs/recipes/working-with-branches.md`, "Closing" section.

One entry per closed project, most recent last.

## Drum box, first slice (standalone, 8 synthesised tracks) — closed 2026-10-21

Delivered a JUCE/C++ standalone drum machine with a 70's funk look: 16-step sequencer with tempo, swing and accents (`DBX-001` to `DBX-003`), eight synthesised voices (`DBX-004`), standalone app that builds and whose transport logic is tested (`DBX-005`), look and ease of use attested by the human (`DBX-006`). GitHub CI runs the repository checks on Linux and builds the Windows `.exe` as an artifact. Engine kept free of JUCE so it is tested without an audio device. Left out: plugin formats, samples, pattern saving, export, separate outputs, macOS CI (see `docs/intentions/drum-box.md`). Limits: the Linux app build needs X11, ALSA and FreeType development packages; the example contract `APP-001` is still to replace. Full text: the git history of the drum-box brief (`git log --follow`).
