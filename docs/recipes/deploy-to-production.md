# Recipe: deploy to production

*(template for the project to complete — real server, real scripts)*

Server context and pitfalls: `docs/deployment.md` if present. Deploy only
on explicit request.

1. On the dev machine: green checks, commit pushed to the main branch.
2. On the server: fetch the new version, rebuild, restart.
3. Check that the service answers (e.g. a health route, an expected HTTP
   code).
4. If a service fails: read its logs; list here the causes already met
   over time.
