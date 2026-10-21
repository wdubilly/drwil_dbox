// Pattern matching shared by run-checks.mjs (deferral at pre-commit) and
// check-control-coverage.mjs (CI coverage): both read the same `paths` of
// .drwil/ia-first.json, so they must interpret them the same way.

/** fnmatch the GitLab/GitHub way: `**` crosses `/`, `*` does not. */
export function globToRegex(glob) {
  let out = "";
  for (let i = 0; i < glob.length; ) {
    if (glob.startsWith("**/", i)) {
      out += "(?:.*/)?";
      i += 3;
    } else if (glob.startsWith("**", i)) {
      out += ".*";
      i += 2;
    } else if (glob[i] === "*") {
      out += "[^/]*";
      i += 1;
    } else {
      out += glob[i].replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
      i += 1;
    }
  }
  return new RegExp(`^${out}$`);
}
