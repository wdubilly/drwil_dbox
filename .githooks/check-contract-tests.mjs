#!/usr/bin/env node
// Link between a contract and its tests (QUA-029): each file cited by the `**Tests**` field of a
// contract exists and carries the contract's ID (in a test name, by convention). Stack-agnostic on
// purpose: test names are never parsed, the ID only has to appear in the file.
// Requirement added after projects were already equipped: blocking only if the project has it
// (`newRequirements`: "blocking", default of fresh installs), otherwise reported without failing.
import { existsSync, readFileSync, realpathSync, statSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { readContracts } from "./contracts.mjs";
import { annotate, glossary } from "./glossary.mjs";

const TEXTS = {
  fr: {
    missing: (id, f) => `${id} : test cité introuvable \`${f}\``,
    noId: (id, f) => `${id} : \`${f}\` ne porte pas l'ID ${id} (le citer dans le nom du test qui le protège)`,
    summary: (n, p) => `contract-tests : ${n} lien(s) contrat ↔ test vérifié(s), ${p} problème(s)`,
    warning: `contract-tests : exigence récente, non bloquante (réglage « newRequirements » : warning ; « blocking » pour l'imposer)`,
  },
  en: {
    missing: (id, f) => `${id}: cited test not found \`${f}\``,
    noId: (id, f) => `${id}: \`${f}\` does not carry the ID ${id} (cite it in the name of the test that protects it)`,
    summary: (n, p) => `contract-tests: ${n} contract ↔ test link(s) checked, ${p} problem(s)`,
    warning: `contract-tests: recent requirement, not blocking (setting "newRequirements": warning; "blocking" to enforce it)`,
  },
};

/**
 * Broken links of the contracts: `read(path)` returns the file's text, or null if it is not a
 * readable file. Returns { links, problems }. `APP-002` is not found inside `APP-0021`.
 */
export function linkProblems(contracts, read, lang = "fr") {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  const problems = [];
  let links = 0;
  for (const k of contracts) {
    for (const file of k.tests ?? []) {
      links++;
      const text = read(file);
      if (text === null) problems.push(T.missing(k.id, file));
      else if (!new RegExp(`(?<![A-Za-z0-9-])${k.id}(?!\\d)`).test(text)) problems.push(T.noId(k.id, file));
    }
  }
  return { links, problems };
}

/** Runs the check on `root`; returns the exit code. */
export function main(root = process.cwd(), log = console.log) {
  let cfg = {};
  try {
    cfg = JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8"));
  } catch {}
  const lang = cfg.lang === "en" ? "en" : "fr";
  const T = TEXTS[lang];
  const registry = join(root, cfg.dirs?.contracts ?? "docs/contracts.md");
  const contracts = existsSync(registry) ? readContracts(readFileSync(registry, "utf8")) : [];
  const read = (file) => {
    const p = join(root, file);
    return existsSync(p) && statSync(p).isFile() ? readFileSync(p, "utf8") : null;
  };
  const { links, problems } = linkProblems(contracts, read, lang);
  const blocking = cfg.newRequirements === "blocking";
  // An ID cited to the human comes with its label (QUA-039).
  const labels = glossary(root);
  for (const p of problems) log(annotate(`  ${blocking ? "✗" : "⚠"} ${p}`, labels));
  log(T.summary(links, problems.length));
  if (problems.length && !blocking) log(T.warning);
  return problems.length && blocking ? 1 : 0;
}

// Through a symbolic link (an installed copy), argv[1] is the link: compare the real paths.
const direct = process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url));
if (direct) process.exitCode = main();
