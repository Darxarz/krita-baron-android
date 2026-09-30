# Website review bundle

This is a source review bundle, not a deployment package.

Source checkout: `D:\orchestrion-baron-integration`.
Branch: `baron/krita-orchestrion`.
Baseline: `ca6a0fd`.

- `tracked.patch`: changes to already tracked website files.
- `new-files/`: new integration route, device service, native compiler adapter,
  tests and browser consent assets.
- Native server engine: `../server/` in this repository; configure its absolute
  Python/compiler paths as described in `../docs/SERVER.md`.

Includes the earlier desktop browser login, bleatbucks prices and model metadata
integration, plus native workflow preparation and prefilled browser approval codes.
Codes are never approved automatically. New-file tests use fakes/in-memory data.

After the final native changes, 54 tests passed across `kritaIntegration`,
`kritaNative`, `kritaProxyGuard` and `authSecurity`. The prior desktop handoff in
`new-files/BARON_KRITA_INTEGRATION.md` describes its earlier verification separately.

Production `Z:\orchestrator` has not been modified or restarted. Do not copy a
complete userPortalServer file over Claude's work; review/apply the patch against
the matching baseline and resolve changes on an isolated branch first.
