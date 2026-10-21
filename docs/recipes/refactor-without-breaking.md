# Recipe: refactor without breaking anything

Stack-independent method: characterisation tests first, small steps, no
change of behaviour.

1. **Characterisation tests first**, on the current code: what the user
   sees (roles, texts, buttons), the calls sent (bodies included), the
   error cases. Commit them alone, green, before touching the code.
2. Split in small steps, one commit each, checks green every time
   (`node .githooks/run-checks.mjs`): extract the pure logic first (with
   its tests), then the rest. A file above a reasonable ceiling is a signal
   to split: the kit ships an opt-in guard (`.githooks/check-file-size.mjs`,
   inherited ceilings in `.githooks/check-file-size.legacy.json` — they can
   only go down) and an empty extension point for the project's own hygiene
   rules (`.githooks/check-code-rules.mjs`); declare them in
   `.drwil/ia-first.json → checks` once the project is ready to adopt them.
3. No change of behaviour. A bug found on the way: separate commit with
   its test, reported (`AGENTS.md`, Conduct section).
4. **Pitfalls already met** *(generic JS/TS examples, for the project to
   complete as refactors go)*:
   - state held by an unmounted component is lost when the screen changes
     → lift it into a parent component or a stable hook;
   - a value used before its declaration (temporal dead zone);
   - a type import mixed with a value import: separate them with
     `import type` when the name is only used as a type.
5. Also check the production build, and if possible a manual pass in the
   app over the touched path.
