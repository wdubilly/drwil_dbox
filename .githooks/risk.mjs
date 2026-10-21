// Risk levels of a workstream brief: ceremony follows risk, traceability
// (QUA-016) does not change.
// - LOW: attached to docs/projects/routine-maintenance.md, no dedicated brief;
// - MEDIUM: brief with a scope block;
// - HIGH: brief, Decisions section, related contracts cited, evidence.
// The level is explicit (`**Risk**: HIGH`) and can only be raised: a scope
// that touches the governance mechanics imposes HIGH at least.
import { acceptanceCriteria } from "./briefs.mjs";
import { fnmatch, readBlock } from "./scope.mjs";

export const LEVELS = ["LOW", "MEDIUM", "HIGH"];

/** Paths whose change imposes HIGH (configurable: .drwil/ia-first.json → risk.sensitivePaths). */
export const SENSITIVE_PATHS = [".githooks/*", ".github/workflows/*", ".gitlab-ci.yml", ".drwil/*"];

/** Minimum level imposed by a brief's scope: HIGH if a pattern touches a sensitive path. */
export function minimumLevel(patterns, sensitive = SENSITIVE_PATHS) {
  const touches = patterns.some((p) => sensitive.some((s) => fnmatch(p, s) || fnmatch(s, p) || p.startsWith(s.replace(/\*$/, ""))));
  return touches ? "HIGH" : "LOW";
}

const TEXTS = {
  fr: {
    unknown: (v) => `niveau de risque « ${v} » inconnu (LOW, MEDIUM ou HIGH)`,
    tooLow: (v, min) =>
      `risque ${v} déclaré, mais le cadrage touche la mécanique de gouvernance : ${min} au minimum (forçable vers le haut seulement)`,
    missing: (min) => `pas de champ « Risque » alors que le cadrage impose ${min}`,
    decisions: () => `risque HIGH sans section « Décisions »`,
    contracts: () => `risque HIGH sans contrat concerné cité (ID du registre)`,
    noTest: () => `le cadrage cite du code mais aucun fichier de test : les lister dès le cadrage évite un élargissement en plein chantier`,
    noChecks: () =>
      `un critère crée un contrat, mais le cadrage ne cite pas .drwil/ia-first.json, où se déclare son contrôle : l'ajouter dès le cadrage`,
  },
  en: {
    unknown: (v) => `unknown risk level "${v}" (LOW, MEDIUM or HIGH)`,
    tooLow: (v, min) => `risk ${v} declared, but the scope touches governance mechanics: ${min} at least (can only be raised)`,
    missing: (min) => `no "Risk" field although the scope requires ${min}`,
    decisions: () => `HIGH risk without a "Decisions" section`,
    contracts: () => `HIGH risk without any related contract cited (registry ID)`,
    noTest: () => `the scope cites code but no test file: listing them when scoping avoids widening it mid-workstream`,
    noChecks: () =>
      `a criterion creates a contract, but the scope does not cite .drwil/ia-first.json, where its check is declared: add it when scoping`,
  },
};

const TEST_FILE = /(\.test\.|\.spec\.|_test\.|(^|\/)(tests?|__tests__)\/)/;
const NEW_CONTRACT = /contrat à créer|contract to create/i;

/**
 * What a scope usually forgets (QUA-035), as warnings: code without any test file; a criterion that
 * creates a contract without the checks configuration, where that contract's own check is declared.
 */
export function scopeGaps(text, patterns, T) {
  const gaps = [];
  const code = patterns.filter((p) => !p.endsWith(".md") && !p.startsWith("docs/") && !TEST_FILE.test(p));
  if (code.length && !patterns.some((p) => TEST_FILE.test(p))) gaps.push(T.noTest());
  if (acceptanceCriteria(text).some((c) => NEW_CONTRACT.test(c.text)) && !patterns.includes(".drwil/ia-first.json"))
    gaps.push(T.noChecks());
  return gaps;
}

/**
 * Checks a brief's risk level. Returns { errors, warnings } (messages without prefix).
 * Without a Risk field: never an error (compatibility), a warning if HIGH is imposed.
 */
export function checkRisk(text, { sensitive = SENSITIVE_PATHS, idPattern = "(?:SEC|QUA)-\\d{3}", lang = "fr" } = {}) {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  const errors = [];
  const warnings = [];
  const { patterns } = readBlock(text);
  const min = minimumLevel(patterns, sensitive);
  // Scope gaps come last: the risk messages say more.
  const gaps = patterns.length ? scopeGaps(text, patterns, T) : [];
  const m = /^\*\*(?:Risque|Risk)\*\*\s*:\s*`?([^\s`]+)`?/m.exec(text);
  if (!m) {
    if (min === "HIGH") warnings.push(T.missing(min));
    return { errors, warnings: [...warnings, ...gaps] };
  }
  const level = m[1].toUpperCase();
  if (!LEVELS.includes(level)) return { errors: [T.unknown(m[1])], warnings: gaps };
  if (LEVELS.indexOf(level) < LEVELS.indexOf(min)) errors.push(T.tooLow(level, min));
  if (level === "HIGH") {
    if (!/^##\s*(\d+\.\s*)?(Décisions|Decisions)\b/m.test(text)) errors.push(T.decisions());
    if (!new RegExp(idPattern).test(text)) errors.push(T.contracts());
  }
  return { errors, warnings: [...warnings, ...gaps] };
}
