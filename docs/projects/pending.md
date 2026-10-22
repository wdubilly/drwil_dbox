# Pending work — index

One line per topic. Detail and **status** live in the linked brief, the only
source: this index copies no state. A topic without a link fits in two
lines here. Unscoped ideas: `docs/intentions/README.md`. Writing rules:
`docs/ia-first.md`, section 7.

Mandatory marker on every open item (QUA-015): `[AI]` doable by an agent in
the repo; `[human]` a gesture outside the repo (server, vault, approval,
infrastructure); `[decision]` to be settled by the requester before any
work. An agent does not attempt a `[human]` gesture and does not decide a
`[decision]`: it raises it. Small tasks that don't justify a project:
`docs/projects/routine-maintenance.md`.

Optional priority, not checked by check-docs.mjs (plain convention): `[P0]`
most urgent, `[P1]` high, `[P2]` normal, `[P3]` least urgent. Without a tag,
a topic is considered `[P2]`.

A project whose brief is **entirely** finished (not just one point among
others) is condensed into `docs/projects/journal.md` then removed from
this index, not checked: see
`docs/recipes/working-with-branches.md`, "Closing" section. Remaining
checked lines are already-done points of a brief that stays open
otherwise.

## Projects

- [ ] [decision] [P2] Drum box, follow-ups of the interface redesign: `docs/intentions/ux-followups.md`

<!-- Example:
- [ ] [AI] short title of the topic, link to a brief if more than two lines
  are needed, otherwise the topic fits here.
- [x] topic delivered (YYYY-MM-DD).
-->
