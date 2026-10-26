# Guarantee: test coverage of layer `.`

**Status**: opened on 2026-10-26 — blocking guarantee, written by `drwil guarantees`.
**Risk**: HIGH
**Maturity**: to refine
**Guarantee**: blocking

<!-- scope
files:
  - .drwil/ia-first.json
-->

## 1. Need

drwil guarantees 100 % test coverage (lines, branches, functions, statements, as far as the tool measures them) on every layer. This layer has no check yet:

- stack not detected: coverage tool unknown

## Acceptance criteria

- The layer's coverage check is declared, cited by the project's coverage contract and green — contract to create

## 4. Decisions

- Mandatory guarantee, no waiver: a check that did not run has not passed (`QUA-013`), no check is bypassed (`QUA-019`).

## Points to decide

- [AI] Decided by the requester on 2026-10-26: gcov (gcovr to install) on the engine `drumbox_engine`, threshold at the measured level as a ratchet, 100 % on new code; the JUCE UI (`MainComponent`, `SequenceView`, `DetailPanel`, `Widgets`, `Theme`, `*LookAndFeel`, `Main`) excluded in `guarantees.exclusions` with its reason (no display in CI to test it).

## 5. Increments

- **Increment 1 — resolve the guarantee**: see the need above, then run `npx drwil guarantees` again.

## 6. Hand-off

- **Last state**: written by `drwil guarantees`; as long as it is not resolved, `drwil verify` is FAIL and `/drwil-start` offers only it.
