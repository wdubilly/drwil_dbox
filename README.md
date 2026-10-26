# drwil_dbox

Drum Box is a standalone retro drum machine for musicians: a 16-step sequencer with twelve
synthesised tracks (eight drums, bass, a four-voice synth, a pluck, FX), 16 patterns chained into
a song, a mixer with five effects, song files and WAV export. Dark flat look by default, 70's
funk skin as an option.

> **Code assistants**: start with `AGENTS.md` (conduct, contracts, context
> loading).

## At a glance

```
UI (JUCE: MainComponent, SequenceView, DetailPanel)
   |  edits the song, reads the playback state
   v
Engine (pure C++, drumbox_engine): Song -> Sequencer -> voices -> mixer -> effects -> audio / WAV
```

## Getting started

1. Set up a workstation: `INSTALL.md` (if present).
2. Run and check: `docs/recipes/run-locally.md` (if present).
3. Enable the verification hooks, once per clone:
   `git config core.hooksPath .githooks`.

## Documentation

| Topic | Document |
|---|---|
| What the app does, screen by screen | `docs/features.md` (if present) |
| Code structure, repo map | `docs/architecture.md` |
| Verified rules (contracts) | `docs/contracts.md` |
| Security, authentication and roles | `docs/security.md` (if present) |
| Production deployment | `docs/deployment.md` (if present) |
| Procedures (run, deploy, add a route, a screen…) | `docs/recipes/` |
| Style guide | `docs/style-guide.md` (if present) |
| Pending projects | `docs/projects/pending.md` |
| See where the project stands | `docs/recipes/view-progress.md` |
| Audit risks and technical debt | `docs/recipes/audit-risks-and-debt.md` |
| Track a project's cost | `docs/recipes/track-consumption-per-increment.md` |
| Explore product value | `docs/recipes/discover-product-value.md` |
| Work with branches and merge/pull requests | `docs/recipes/working-with-branches.md` |
