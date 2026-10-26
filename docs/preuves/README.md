# Closing proofs

One file per closed workstream, written by the closing
(`node .githooks/close.mjs`) from the `drwil verify --evidence` evidence it
requires: what the workstream had got accepted, and what proves it.

Each file keeps, for one workstream: the closing date, the verified commit
and its verdict, then one row per acceptance criterion — the criterion, the
contract that proves it, the checks that contract ran, and the fingerprint of
the files it covers. Its form is described in `docs/ia-first.md`
("Closing proof").

These files are **documentation, versioned with the code**, unlike the run
evidence of `.drwil/evidence/` (if present), which git ignores. `drwil verify` replays
their criteria after the closing and fails, naming them, when a proof has
disappeared: never delete a row to make the verdict green — restore the
proof, or retire the criterion on purpose, in a workstream that says so.

Folder of this project: `docs/preuves` by default, `dirs.proofs` setting of
`.drwil/ia-first.json`.
