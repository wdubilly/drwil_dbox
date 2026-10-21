# Intents

An **intent** is a stated need, not yet scoped, estimated or prioritised:
it is not a commitment. Each brief states the need as it was expressed,
what already exists in the code (as read on the brief's date) and the
questions to decide before writing anything.

Lifecycle: a scoped intent becomes a workstream (`docs/projects/`);
abandoned, its brief is deleted, with the reason in the commit message.
A need that is already clear and doable as is does not go through here: it
goes straight into the workstream index. Shape of a brief (checked,
QUA-015): a dated (YYYY-MM-DD) « Status » line at the top, sections
« Need », « Current state » (read from the code, dated) and « Open
questions ». An intent re-read later and found partly outdated gets a
dated line under its Status, `**Reviewed on YYYY-MM-DD**: what became
false, what is still alive`; an intent with no dated review for 30 days is
reported by the checks. Closing a workstream updates the intent it comes
from (the brief's `**Intent**` field), or summarises it in the journal and
removes it when fully covered.

| Brief | In one line |
|---|---|
| [drum-box](drum-box.md) | Retro 70's funk drum machine, plugin + standalone (JUCE) |
| [song-save-export](song-save-export.md) | Save and load songs, export to WAV |
| [sound-presets-effects](sound-presets-effects.md) | Synth and bass presets, a few effects |
| [pattern-editing](pattern-editing.md) | Pattern copy, piano roll, predictable chain edits |

A one-line idea, too light for a brief, is noted with one command:
`node .githooks/note.mjs "<text>"` appends a dated line to
`docs/intentions/notes.md` (if present) (outside the workstream index), at any moment and
from any tool, without changing the activity; a note is always an intent,
never an order to code. It is promoted to an intent brief when it matures,
and its line is then removed.

## Brief template

```
# Intent: short title

**Status**: proposed on YYYY-MM-DD.

## Need

(the need as expressed, without rewording that decides in the requester's place)

## Current state

(read from the code on the date above; « not checked » if not read)

## Open questions

- [decision] ...
```
