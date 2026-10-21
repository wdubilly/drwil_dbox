#!/usr/bin/env node
// Git adapter of the check engine (.githooks/engine.mjs), run at commit time (pre-commit),
// at push time (pre-push) and in CI. It implements no check: it runs them through the engine
// shared with `drwil verify` and lays out the output for a human who commits.
// QUA-013: a check that did not run is not a passed check.
// Each check therefore ends "failed", "OK" or "not run", never hidden.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { intentsLeftBehind, staleIntents } from "./briefs.mjs";
import { checks, projectRoot } from "./engine.mjs";
import { annotate, glossary } from "./glossary.mjs";

const root = projectRoot(import.meta.url);
process.chdir(root);

let cfg = {};
try {
  cfg = JSON.parse(readFileSync(".drwil/ia-first.json", "utf8"));
} catch {}
const lang = cfg.lang === "en" ? "en" : "fr";
const T = {
  fr: {
    project: "contrôles du projet (lint, tests…)",
    noCheck: "aucun déclaré dans .drwil/ia-first.json → checks",
    notRun: "non exécuté :",
    failed: "échec :",
    ok: (n) => `contrôles exécutés : OK (${n} non exécuté(s), voir ci-dessus)`,
    staleAudit: (f, n) => `audit périmé (${f}, vieux de ${n} jours) : à relancer`,
    staleIntent: (f, n) => `intention sans revue depuis ${n} jours (${f}) : la revoir, la mettre à jour (ligne « Revu le ») ou la clore`,
    intentLeft: (b, i) =>
      `la fiche ${b} est supprimée sans que son intention ${i} soit touchée : la mettre à jour, ou la résumer au journal et la retirer si elle est couverte`,
  },
  en: {
    project: "project checks (lint, tests…)",
    noCheck: "none declared in .drwil/ia-first.json → checks",
    notRun: "not run:",
    failed: "failed:",
    ok: (n) => `checks run: OK (${n} not run, see above)`,
    staleAudit: (f, n) => `stale audit (${f}, ${n} days old): time to re-run`,
    staleIntent: (f, n) => `intent not reviewed for ${n} days (${f}): review it, update it ("Reviewed on" line) or close it`,
    intentLeft: (b, i) =>
      `brief ${b} is deleted without touching its intent ${i}: update it, or summarise it in the journal and remove it when covered`,
  },
}[lang];

// At pre-commit only (DRWIL_HOOK=pre-commit, set by .githooks/pre-commit), the engine defers
// a project check outside its `paths`. If git does not answer, everything runs (never deferred by default).
function stagedFiles() {
  if (process.env.DRWIL_HOOK !== "pre-commit") return null;
  const r = spawnSync("git", ["diff", "--cached", "--name-only", "--no-renames", "-z"], { encoding: "utf8" });
  return r.status === 0 ? r.stdout.split("\0").filter(Boolean) : null;
}

// Only the commit may leave a check unrun (a check deferred outside its `paths` is replayed on push
// and in CI): everywhere else a check that did not run fails, never a green light (QUA-013).
const atCommit = process.env.DRWIL_HOOK === "pre-commit";
const failures = [];
const notRun = [];
// An unconfigured project (no CI, no declared check) stays a visible warning: only a check that
// should have run and could not (missing tool, deferred) blocks.
// In CI nothing is "unconfigured": the pipeline must be complete.
const unrun = (what, configured = false) =>
  (atCommit || (configured && !process.env.CI) ? notRun : failures).push(
    atCommit || (configured && !process.env.CI) ? what : `${T.notRun} ${what}`,
  );
const announce = (name) => console.log(`[ia-first] ${name}…`);
// "not-applicable" (excluded by design: minimal mode, QUA-017 in CI…) is not shown, as before.
const staged = stagedFiles();
for (const c of checks(root, { cfg, staged })) {
  const r = c.run({ beforeRun: announce });
  if (r.status === "fail") failures.push(r.message ?? c.name);
  else if (r.status === "not-run") unrun(`${c.name} (${r.detail})`, r.configured);
}
if (!Array.isArray(cfg.checks) || !cfg.checks.length) unrun(`${T.project} (${T.noCheck})`, true);

// Non-blocking reminder (comfort, not a contract): an audit (opportunity, risks/debt) whose
// "Dernier scan"/"Last scan" is more than 30 days old would benefit from a re-run.
const STALE_AFTER_DAYS = 30;
for (const file of [
  { fr: "docs/audit-risques.md", en: "docs/audit-risks.md" }[lang],
  { fr: "docs/decouverte-valeur.md", en: "docs/value-discovery.md" }[lang],
]) {
  if (!existsSync(file)) continue;
  const line = readFileSync(file, "utf8")
    .split(/\r?\n/)
    .find((l) => /^>\s*(dernier scan|last scan)/i.test(l));
  const m = line?.match(/\d{4}-\d{2}-\d{2}/);
  if (!m) continue;
  const days = Math.floor((Date.now() - new Date(m[0]).getTime()) / 86400000);
  if (days >= STALE_AFTER_DAYS) notRun.push(T.staleAudit(file, days));
}

// Reminders on the intents (QUA-036): never blocking, they say what the closing left behind.
const reminders = [
  ...(staged ? intentsLeftBehind(root, staged).map((l) => T.intentLeft(l.brief, l.intent)) : []),
  ...staleIntents(root, cfg).map((s) => T.staleIntent(s.file, s.days)),
];

// An ID cited to the human comes with its label (QUA-039).
const labels = glossary(root);
const say = (line) => console.log(annotate(line, labels));
for (const c of notRun) say(`[ia-first] ⚠ ${T.notRun} ${c}`);
for (const r of reminders) say(`[ia-first] ⚠ ${r}`);
if (failures.length) {
  for (const c of failures) say(`[ia-first] ✗ ${T.failed} ${c}`);
  process.exit(1);
}
console.log(`[ia-first] ${T.ok(notRun.length)}`);
