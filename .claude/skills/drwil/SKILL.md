---
name: drwil
description: Entry point to discover the kit's governance capabilities. Without an argument, shows a menu of capabilities and their dedicated command. With an argument (verify, adopt, audit, value, usage, progress, release), goes straight to the matching capability.
argument-hint: "[verify|adopt|audit|value|usage|progress|release]"
arguments: capability
disable-model-invocation: true
---

Requested capability: `$capability`.

If `$capability` is empty, show this menu and stop there (don't run
anything else):

| Command | Capability | Recipe |
| --- | --- | --- |
| `/drwil verify` | Is my work verified? (agent/human workflow below) | `docs/ia-first.md`, section 5 |
| `/drwil-adopt` | Adopt the kit on an existing project | `docs/recipes/adopt-the-kit.md` |
| `/drwil-audit` | Audit risks and debt | `docs/recipes/audit-risks-and-debt.md` |
| `/drwil-value` | Discover product value | `docs/recipes/discover-product-value.md` |
| `/drwil-usage` | Track consumption per increment | `docs/recipes/track-consumption-per-increment.md` |
| `/drwil-progress` | View progress | `docs/recipes/view-progress.md` |
| `/drwil-release` | Create a release (tag + GitHub note), optional module | `docs/recipes/create-a-release.md` |
| `/drwil-start` | Start a workstream: poll of the scoped briefs, the human answer moves to BUILD | `docs/ia-first.md` ("Starting and closing") |

If `$capability` is `verify`: run `npx drwil verify --agent` at the project
root (the project's own `drwil` CLI is the source of truth; never derive a
verdict yourself) and follow its "Next step" line:

- `GOVERNANCE: PASS`: the work is verified; continue or finish.
- `GOVERNANCE: FAIL`: fix the failing contracts, then run `/drwil verify`
  again.
- `GOVERNANCE: VERIFY ERROR`: report the verification problem; never claim
  the work is verified.
- `GOVERNANCE: MANUAL REVIEW REQUIRED`: stop claiming validation; relay
  "Human attestation required.", the listed contracts and the command
  `drwil attest <CONTRACT_ID>` to the human, as an action **for them**.

Never run `drwil attest`, never write or edit anything in the attestations
folder (.drwil/evidence/attestations), never present a human attestation as
done by the agent: an agent never attests its own work.

If `$capability` matches one of `adopt`, `audit`, `value`, `usage`,
`progress`, `release`, apply the matching recipe from the table above directly
(don't rewrite it here, read it and follow it). Otherwise, say the
capability isn't recognized and show the menu again.
