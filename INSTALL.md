# Installation by someone else

Prerequisites, clone/setup steps and first startup, to be detailed by the
project once the stack has been chosen.

## 1. Prerequisites

*(tools to install: runtime, package manager, Docker…)*

## 2. Clone and configure

*(repo to clone, config file to fill in, e.g. `.env` from `.env.example`)*

Once per clone, enable the governance hooks (checks at commit and push):

```bash
git config core.hooksPath .githooks
```

## 3. First startup and check

Run and check: `docs/recipes/run-locally.md` (if present).

## 4. Using an AI tool other than Claude

`AGENTS.md` is the shared entry point; tool-specific files (e.g.
`CLAUDE.md`, `GEMINI.md` — if present) only point to it. A new tool: have
it read `AGENTS.md` first, without duplicating its content.
