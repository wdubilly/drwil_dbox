// QUA-062: the YAML header of every shipped skill reads in strict YAML. Some tools parse it strictly
// (Copilot CLI) and silently drop a skill whose header does not: a plain value holding ": " is the usual cause.
// No YAML library in a shipped hook: only the shapes a header uses are checked (`key: value`, indented continuation).
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";

const T = {
  fr: {
    open: "en-tête YAML absent (le fichier doit commencer par ---)",
    close: "en-tête YAML jamais refermé (--- manquant)",
    line: (n, l) => `ligne ${n} : ni « clé: valeur » ni suite indentée « ${l} »`,
    colon: (n, k) => `ligne ${n} : la valeur de « ${k} » contient « : » ou « # » : la mettre entre guillemets`,
    indicator: (n, k) => `ligne ${n} : la valeur de « ${k} » commence par un caractère réservé du YAML : la mettre entre guillemets`,
    quote: (n, k) => `ligne ${n} : la valeur de « ${k} » a un guillemet mal refermé`,
    where: (f, p) => `${f} : ${p}`,
  },
  en: {
    open: "missing YAML header (the file must start with ---)",
    close: "YAML header never closed (missing ---)",
    line: (n, l) => `line ${n}: neither "key: value" nor an indented continuation "${l}"`,
    colon: (n, k) => `line ${n}: the value of "${k}" holds ": " or " #": quote it`,
    indicator: (n, k) => `line ${n}: the value of "${k}" starts with a YAML reserved character: quote it`,
    quote: (n, k) => `line ${n}: the value of "${k}" has an unclosed quote`,
    where: (f, p) => `${f}: ${p}`,
  },
};

const KEY_RE = /^([A-Za-z][\w-]*):(?:\s+(.*))?$/;
const RESERVED_START_RE = /^[&*!|>%@`]/;

/** Problems of a skill file's header (texts in `lang`), [] when it reads in strict YAML. */
export function headerProblems(text, lang = "fr") {
  const t = T[lang] ?? T.fr;
  const lines = text.split(/\r?\n/);
  if (lines[0] !== "---") return [t.open];
  const end = lines.indexOf("---", 1);
  if (end === -1) return [t.close];
  const problems = [];
  lines.slice(1, end).forEach((line, i) => {
    const n = i + 2;
    if (!line.trim() || /^\s/.test(line) || line.startsWith("#")) return;
    const m = KEY_RE.exec(line);
    if (!m) {
      problems.push(t.line(n, line));
      return;
    }
    const value = (m[2] ?? "").trim();
    if (!value) return;
    const quote = value[0];
    if (quote === '"' || quote === "'") {
      if (value.length < 2 || value.at(-1) !== quote) problems.push(t.quote(n, m[1]));
    } else if (value[0] === "[" || value[0] === "{") {
      return; // flow collection: written as the author meant it
    } else if (RESERVED_START_RE.test(value)) problems.push(t.indicator(n, m[1]));
    else if (/:(\s|$)|\s#/.test(value)) problems.push(t.colon(n, m[1]));
  });
  return problems;
}

/** Errors for every `<root>/.claude/skills/<skill>/SKILL.md` whose header does not read in strict YAML. */
export function skillHeaderProblems(root, lang = "fr") {
  const base = join(root, ".claude", "skills");
  if (!existsSync(base)) return [];
  const t = T[lang] ?? T.fr;
  return readdirSync(base, { withFileTypes: true })
    .filter((e) => e.isDirectory() && existsSync(join(base, e.name, "SKILL.md")))
    .flatMap((e) => {
      const rel = `.claude/skills/${e.name}/SKILL.md`;
      return headerProblems(readFileSync(join(base, e.name, "SKILL.md"), "utf8"), lang).map((p) => t.where(rel, p));
    });
}
