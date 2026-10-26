# Guarantee: test coverage of layer `.`

**Status**: opened on 2026-10-26 — blocking guarantee, written by `drwil guarantees`.
**Risk**: HIGH
**Maturity**: to refine
**Guarantee**: blocking

<!-- scope
files:
  - .drwil/ia-first.json
  - docs/contracts.md
  - .github/workflows/ia-first.yml
  - docs/projects/pending.md
  - docs/projects/journal.md
-->

## 1. Need

drwil guarantees 100 % test coverage (lines, branches, functions, statements, as far as the tool measures them) on every layer. This layer has no check yet:

- stack not detected: coverage tool unknown

## Acceptance criteria

- The layer's coverage check is declared, cited by the project's coverage contract and green, proven by `QUA-004`

## 4. Decisions

- Mandatory guarantee, no waiver: a check that did not run has not passed (`QUA-013`), no check is bypassed (`QUA-019`).

## Points to decide

- [AI] Decided by the requester on 2026-10-26: gcov (gcovr to install) on the engine `drumbox_engine`, threshold at the measured level as a ratchet, 100 % on new code; the JUCE UI (`MainComponent`, `SequenceView`, `DetailPanel`, `Widgets`, `Theme`, `*LookAndFeel`, `Main`) excluded in `guarantees.exclusions` with its reason (no display in CI to test it).

## 5. Increments

- **Increment 1 — resolve the guarantee**: see the need above, then run `npx drwil guarantees` again.

## 6. Hand-off

- **Last state** (2026-10-27): checks `coverage-root` (ratchet lines 93 / branches 60 / functions 99) and `coverage-diff-root` declared, contract QUA-004 written, CI installs gcovr and diff-cover; JUCE UI excluded in `guarantees.exclusions`.
- **Next step**: closing (journal, `node .githooks/close.mjs`, PR). Limit: `drwil guarantees` itself still reports « stack not detected » (C++ is outside its tools table); `drwil verify` judges the declared checks.
