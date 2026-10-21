# Recipe: back up and restore production

*(template to fill in by the project — what to back up, real scripts,
schedule)*

Server context and pitfalls: `docs/deployment.md` if present.

## What is backed up

| What | Where | Content | Format |
|---|---|---|---|
*(one line per piece of data to back up: database, files, configuration…)*

Every backup must be **verified** before being kept (an unreadable copy is
useless) and access-protected if it contains secrets or personal data.

**Deliberately not backed up** *(to list: secrets that must never be in an
automatic backup — SEC-007 — and anything that regenerates on its own at
startup)*.

## Set up automatic backups (once)

*(to describe: schedule, location, retention period, where an off-server
copy lives)*

## Restore

*(step-by-step procedure; specify that nothing is destroyed by the restore
until the previous state has been confirmed safe to discard)*

## Verify the restore works (drill)

*(a periodic restore drill, on a throwaway stack, is the only proof that a
backup actually serves any purpose)*
