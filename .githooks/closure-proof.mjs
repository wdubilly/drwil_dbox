#!/usr/bin/env node
// The closing proof of a workstream (QUA-084): what it had got accepted, and what proves it, written
// as a versioned document by the closing (close.mjs). A module of its own, so that neither file goes
// past the cap (QUA-001), and so that a project can read a proof without going through the closing.
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, posix } from "node:path";
import { acceptanceCriteria, config } from "./briefs.mjs";
import { latestEvidence } from "./state.mjs";

const TEXTS = {
  fr: {
    proofWritten: (f) => `preuve de clôture écrite : ${f}`,
    proofRefused: (r) => `clôture refusée : la preuve de clôture ne peut pas être écrite — ${r}`,
    proofNoEvidence: "aucune évidence de `drwil verify --evidence` à recopier",
    proofOtherCommit: (n) => `l'évidence ${n} ne date pas de HEAD : la renouveler (\`npx drwil verify --evidence\`)`,
    proofNotPass: (s) => `le verdict de la dernière évidence est ${s.toUpperCase()} : rien à prouver tant qu'il n'est pas PASS`,
    proofUnwritable: (f, m) => `${f} : ${m}`,
    proofNotCited: (j, f) => `${j} : l'entrée de journal du chantier ne cite pas \`${f}\` — la preuve de clôture se cite depuis le journal`,
    proofHead: "critères de ce chantier, et ce qui les prouve aujourd'hui encore (rejoués par `drwil verify` après la clôture) :",
    proofHeads: ["Critère", "Contrat", "Contrôle", "Empreinte des fichiers couverts"],
    proofTitle: (w) => `# Preuve de clôture — ${w}`,
    proofClosed: (d) => `**Clos le** : ${d}`,
    proofCommit: (c, v) => `**Commit vérifié** : \`${c}\` — verdict \`drwil verify\` : ${v.toUpperCase()}`,
    proofNone: "_aucun critère d'acceptation_",
    proofFiles: (n) => `${n} fichier(s)`,
    proofHuman: "[humain]",
  },
  en: {
    proofWritten: (f) => `closing proof written: ${f}`,
    proofRefused: (r) => `closing refused: the closing proof cannot be written — ${r}`,
    proofNoEvidence: "no `drwil verify --evidence` evidence to copy from",
    proofOtherCommit: (n) => `evidence ${n} is not from HEAD: renew it (\`npx drwil verify --evidence\`)`,
    proofNotPass: (s) => `the latest evidence's verdict is ${s.toUpperCase()}: nothing to prove until it is PASS`,
    proofUnwritable: (f, m) => `${f}: ${m}`,
    proofNotCited: (j, f) => `${j}: the workstream's journal entry does not cite \`${f}\` — the closing proof is cited from the journal`,
    proofHead: "criteria of this workstream, and what still proves them today (replayed by `drwil verify` after the closing):",
    proofHeads: ["Criterion", "Contract", "Check", "Fingerprint of the covered files"],
    proofTitle: (w) => `# Closing proof — ${w}`,
    proofClosed: (d) => `**Closed on**: ${d}`,
    proofCommit: (c, v) => `**Verified commit**: \`${c}\` — \`drwil verify\` verdict: ${v.toUpperCase()}`,
    proofNone: "_no acceptance criteria_",
    proofFiles: (n) => `${n} file(s)`,
    proofHuman: "[human]",
  },
};

const texts = (root) => TEXTS[config(root).lang === "en" ? "en" : "fr"];

/** Text of a message of this module, read by the closing. */
export const proofText = (root, key, ...args) => texts(root)[key](...args);

const PROOFS_DIR = "docs/preuves";

const proofsDir = (root) => config(root).dirs?.proofs ?? PROOFS_DIR;

const journalPath = (root) => `${config(root).dirs?.projects ?? "docs/projects"}/journal.md`;

/** Path of the closing proof of `brief`, relative to the root. */
export const proofPath = (root, brief) => `${proofsDir(root)}/${posix.basename(brief, ".md")}.md`;

/**
 * The closing proof of `brief` (QUA-084): what the workstream had accepted, and what proves it, copied
 * from the latest `drwil verify --evidence` evidence — criteria, contracts, checks, verified commit and
 * fingerprint of the files each contract covers. `{ file, text }`, or `{ problem }` when there is
 * nothing to prove from (no evidence, evidence from another commit, verdict not passing): the closing
 * is then refused, rather than deleting a brief whose proof would stay unwritten. `head`: the commit
 * the evidence must come from, read by the caller.
 */
export function closureProof(root, brief, text, { head, now = new Date() } = {}) {
  const T = texts(root);
  const last = latestEvidence(root);
  if (!last?.data) return { problem: T.proofNoEvidence };
  const ev = last.data;
  if (!head || ev.commit !== head) return { problem: T.proofOtherCommit(last.name) };
  if (ev.status !== "pass" && ev.status !== "attested") return { problem: T.proofNotPass(String(ev.status ?? "?")) };
  const cell = (v) => String(v).replace(/\\/g, "\\\\").replace(/\|/g, "\\|").replace(/\n/g, " ").trim();
  const entries = Array.isArray(ev.evidence) ? ev.evidence : [];
  const row = (c) => {
    const entry = c.link ? entries.find((e) => e.id === c.link) : null;
    const checks = (entry?.checks ?? []).map((k) => `\`${cell(k.id)}\``).join(", ");
    const coverage = entry?.coverage ?? null;
    const files = coverage ? Object.keys(coverage).length : 0;
    // One fingerprint for the whole coverage, as an attestation keeps it (QUA-081): the files' content, not their names.
    const print = files
      ? `\`sha256:${createHash("sha256").update(JSON.stringify(coverage)).digest("hex").slice(0, 16)}\` (${T.proofFiles(files)})`
      : "—";
    return `| ${cell(c.text)}${c.human ? ` ${T.proofHuman}` : ""} | ${c.link ? `\`${cell(c.link)}\`` : "—"} | ${checks || "—"} | ${print} |`;
  };
  const criteria = acceptanceCriteria(text);
  const table = criteria.length ? [`| ${T.proofHeads.join(" | ")} |`, "|---|---|---|---|", ...criteria.map(row)] : [T.proofNone];
  return {
    file: proofPath(root, brief),
    // The brief is deleted by the closing: it is named, never cited as a path (check-docs refuses a dead link).
    text: `${[
      T.proofTitle(posix.basename(brief, ".md")),
      "",
      T.proofClosed(now.toISOString().slice(0, 10)),
      T.proofCommit(head.slice(0, 10), String(ev.status)),
      "",
      T.proofHead,
      "",
      ...table,
    ].join("\n")}\n`,
  };
}

/** Writes the proof file, creating its folder; null, or the reason it could not be written. */
export function writeProof(root, proof) {
  try {
    mkdirSync(dirname(join(root, proof.file)), { recursive: true });
    writeFileSync(join(root, proof.file), proof.text);
    return null;
  } catch (e) {
    return texts(root).proofUnwritable(proof.file, e.message);
  }
}

/** The journal entry cites the closing proof, or says it does not (QUA-084): the proof is reachable from the journal. */
export function proofNotCited(root, brief) {
  const file = proofPath(root, brief);
  const journal = journalPath(root);
  if (!existsSync(join(root, journal))) return [];
  return readFileSync(join(root, journal), "utf8").includes(file) ? [] : [texts(root).proofNotCited(journal, file)];
}
