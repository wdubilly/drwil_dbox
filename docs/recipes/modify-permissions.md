# Recipe: change roles or permissions

*(template to fill in by the project once the stack is chosen — where
permissions live, how the front reflects them without being their source of
truth)*

1. Where roles and permissions live server-side (source of truth: it
   decides, never the front alone — see `docs/contracts.md`).
2. Keep any front-end mirror aligned (e.g. a fake test API): otherwise
   front-end tests check something other than reality.
3. A new role in the identity provider (e.g. Keycloak, a realm or
   equivalent): add it to the versioned configuration if there is one, and
   document it in `docs/security.md` if present.
4. Tests: the expected rule server-side, the screens visible per role
   front-end.
5. Any opening of access towards production must be explicitly flagged in
   the commit message.
