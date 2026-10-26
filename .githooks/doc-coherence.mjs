// Coherence rules between the index, the briefs and the code, written once: check-docs.mjs (at each
// commit, as warnings) and close.mjs (at closing, blocking and corrective) read them from here, so
// the same inconsistency is seen by both.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { branchFiles, briefIntent, briefStatus } from "./briefs.mjs";
import { briefReferences, changedFiles } from "./check-code-rules.mjs";

const TEXTS = {
  fr: {
    openButDone: (brief) => `case ouverte mais la fiche ${brief} se dit déjà terminée (QUA-015)`,
    checkedButOpen: (brief) => `case cochée mais la fiche ${brief} ne se dit pas terminée (QUA-015)`,
    deadClosing: (place, b) => `${place} cite ${b}, qui sera supprimée : à réorienter vers le journal`,
    deadGone: (place, b) => `${place} cite ${b}, déjà retirée : citation à supprimer`,
    codeCite: (place, b) =>
      `${place} cite la fiche de chantier ${b} : réécrire ce commentaire en citant le contrat que ce code sert (la clôture ne touche pas au code) (QUA-049)`,
    intentLeft: (b, i) =>
      `la fiche ${b} est supprimée sans que son intention ${i} soit touchée : la mettre à jour, ou la résumer au journal et la retirer si elle est couverte (QUA-036)`,
    twin: (p, news, old) => `${p} : cité par ${news}${old ? `, déjà cité par ${old}` : ""} — doublon possible, à vérifier`,
  },
  en: {
    openButDone: (brief) => `item unchecked but ${brief} already says it is done (QUA-015)`,
    checkedButOpen: (brief) => `item checked but ${brief} does not say it is done (QUA-015)`,
    deadClosing: (place, b) => `${place} cites ${b}, which will be deleted: to redirect to the journal`,
    deadGone: (place, b) => `${place} cites ${b}, already removed: citation to delete`,
    codeCite: (place, b) =>
      `${place} cites the workstream brief ${b}: rewrite this comment citing the contract this code serves (closing does not touch code) (QUA-049)`,
    intentLeft: (b, i) =>
      `brief ${b} is deleted without touching its intent ${i}: update it, or summarise it in the journal and remove it when covered (QUA-036)`,
    twin: (p, news, old) => `${p}: cited by ${news}${old ? `, already cited by ${old}` : ""} — possible duplicate, to check`,
  },
};

export const MARKERS_RE = /\[(IA|humain|décision|AI|human|decision)\]/i;
// A line that declares the file optional must not fail the check.
export const OPTIONAL_RE = /si présent|if present/i;
// A planned path not created yet, marked right after the citation, is not an error.
export const TO_CREATE_RE = /^\s*\(à créer\)|^\s*\(to create\)/i;
export const BRIEF_CITATION_RE = /`((?:docs\/projects|docs\/intentions)\/[^`]+\.md)`/g;
// tolerates a date in parentheses right after the word (e.g. "**Status** (2026-10-04):").
export const STATUS_RE = /^\*{0,2}(Statut|Status)\*{0,2}\s?(\([^)]*\))?\s?:/;

/**
 * The box ([ ]/[x]) of an index line must reflect the actual status of the brief it cites. The "Status"
 * text stays free (prose), so only a brief with a dated Status line in its first lines is judged.
 * `[{ place: "index:line", message, brief, line, checked }]`; `except`: a brief left out (the one closing).
 */
export function checkboxProblems(root, cfg, lang, { except } = {}) {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  const problems = [];
  const indexPath = join(root, cfg.dirs?.index ?? "docs/projects/pending.md");
  if (!existsSync(indexPath)) return problems;
  const relIndex = relative(root, indexPath).split("\\").join("/");
  const lines = readFileSync(indexPath, "utf8").split(/\r?\n/);
  // groups each bullet (an entry may spread over several indented lines): a new bullet starts at "- [ ]" or "- [x]".
  let bullet = null;
  const bullets = [];
  for (let i = 0; i < lines.length; i++) {
    const m = /^\s*-\s*\[( |x|X)\]/.exec(lines[i]);
    if (m) {
      bullet = { checked: m[1].toLowerCase() === "x", line: i + 1, text: lines[i] };
      bullets.push(bullet);
    } else if (bullet && lines[i].trim() !== "" && /^\s+\S/.test(lines[i])) {
      bullet.text += "\n" + lines[i];
    } else {
      bullet = null;
    }
  }
  for (const b of bullets) {
    const judged = [];
    for (const m of b.text.matchAll(BRIEF_CITATION_RE)) {
      const briefPath = join(root, m[1]);
      if (m[1] === except || !existsSync(briefPath)) continue; // a missing path is reported elsewhere.
      const briefText = readFileSync(briefPath, "utf8");
      if (
        !briefText
          .split(/\r?\n/)
          .slice(0, 15)
          .some((l) => STATUS_RE.test(l))
      )
        continue; // no Status line: reported elsewhere.
      judged.push({ brief: m[1], done: briefStatus(briefText) === "done" });
    }
    // One box for several briefs can only be aligned when they all agree; an open box needs its marker.
    const agree = judged.length > 0 && judged.every((j) => j.done === judged[0].done);
    const fixTo = agree && (judged[0].done || MARKERS_RE.test(b.text)) ? (judged[0].done ? "x" : " ") : null;
    for (const j of judged) {
      const wrong = !b.checked && j.done ? "openButDone" : b.checked && !j.done ? "checkedButOpen" : null;
      if (wrong) problems.push({ place: `${relIndex}:${b.line}`, message: T[wrong](j.brief), file: relIndex, line: b.line, fixTo });
    }
  }
  return problems;
}

const git = (root, args) => {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
  return r.status === 0 ? r.stdout : null;
};

/** Lines [from, to) of the index item citing this brief, and it alone; null if there is none. */
export function indexItem(lines, brief) {
  for (let i = 0; i < lines.length; i++) {
    if (!/^\s*- \[[ xX]\]/.test(lines[i])) continue;
    let end = i + 1;
    while (end < lines.length && /^\s+\S/.test(lines[end]) && !/^\s*- \[/.test(lines[end])) end++;
    const cited = new Set(
      lines
        .slice(i, end)
        .join("\n")
        .match(/`docs\/[^`]+\.md`/g),
    );
    if (cited.has(`\`${brief}\``) && cited.size === 1) return [i, end];
  }
  return null;
}

const escapeRe = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

// Dead reference: redirected to the journal when it is the closing brief, otherwise the citation
// goes, with the dash or the parentheses that introduced it.
const redirect = (cite, to) => (s) => s.split(`\`${cite}\``).join(`\`${to}\``);
const drop = (cite) => {
  const c = escapeRe(cite);
  const re = new RegExp(`\\s*(?:\\((?:voir |see |cf\\. )?\`${c}\`\\)|[—–]\\s*\`${c}\`|\`${c}\`)`);
  return (s) => s.replace(re, "");
};

/**
 * Citations of a brief or intent in the Markdown files that no longer lead anywhere, or will not once
 * `closing` (the brief being closed) is deleted: `[{ place, message, file, line, edit }]`, `edit` being
 * the line rewrite (redirect to the journal for the closing brief, deletion for any other).
 */
export function deadReferences(root, cfg, lang, closing, { journal = "docs/projects/journal.md" } = {}) {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  const out = git(root, ["ls-files", "-z", "--cached", "--others", "--exclude-standard"]) ?? "";
  const index = cfg.dirs?.index ?? "docs/projects/pending.md";
  const indexLines = existsSync(join(root, index)) ? readFileSync(join(root, index), "utf8").split(/\r?\n/) : [];
  const own = closing ? indexItem(indexLines, closing) : null;
  const found = [];
  for (const file of new Set(out.split("\0"))) {
    if (!file.endsWith(".md") || file === closing || !existsSync(join(root, file))) continue;
    const lines = readFileSync(join(root, file), "utf8").split(/\r?\n/);
    lines.forEach((line, i) => {
      if (OPTIONAL_RE.test(line) || OPTIONAL_RE.test(lines[i + 1] ?? "")) return;
      if (file === index && own && i >= own[0] && i < own[1]) return; // removed with the closing brief's item.
      const edits = [];
      for (const m of line.matchAll(BRIEF_CITATION_RE)) {
        const cite = m[1];
        if (/[<>{}*?|=:@\s]/.test(cite) || TO_CREATE_RE.test(line.slice(m.index + m[0].length))) continue;
        if (cite === closing) edits.push({ message: T.deadClosing, cite, edit: redirect(cite, journal) });
        else if (!existsSync(join(root, cite)) && !existsSync(join(root, dirname(file), cite)))
          edits.push({ message: T.deadGone, cite, edit: drop(cite) });
      }
      for (const e of edits)
        found.push({ place: `${file}:${i + 1}`, message: e.message(`${file}:${i + 1}`, e.cite), file, line: i + 1, edit: e.edit });
    });
  }
  return found;
}

/** A workstream brief cited by a code file's comment, changed or not: `[{ place, message }]`, to rewrite by hand. */
export function codeCitations(root, cfg, lang) {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  return briefReferences(root, cfg).map((r) => ({ place: `${r.file}:${r.line}`, message: T.codeCite(`${r.file}:${r.line}`, r.brief) }));
}

/** The closing brief's intent when it exists and neither this branch nor the working tree touched it: `[message]`. */
export function intentLeft(root, lang, brief, text) {
  const intent = briefIntent(text);
  if (!intent || !existsSync(join(root, intent))) return [];
  const touched = new Set([...changedFiles(root), ...branchFiles(root)]);
  return touched.has(intent) ? [] : [TEXTS[lang === "en" ? "en" : "fr"].intentLeft(brief, intent)];
}

const CITED_PATH = /`([^`\s<>{}*?|=:@$]+\/[^`\s]+\.[a-z]+)`/g;

/**
 * Warnings: a new intent (added on the branch or not committed yet) citing a path another intent
 * already cites — by path only, never by keywords. Paths under docs/ are shared by nature: skipped.
 * One warning per path cited, naming every new intent citing it (QUA-128), not one per intent.
 */
export function twinIntents(root, cfg, lang) {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  const dir = (cfg.dirs?.intentions ?? "docs/intentions").replace(/\/$/, "");
  const mine = (f) => f.startsWith(`${dir}/`) && f.endsWith(".md") && !/\/(README|notes)\.md$/i.test(f);
  const untracked = (git(root, ["ls-files", "-z", "--others", "--exclude-standard"]) ?? "").split("\0");
  const added = [
    ...branchFiles(root, "A"),
    ...changedFiles(root).filter((f) => git(root, ["cat-file", "-e", `HEAD:${f}`]) === null),
    ...untracked,
  ];
  const all = (git(root, ["ls-files", "-z", "--cached", "--others", "--exclude-standard"]) ?? "").split("\0").filter(mine);
  const paths = (f) =>
    [...new Set([...readFileSync(join(root, f), "utf8").matchAll(CITED_PATH)].map((m) => m[1]))].filter((p) => !p.startsWith("docs/"));
  const byPath = new Map();
  for (const f of new Set(added.filter((f) => mine(f) && existsSync(join(root, f))))) {
    for (const p of paths(f)) {
      const others = all.filter((o) => o !== f && existsSync(join(root, o)) && paths(o).includes(p));
      if (!others.length) continue;
      const g = byPath.get(p) ?? { news: new Set(), others: new Set() };
      byPath.set(p, g);
      g.news.add(f);
      for (const o of others) g.others.add(o);
    }
  }
  // The new intents citing each other are named once, among the new ones.
  return [...byPath].map(([p, g]) => T.twin(p, [...g.news].join(", "), [...g.others].filter((o) => !g.news.has(o)).join(", ")));
}

/**
 * The line rewrites behind the findings, merged per line: `[{ file, line, from, to }]` — what the
 * closing proposes (a diff) and what `--fix` writes.
 */
export function proposals(root, findings) {
  const edits = new Map();
  for (const f of findings) {
    const edit = f.edit ?? (f.fixTo ? (s) => s.replace(/\[( |x|X)\]/, `[${f.fixTo}]`) : null);
    if (edit) edits.set(`${f.file}\0${f.line}`, [...(edits.get(`${f.file}\0${f.line}`) ?? []), { file: f.file, line: f.line, edit }]);
  }
  const out = [];
  for (const list of edits.values()) {
    const lines = readFileSync(join(root, list[0].file), "utf8").split(/\r?\n/);
    const from = lines[list[0].line - 1];
    const to = list.reduce((s, e) => e.edit(s), from);
    if (to !== from) out.push({ file: list[0].file, line: list[0].line, from, to });
  }
  return out.sort((a, b) => a.file.localeCompare(b.file) || a.line - b.line);
}

/** Writes the proposals; a line that changed since they were computed is left alone. Returns how many were applied. */
export function applyProposals(root, list) {
  let applied = 0;
  for (const file of new Set(list.map((p) => p.file))) {
    const raw = readFileSync(join(root, file), "utf8");
    const lines = raw.split("\n");
    for (const p of list.filter((x) => x.file === file)) {
      const cr = lines[p.line - 1].endsWith("\r") ? "\r" : "";
      if (lines[p.line - 1].replace(/\r$/, "") !== p.from) continue;
      lines[p.line - 1] = p.to + cr;
      applied++;
    }
    writeFileSync(join(root, file), lines.join("\n"));
  }
  return applied;
}
