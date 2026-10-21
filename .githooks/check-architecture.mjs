#!/usr/bin/env node
// The architecture map stays current (QUA-040): drwil maintains the project's map
// (docs/architecture.md, a project file `init` never rewrites) by reporting what it does not cite yet.
// - By default, every folder holding code (the project's code prefixes and layers, two levels deep).
// - Setting `architectureMap.files`: path prefixes under which every file (tests excluded) is cited.
// Each gap comes with the line to complete; the role stays to be written by the agent.
// Requirement added after projects were already equipped: blocking only if the project has it
// (`newRequirements`: "blocking", default of fresh installs), otherwise reported without failing.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, realpathSync } from "node:fs";
import { basename, join, posix } from "node:path";
import { fileURLToPath } from "node:url";
import { isCode } from "./scope.mjs";

const TEST_FILE = /(\.test\.|\.spec\.|_test\.|(^|\/)(tests?|__tests__)\/)/;
const SOURCE = /\.(m?[jt]sx?|cjs|py|go|rs|rb|php|java|kt|cs|swift|dart|sh)$/;

const TEXTS = {
  fr: {
    gap: (p, map) => `\`${p}\` absent de la carte ${map} : l'y ajouter avec son rôle (« \`${p}\` — … »)`,
    summary: (n, p) => `carte d'architecture : ${n} élément(s) attendu(s), ${p} absent(s)`,
    warning: `carte d'architecture : exigence récente, non bloquante (réglage « newRequirements » : warning ; « blocking » pour l'imposer)`,
    noMap: (map) => `carte d'architecture : ${map} absente, rien à vérifier`,
  },
  en: {
    gap: (p, map) => `\`${p}\` missing from the map ${map}: add it with its role (« \`${p}\` — … »)`,
    summary: (n, p) => `architecture map: ${n} expected item(s), ${p} missing`,
    warning: `architecture map: recent requirement, not blocking (setting "newRequirements": warning; "blocking" to enforce it)`,
    noMap: (map) => `architecture map: ${map} missing, nothing to check`,
  },
};

/** What the map must cite, from the tracked files: code folders (two levels) and, under `files`, every non-test file. */
export function expected(tracked, cfg) {
  const items = new Set();
  for (const f of tracked) {
    const dir = posix.dirname(f).split("/").slice(0, 2).join("/");
    if (dir !== "." && isCode(f, cfg)) items.add(`${dir}/`);
  }
  for (const prefix of cfg.architectureMap?.files ?? [])
    for (const f of tracked) if (f.startsWith(prefix) && SOURCE.test(f) && !TEST_FILE.test(f)) items.add(f);
  return [...items].sort();
}

/** The expected items the map does not cite (a folder with or without its slash; a file by its path or its name in backticks). */
export function gaps(map, items) {
  return items.filter((p) =>
    p.endsWith("/") ? !map.includes(p) && !map.includes(`\`${p.slice(0, -1)}\``) : !map.includes(p) && !map.includes(`\`${basename(p)}\``),
  );
}

/** Runs the check on `root`; returns the exit code. */
export function main(root = process.cwd(), log = console.log) {
  let cfg = {};
  try {
    cfg = JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8")) ?? {};
  } catch {}
  const T = TEXTS[cfg.lang === "en" ? "en" : "fr"];
  const file = cfg.architectureMap?.file ?? "docs/architecture.md";
  if (!existsSync(join(root, file))) {
    log(T.noMap(file));
    return 0;
  }
  const git = spawnSync("git", ["ls-files", "-z"], { cwd: root, encoding: "utf8" });
  const tracked = git.status === 0 ? git.stdout.split("\0").filter(Boolean) : [];
  const items = expected(tracked, cfg);
  const missing = gaps(readFileSync(join(root, file), "utf8"), items);
  const blocking = cfg.newRequirements === "blocking";
  for (const p of missing) log(`  ${blocking ? "✗" : "⚠"} ${T.gap(p, file)}`);
  log(T.summary(items.length, missing.length));
  if (missing.length && !blocking) log(T.warning);
  return missing.length && blocking ? 1 : 0;
}

// Through a symbolic link (an installed copy), argv[1] is the link: compare the real paths.
const direct = process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url));
if (direct) process.exitCode = main();
