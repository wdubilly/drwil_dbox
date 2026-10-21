# Recipe: manage access

*(template to fill in by the project if an external identity provider is
used, e.g. Keycloak, Auth0… — otherwise this recipe does not apply and can
be removed)*

Authentication and roles model: `docs/security.md`, section
"Authentication and roles" (if present). Change a role or a permission:
`docs/recipes/modify-permissions.md`.

## Common operations

- **Grant access**: add the person to the intended group or role.
- **First login / forgotten password**: identity provider's procedure.
- A role change, an account deactivation or a closed session must take
  effect without waiting for the person to log back in, or at worst within
  an announced delay.
- **Test accounts** on a dev workstation: never in production.
- **Login and admin action history**: to be kept and reviewed without
  copying secrets (passwords, TOTP) into the logs.
