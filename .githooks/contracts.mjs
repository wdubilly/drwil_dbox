// Reading and validation of the contract registry (docs/contracts.md, canonical
// source of the contracts). Dependency-free, shared by the hooks and
// `drwil verify` (a single engine, .githooks/engine.mjs).
//
// Two forms are read:
// - sections (target form): `## ID — title` then `**Name**: value` fields;
// - table (legacy form): `| ID | Rule | … |`, read for compatibility.
// Verification fields: `**Check**` (identifier(s) of a check known to the engine,
// between backticks) and `**Manual**` (the part of the proof that stays human).
// `**Tests**`: the test files that protect the contract, between backticks (checked by
// .githooks/check-contract-tests.mjs: each exists and carries the contract's ID).
// A check's command never lives here.

/** Identifiers of the steps of .githooks/run-checks.mjs. The first four are also those of BASE in check-control-coverage.mjs. */
export const BASE_CHECKS = [
  "secrets",
  "docs-references",
  "contract-tests",
  "architecture-map",
  "scope-gate",
  "checks-self-test",
  "ci-coverage",
  "main-branch",
];

const ID_RE = /^[A-Z][A-Z0-9]*-\d+$/;
const CHECK_FIELD = /^(contrôle|controle|check)$/i;
const MANUAL_FIELD = /^(manuel|manual)$/i;
const RULE_FIELD = /^(règle|regle|rule)$/i;
const SEVERITY_FIELD = /^(sévérité|severite|severity)$/i;
const TESTS_FIELD = /^tests$/i;

// Only blocking contracts decide the gate of `drwil verify`; the others are run and shown
// without changing the exit code. Default: blocking (P0 behaviour). A registry is written in its
// project's language: both words are read.
export const SEVERITIES = {
  bloquant: "blocking",
  blocking: "blocking",
  avertissement: "warning",
  warning: "warning",
  indicatif: "advisory",
  advisory: "advisory",
};

/** Identifiers cited in a Check field: those between backticks, otherwise the comma-separated list. */
function identifiers(value) {
  const quoted = [...value.matchAll(/`([^`]+)`/g)].map((m) => m[1].trim());
  const list = quoted.length ? quoted : value.split(/[,;]/);
  return list.map((v) => v.trim()).filter(Boolean);
}

function cells(line) {
  return line
    .trim()
    .replace(/^\|/, "")
    .replace(/\|$/, "")
    .split("|")
    .map((c) => c.trim());
}

/**
 * Contracts of the text, in order: { id, title, line, form, rule, checks, manual, tests, severity, text }.
 * `text`: the raw section (or table row), base of an attestation's fingerprint.
 * Chain intent → requirement → proof: **Reason** (human intent), **Rule** (explicit requirement),
 * **Check** or **Manual** (proof). Missing field: null.
 */
export function readContracts(text) {
  const lines = text.split(/\r?\n/);
  const contracts = [];
  let current = null;
  let header = null;
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const heading = /^##\s+([A-Z][A-Z0-9]*-\d+)\s*(?:[—–-]\s*(.*))?$/.exec(line);
    if (heading) {
      current = {
        id: heading[1],
        title: (heading[2] ?? "").trim(),
        line: i + 1,
        form: "section",
        rule: null,
        checks: null,
        manual: null,
        tests: null,
        severity: "blocking",
        rawSeverity: null,
        text: line,
      };
      contracts.push(current);
      header = null;
      continue;
    }
    if (/^##\s/.test(line)) {
      current = null;
      header = null;
      continue;
    }
    if (current) current.text += `\n${line}`;
    // `**Name** : value` or `**Name:** value`; value possibly on the next line.
    const field = /^\*\*([^*:]+?)\s*:?\*\*\s*:?\s*(.*)$/.exec(line);
    if (current && field) {
      let value = field[2].trim();
      if (!value) {
        let j = i + 1;
        while (j < lines.length && !lines[j].trim()) j++;
        if (j < lines.length && !/^\s*(\*\*|#|\|)/.test(lines[j])) value = lines[j].trim();
      }
      const name = field[1].trim();
      if (CHECK_FIELD.test(name)) current.checks = identifiers(value);
      else if (MANUAL_FIELD.test(name)) current.manual = value;
      else if (RULE_FIELD.test(name)) current.rule = value;
      else if (TESTS_FIELD.test(name)) current.tests = identifiers(value);
      else if (SEVERITY_FIELD.test(name)) {
        current.rawSeverity = value.replace(/`/g, "").trim();
        current.severity = SEVERITIES[current.rawSeverity.toLowerCase()] ?? null;
      }
      continue;
    }
    if (!/^\s*\|/.test(line)) {
      header = null;
      continue;
    }
    const c = cells(line);
    if (c.every((x) => /^:?-+:?$/.test(x))) continue;
    if (!ID_RE.test(c[0])) {
      header = c;
      continue;
    }
    const col = (re) => (header ? header.findIndex((h) => re.test(h)) : -1);
    const iCheck = col(CHECK_FIELD);
    const iManual = col(MANUAL_FIELD);
    const iRule = col(RULE_FIELD);
    contracts.push({
      id: c[0],
      title: c[1] ?? "",
      line: i + 1,
      form: "table",
      rule: iRule >= 0 ? c[iRule] || null : null,
      checks: iCheck >= 0 && c[iCheck] && c[iCheck] !== "—" ? identifiers(c[iCheck]) : null,
      manual: iManual >= 0 && c[iManual] && c[iManual] !== "—" ? c[iManual] : null,
      tests: null,
      severity: "blocking",
      rawSeverity: null,
      text: line,
    });
  }
  for (const k of contracts)
    k.text = k.text
      .split("\n")
      .map((l) => l.trimEnd())
      .join("\n")
      .trim();
  return contracts;
}

const TEXTS = {
  fr: {
    duplicate: (id) => `contrat ${id} défini plusieurs fois`,
    unknown: (id, c, known) => `${id} : contrôle inconnu « ${c} » (connus : ${known})`,
    noProof: (id) => `${id} : aucune preuve déclarée (champ Contrôle ou Manuel)`,
    noRule: (id) => `${id} : exigence absente (champ Règle)`,
    severity: (id, v) => `${id} : sévérité « ${v} » inconnue (bloquant, avertissement ou indicatif)`,
    emptyCheck: (id) => `${id} : champ Contrôle vide`,
    catalogDuplicate: (id) => `ID ${id} défini plusieurs fois dans le catalogue`,
    both: (id, registry) =>
      `ID ${id} défini à la fois dans le catalogue et dans ${registry} : un contrat adopté quitte le catalogue ; sinon, renuméroter la ligne du catalogue`,
  },
  en: {
    duplicate: (id) => `contract ${id} defined more than once`,
    unknown: (id, c, known) => `${id}: unknown check "${c}" (known: ${known})`,
    noProof: (id) => `${id}: no declared proof (Check or Manual field)`,
    noRule: (id) => `${id}: missing requirement (Rule field)`,
    severity: (id, v) => `${id}: unknown severity "${v}" (blocking, warning or advisory)`,
    emptyCheck: (id) => `${id}: empty Check field`,
    catalogDuplicate: (id) => `ID ${id} defined more than once in the catalog`,
    both: (id, registry) =>
      `ID ${id} defined both in the catalog and in ${registry}: an adopted contract leaves the catalog; otherwise, renumber the catalog line`,
  },
};

/**
 * Configuration errors of the registry, each `{ id, line, type, message }`
 * (type: duplicate | empty-check | unknown | no-rule | no-proof | severity):
 * duplicate, unknown check, section contract without a requirement (Rule) or without
 * any declared proof: an unverifiable obligation is never accepted silently.
 * A table contract without Check nor Manual is not an error: it is a legacy
 * contract, treated as MANUAL.
 */
export function validateContracts(contracts, knownIds, lang = "fr") {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  const known = new Set(knownIds);
  const seen = new Set();
  const errors = [];
  const error = (k, type, message) => errors.push({ id: k.id, line: k.line, type, message });
  for (const k of contracts) {
    if (seen.has(k.id)) error(k, "duplicate", T.duplicate(k.id));
    seen.add(k.id);
    if (k.checks && !k.checks.length) error(k, "empty-check", T.emptyCheck(k.id));
    for (const c of k.checks ?? []) if (!known.has(c)) error(k, "unknown", T.unknown(k.id, c, [...known].join(", ")));
    if (k.severity === null) error(k, "severity", T.severity(k.id, k.rawSeverity));
    if (k.form === "section" && !k.rule) error(k, "no-rule", T.noRule(k.id));
    if (k.form === "section" && k.checks === null && k.manual === null) error(k, "no-proof", T.noProof(k.id));
  }
  return errors;
}

/** IDs a registry defines (heading `## ID` or first cell `| ID |`) matching `idPattern`, and those defined more than once. */
export function definedIds(text, idPattern) {
  const counts = new Map();
  for (const m of text.matchAll(new RegExp(`^(?:##\\s+|\\|\\s*)(${idPattern})`, "gm"))) counts.set(m[1], (counts.get(m[1]) ?? 0) + 1);
  return { defined: new Set(counts.keys()), duplicates: [...counts].filter(([, n]) => n > 1).map(([id]) => id) };
}

/**
 * ID conflicts between the registry and the catalog: an ID defined twice in the catalog, or both in
 * the catalog and in the registry (a contract adopted from the catalog keeps its ID and leaves it).
 * Catalog IDs are the first cell of a table row, `catalogue:ID` (French) or `catalog:ID`.
 */
export function idConflicts(catalogText, registryIds, registry, lang = "fr") {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  const ids = [...catalogText.matchAll(/^\|\s*catal(?:ogue|og):([A-Z][A-Z0-9]*-\d+)\b/gm)].map((m) => m[1]);
  const seen = new Set();
  const problems = [];
  for (const id of ids) {
    if (seen.has(id)) problems.push(T.catalogDuplicate(id));
    seen.add(id);
  }
  for (const id of seen) if (registryIds.has(id)) problems.push(T.both(id, registry));
  return problems;
}

/** Identifiers known to the engine: base + `id` of the checks declared by the project (.drwil/ia-first.json → checks). */
export function knownChecks(cfg) {
  const project = (Array.isArray(cfg?.checks) ? cfg.checks : []).map((c) => c.id).filter(Boolean);
  return [...BASE_CHECKS, ...project];
}
