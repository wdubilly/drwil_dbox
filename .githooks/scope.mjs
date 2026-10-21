// Grammar of the `scope` block of workstream briefs (docs/projects/*.md).
//
// Single source of the grammar: the commit-time check (check-docs.mjs) and the
// reminder to the agent (.claude/hooks/scope-reminder.mjs) both import it.
// A brief in docs/projects/ may carry, in an HTML comment:
//
//     <!-- scope
//     files:
//       - backend/app/routers/scripts.py
//       - frontend/src/components/Script*
//     -->
//
// A code file is covered if it matches a pattern (fnmatch style) of a brief,
// delivered or not. Nothing here throws on an unreadable brief: the mechanism
// helps, it must never break.

const BLOCK_RE = /<!--[ \t]*scope[ \t]*\r?\n([\s\S]*?)-->/;
const ENTRY_RE = /^-[ \t]+(\S+)$/;
const WILDCARDS = new Set(["*", "?", "["]);

/** Path relative to the root, `/` separators, no leading `./`. */
export function normalize(path) {
  let p = String(path).replace(/\\/g, "/").trim();
  while (p.startsWith("./")) p = p.slice(2);
  return p;
}

/**
 * The reminder's scope: the project's code and its configuration; never
 * docs/ nor a .md, so that the agent can always write its brief.
 * Read from the project config (.drwil/ia-first.json): layers, generic code
 * folders, extra files and patterns, CI files.
 */
export function isCode(path, cfg = {}) {
  const p = normalize(path);
  if (!p || p.startsWith("docs/") || /\.md$/i.test(p)) return false;
  const prefixes = [...(cfg.layers ?? []), ...(cfg.layerPrefixes ?? []), ...(cfg.codePrefixes ?? [])].map((x) => `${x}/`);
  if (prefixes.some((x) => p.startsWith(x))) return true;
  if ((cfg.extraCodeFiles ?? []).includes(p)) return true;
  if ((cfg.ciFiles ?? []).includes(p)) return true;
  return (cfg.extraCodeGlobs ?? []).some((g) => fnmatch(p, g));
}

/** fnmatch style: `*` and `?` cross `/`, the rest is literal. */
export function fnmatch(path, pattern) {
  const re = pattern
    .replace(/[.+^${}()|[\]\\]/g, "\\$&")
    .replace(/\*/g, ".*")
    .replace(/\?/g, ".");
  return new RegExp(`^${re}$`).test(path);
}

/** A pattern must name at least two folders before its first wildcard
 * (`backend/app/x*` yes, `backend/*` or `**` no): otherwise it covers a whole
 * tree and the reminder says nothing any more. */
export function tooBroad(pattern) {
  if (pattern.includes("**")) return true;
  const first = [...pattern].findIndex((c) => WILDCARDS.has(c));
  if (first === -1) return false;
  return (pattern.slice(0, first).match(/\//g) ?? []).length < 2;
}

/**
 * Valid patterns and problems of a brief's `scope` block. No block:
 * `{ patterns: [], problems: [] }`. A too broad pattern is reported and removed
 * from the valid patterns.
 */
export function readBlock(text) {
  const m = BLOCK_RE.exec(text);
  if (!m) return { patterns: [], problems: [] };
  const patterns = [];
  const problems = [];
  let header = false;
  for (const raw of m[1].split(/\r?\n/)) {
    const line = raw.trim();
    if (!line) continue;
    if (!header) {
      if (line === "files:") header = true;
      else problems.push({ kind: "header", line });
      continue;
    }
    const entry = ENTRY_RE.exec(line);
    if (entry) patterns.push(normalize(entry[1]));
    else problems.push({ kind: "line", line });
  }
  if (!header && problems.length === 0) problems.push({ kind: "header", line: "" });
  const valid = [];
  for (const pattern of patterns) {
    if (tooBroad(pattern)) problems.push({ kind: "broad", pattern });
    else valid.push(pattern);
  }
  return { patterns: valid, problems };
}

/**
 * Patterns completed by the copies the project declares (`scopeMirrors` of .drwil/ia-first.json, pairs
 * of path prefixes `{ "from": "templates/", "to": "" }`): a pattern under `from` also covers the same
 * path under `to`. Lets a brief name the source of a generated copy without listing the copy (QUA-035).
 */
export function withMirrors(patterns, cfg = {}) {
  const mirrors = (Array.isArray(cfg?.scopeMirrors) ? cfg.scopeMirrors : []).filter(
    (m) => typeof m?.from === "string" && m.from && typeof m.to === "string",
  );
  const all = [...patterns];
  for (const p of patterns) for (const m of mirrors) if (p.startsWith(m.from)) all.push(normalize(m.to + p.slice(m.from.length)));
  return [...new Set(all)];
}

/** `{ brief (relative path): patterns }` for the briefs whose block reads without a form error, copies included. */
export function patternsByBrief(briefTexts, cfg = {}) {
  const result = new Map();
  for (const [rel, text] of briefTexts) {
    const patterns = withMirrors(readBlock(text).patterns, cfg);
    if (patterns.length) result.set(rel, patterns);
  }
  return result;
}

export function briefsCovering(path, byBrief) {
  const p = normalize(path);
  const briefs = [];
  for (const [brief, patterns] of byBrief) if (patterns.some((g) => fnmatch(p, g))) briefs.push(brief);
  return briefs;
}

/** Code files of `paths` that no brief covers, sorted. */
export function outsideBriefs(paths, byBrief, cfg) {
  const set = new Set();
  for (const path of paths) {
    const p = normalize(path);
    if (isCode(p, cfg) && briefsCovering(p, byBrief).length === 0) set.add(p);
  }
  return [...set].sort();
}
