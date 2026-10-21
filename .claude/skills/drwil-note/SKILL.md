---
name: drwil-note
description: Note an idea or an intent at any moment, even in the middle of a workstream, without framing or coding it — one dated line in the notes file of the intents. Use when the human says "note that", "idea:", "intent:" or runs /drwil-note.
argument-hint: "<text of the note>"
arguments: text
---

# Note an idea (`/drwil-note`)

A note is **always an intent, never an order to code**: the agent records it, nothing else.

1. Run `node .githooks/note.mjs "$text"` (the human's words, kept as they are, on one line, quotes
   escaped). It appends one dated line to `docs/intentions/notes.md` (if present); it changes no activity,
   touches neither the state nor the active brief, and commits nothing.
2. Say in one sentence that the note is recorded. Do not code it, do not frame it, do not open a
   brief, do not move the activity; the workstream in progress goes on.
3. The notes file goes out with the branch in progress, in the next commit (commit only if the
   human asks); it is not listed in the index of workstreams.

Any tool can do the same from a terminal: `node .githooks/note.mjs "<text>"`.
