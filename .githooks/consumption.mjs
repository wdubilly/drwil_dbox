#!/usr/bin/env node
// Token consumption per workstream, with nothing to type (QUA-042). Source: Claude Code's session logs
// (~/.claude/projects/<project path, non-alphanumeric characters as "-">/*.jsonl), where each agent
// answer carries its tokens, model, timestamp and the git branch of the moment; one branch per brief
// (`workstream/<brief>`, and `scoping/<brief>` or `cadrage/<brief>` for its scoping) attributes it.
// The logs hold the whole conversation: only the consumption fields are read, never a message content
// is shown or written. Never blocking: without a readable log, it says so and exits 0.
// Usage: node .githooks/consumption.mjs [--since YYYY-MM-DD (UTC)] [--json] [--logs <folder>]
//        node .githooks/consumption.mjs --record <brief> (at closing: one line in .drwil/usage.jsonl)
import { appendFileSync, existsSync, mkdirSync, readdirSync, readFileSync, realpathSync, statSync } from "node:fs";
import { homedir } from "node:os";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

/** Pause beyond which time is no longer counted as active. */
export const PAUSE_MS = 15 * 60 * 1000;

/**
 * Indicative weights, in input-token equivalents (price ratios published by Anthropic: output ×5,
 * cache read ×0.1, cache write ×1.25 for 5 minutes). Prices change: the weighted total is only an order
 * of magnitude, the four categories remain the measure.
 */
export const WEIGHTS = { input: 1, output: 5, cacheRead: 0.1, cacheWrite: 1.25 };

const USAGE = ".drwil/usage.jsonl";

const TEXTS = {
  fr: {
    columns: ["chantier", "requêtes", "entrée", "sortie", "cache lu", "cache écrit", "cache lu/req", "pondéré", "durée", "modèles"],
    weighted: "pondéré : indicatif, en équivalent tokens d'entrée (sortie ×5, cache lu ×0,1, cache écrit ×1,25).",
    noLogs: (d) =>
      `consommation : aucun journal de session lisible dans ${d} (Claude Code seulement) — non mesuré ; autre outil IA : estimer à la main (docs/recipes/track-consumption-per-increment.md)`,
    noAnswer: (d) => `consommation : aucune réponse d'agent dans ${d} pour cette période ou ce chantier — non mesuré`,
    badDate: "consommation : date de --since illisible (attendu AAAA-MM-JJ)",
    noBrief: "consommation : --record attend le nom de la fiche (docs/projects/<fiche>.md, sans extension)",
    unreadable: (n) => `consommation : ${n} ligne(s) de journal illisible(s), ignorée(s)`,
    already: (b) => `consommation : ${USAGE} a déjà une ligne pour « ${b} » — rien d'ajouté`,
    recorded: (b, t, c) => `consommation : « ${b} » consigné dans ${USAGE} (${t} tokens pondérés, cache lu moyen par requête : ${c})`,
    noBranch: "(sans branche)",
    main: "(branche principale)",
    branch: (b) => `(branche ${b})`,
  },
  en: {
    columns: ["workstream", "requests", "input", "output", "cache read", "cache write", "cache read/req", "weighted", "time", "models"],
    weighted: "weighted: indicative, in input-token equivalents (output ×5, cache read ×0.1, cache write ×1.25).",
    noLogs: (d) =>
      `consumption: no readable session log in ${d} (Claude Code only) — not measured; another AI tool: estimate by hand (docs/recipes/track-consumption-per-increment.md)`,
    noAnswer: (d) => `consumption: no agent answer in ${d} for this period or workstream — not measured`,
    badDate: "consumption: unreadable --since date (expected YYYY-MM-DD)",
    noBrief: "consumption: --record expects the brief's name (docs/projects/<brief>.md, without extension)",
    unreadable: (n) => `consumption: ${n} unreadable log line(s), ignored`,
    already: (b) => `consumption: ${USAGE} already has a line for "${b}" — nothing added`,
    recorded: (b, t, c) => `consumption: "${b}" recorded in ${USAGE} (${t} weighted tokens, average cache read per request: ${c})`,
    noBranch: "(no branch)",
    main: "(main branch)",
    branch: (b) => `(branch ${b})`,
  },
};

/** Logs folder of a project: absolute path, non-alphanumeric characters replaced by "-". */
export function logsFolder(root, env = process.env) {
  const base = env.CLAUDE_CONFIG_DIR ?? join(homedir(), ".claude");
  return join(base, "projects", resolve(root).replace(/[^A-Za-z0-9]/g, "-"));
}

/**
 * Agent answers of a log text: [{ key, time, branch, model, usage }]. Only these fields are extracted;
 * an unreadable line is counted, never guessed.
 */
export function readLog(text) {
  const answers = [];
  let unreadable = 0;
  for (const line of text.split("\n")) {
    if (!line.trim()) continue;
    let j;
    try {
      j = JSON.parse(line);
    } catch {
      unreadable++;
      continue;
    }
    const usage = j?.message?.usage;
    // "<synthetic>": a message produced by the tool itself (error, interruption), without a model call.
    if (j?.type !== "assistant" || !usage || !j.timestamp || j.message.model === "<synthetic>") continue;
    answers.push({
      key: j.requestId ?? j.message.id ?? j.uuid,
      time: Date.parse(j.timestamp),
      branch: j.gitBranch || null,
      model: j.message.model ?? "unknown",
      usage: {
        input: usage.input_tokens ?? 0,
        output: usage.output_tokens ?? 0,
        cacheRead: usage.cache_read_input_tokens ?? 0,
        cacheWrite: usage.cache_creation_input_tokens ?? 0,
      },
    });
  }
  return { answers, unreadable };
}

/** Group of a branch: the brief for a workstream or scoping branch, otherwise the branch itself. */
export function group(branch, T = TEXTS.en) {
  if (!branch) return T.noBranch;
  if (["master", "main"].includes(branch)) return T.main;
  // `chantier/` and `cadrage/`: names of the branches of the French-named versions.
  const prefix = ["workstream/", "scoping/", "chantier/", "cadrage/"].find((p) => branch.startsWith(p));
  return prefix ? branch.slice(prefix.length) : T.branch(branch);
}

/** Active time: sum of the gaps between successive answers, without the pauses longer than PAUSE_MS. */
export function activeTime(times) {
  const t = [...times].sort((a, b) => a - b);
  let total = 0;
  for (let i = 1; i < t.length; i++) if (t[i] - t[i - 1] <= PAUSE_MS) total += t[i] - t[i - 1];
  return total;
}

/**
 * Aggregates answers: a request counted once (a log repeats an answer over several blocks, each with
 * the request's usage). Returns [{ workstream, requests, input, output, cacheRead, cacheWrite,
 * cacheReadPerRequest, weighted, models, minutes }], most weighted first. The average cache read per
 * request shows the cost of the context read again, which grows with a long session.
 */
export function aggregate(answers, { since = null, T = TEXTS.en } = {}) {
  const seen = new Set();
  const groups = new Map();
  for (const r of answers) {
    if (since !== null && r.time < since) continue;
    if (seen.has(r.key)) continue;
    seen.add(r.key);
    const name = group(r.branch, T);
    const g = groups.get(name) ?? { requests: 0, input: 0, output: 0, cacheRead: 0, cacheWrite: 0, models: new Set(), t: [] };
    groups.set(name, g);
    g.requests++;
    for (const k of Object.keys(WEIGHTS)) g[k] += r.usage[k];
    g.models.add(r.model);
    g.t.push(r.time);
  }
  return [...groups]
    .map(([name, g]) => ({
      workstream: name,
      requests: g.requests,
      input: g.input,
      output: g.output,
      cacheRead: g.cacheRead,
      cacheWrite: g.cacheWrite,
      cacheReadPerRequest: Math.round(g.cacheRead / g.requests),
      weighted: Math.round(Object.entries(WEIGHTS).reduce((s, [k, w]) => s + g[k] * w, 0)),
      models: [...g.models].sort(),
      minutes: Math.round(activeTime(g.t) / 60000),
    }))
    .sort((a, b) => b.weighted - a.weighted || a.workstream.localeCompare(b.workstream));
}

const readable = (n, fr) => {
  const s = n >= 1e6 ? `${(n / 1e6).toFixed(1)} M` : n >= 1e3 ? `${Math.round(n / 1e3)} k` : String(n);
  return fr ? s.replace(".", ",") : s;
};

/** Readable table of the measure. */
export function format(rows, T = TEXTS.en) {
  const fr = T === TEXTS.fr;
  const body = rows.map((l) => [
    l.workstream,
    String(l.requests),
    readable(l.input, fr),
    readable(l.output, fr),
    readable(l.cacheRead, fr),
    readable(l.cacheWrite, fr),
    readable(l.cacheReadPerRequest, fr),
    readable(l.weighted, fr),
    `${l.minutes} min`,
    l.models.join(", "),
  ]);
  const widths = T.columns.map((e, i) => Math.max(e.length, ...body.map((c) => c[i].length)));
  const line = (c) =>
    c
      .map((v, i) => (i === 0 || i === c.length - 1 ? v.padEnd(widths[i]) : v.padStart(widths[i])))
      .join("  ")
      .trimEnd();
  return [line(T.columns), ...body.map(line), "", T.weighted].join("\n");
}

/** The usage.jsonl line of a workstream, in the recipe's format (dashboard: workstream, tokens, model…). */
export function usageLine(row, date) {
  return {
    workstream: row.workstream,
    increment: "all",
    tokens: row.weighted,
    input: row.input,
    output: row.output,
    cache_read: row.cacheRead,
    cache_write: row.cacheWrite,
    requests: row.requests,
    model: row.models.length === 1 ? row.models[0] : row.models.join(", "),
    duration_min: row.minutes,
    date,
    source: "claude-code-logs",
  };
}

const jsonlFiles = (folder) =>
  readdirSync(folder).flatMap((name) => {
    const path = join(folder, name);
    if (statSync(path).isDirectory()) return jsonlFiles(path);
    return name.endsWith(".jsonl") ? [path] : [];
  });

/** Reads the arguments and the logs, prints or records; returns the exit code. */
export function main(args, { env = process.env, root = process.cwd(), log = console.log, err = console.error, now = new Date() } = {}) {
  let lang = "fr";
  try {
    lang = JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8"))?.lang === "en" ? "en" : "fr";
  } catch {}
  const T = TEXTS[lang];
  const opt = { json: false, since: null, logs: null, record: null };
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--json") opt.json = true;
    else if (args[i] === "--since") opt.since = Date.parse(args[++i]);
    else if (args[i] === "--logs") opt.logs = args[++i];
    else if (args[i] === "--record") opt.record = args[++i] ?? "";
  }
  if (Number.isNaN(opt.since)) {
    err(T.badDate);
    return 2;
  }
  if (opt.record !== null && !/^\w[\w.-]*$/.test(opt.record)) {
    err(T.noBrief);
    return 2;
  }
  const folder = opt.logs ?? logsFolder(root, env);
  // Without a log, nothing is measured: never "0 token" — but never blocking either.
  if (!existsSync(folder)) {
    err(T.noLogs(folder));
    return 0;
  }
  const answers = [];
  let unreadable = 0;
  for (const f of jsonlFiles(folder)) {
    const r = readLog(readFileSync(f, "utf8"));
    answers.push(...r.answers);
    unreadable += r.unreadable;
  }
  const rows = aggregate(answers, { since: opt.since, T }).filter((r) => opt.record === null || r.workstream === opt.record);
  if (unreadable && !opt.json) err(T.unreadable(unreadable));
  if (!rows.length) {
    err(T.noAnswer(folder));
    return 0;
  }
  if (opt.record !== null) {
    const path = join(root, USAGE);
    const lines = existsSync(path) ? readFileSync(path, "utf8").split("\n") : [];
    const known = lines.some((l) => {
      try {
        return JSON.parse(l).workstream === opt.record;
      } catch {
        return false;
      }
    });
    if (known) {
      log(T.already(opt.record));
      return 0;
    }
    mkdirSync(join(root, ".drwil"), { recursive: true });
    appendFileSync(path, `${JSON.stringify(usageLine(rows[0], now.toISOString().slice(0, 10)))}\n`);
    log(T.recorded(opt.record, rows[0].weighted, readable(rows[0].cacheReadPerRequest, lang === "fr")));
    return 0;
  }
  log(opt.json ? JSON.stringify({ version: 1, unreadable, workstreams: rows }, null, 2) : format(rows, T));
  return 0;
}

// Through a symbolic link (an installed copy), argv[1] is the link: compare the real paths.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  process.exitCode = main(process.argv.slice(2));
}
