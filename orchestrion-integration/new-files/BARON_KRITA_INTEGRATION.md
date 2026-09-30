# Orchestrion × Krita Baron integration

Prepared on 2026-09-30, branch `baron/krita-orchestrion`, base `ca6a0fd`.
Isolated checkout: `D:\orchestrion-baron-integration`.
Production checkout: `Z:\orchestrator`, branch `site-improvements`, with unrelated
Claude/user changes. No production code, database, configuration or running service
was modified by this integration. Nothing committed, pushed or deployed.

## Behavior

Krita uses a dedicated browser sign-in flow. A named website account explicitly
approves a manually entered code. The device receives a separately revocable,
hashed API key, valid for 30 days and restricted to Krita endpoints. Passwords stay
in the website login flow; Krita stores its credential with Windows DPAPI. The
consent page supports all 14 plugin languages. Ordinary account/password session
revocation also revokes Krita device keys.

The new `/krita/connection` HTTP and WebSocket transport authenticates via headers,
preserving the existing prompt/file ownership, reservation and settlement logic.
The old `/krita/:token` transport remains available for existing clients.
`/api/krita/account`, `/quote`, `/models` and `/logout` expose minimal integration
data. Prices and balances are in **bleatbucks only**. A quote never purchases,
reserves or debits credits. The plugin estimates a whole batch from dimensions;
actual output billing remains the site's existing responsibility.

Catalog data combines installed ComfyUI models with existing website/LoRA metadata.
The plugin only downloads public HTTPS `image.civitai.com` previews, without device
credentials. Missing metadata produces a placeholder. Unsupported preview hosts
are not loaded. This does not create a model-download or paid-model marketplace.
The native plugin catalog includes installed Qwen21 models even where the website's
own generation picker hides that folder.

## Integration into Claude's current branch

1. Read current repository instructions and inspect production Git/WIP again.
   This patch is based on `ca6a0fd`, not on the current uncommitted production files.
2. Merge these source changes into an isolated checkout of the current site branch.
   Review conflicts in `userPortalServer.js` rather than overwriting that file.
   A companion patch and source bundle are provided in the plugin repository.
3. Run the new integration tests plus existing proxy guard and auth security tests
   with `NODE_ENV=test`, `DB_STORAGE=:memory:`. Never point tests at the production DB.
4. Build `user-portal` normally to include the API-key manager change. The standalone
   `user-portal/krita-connect/` files are served directly and must also be deployed.
   The review build directory and local dependency symlinks are not deployment files.
5. Use the site's existing staging/deployment/restart procedure only when rollout is
   authorized. No new database migration, exchange-rate service or payment-package
   configuration is needed. Refresh the page to avoid an old cached portal bundle.
6. Smoke-test with a named staging account: consent, code expiry, connect, revoke,
   quote, gallery, an owned generation result and denied access to unrelated APIs.
   Revoke keys after testing. Runtime generation through the production proxy was
   not tested by this change and must not be claimed as production acceptance.

## Verification

- 50 Jest tests passed across `kritaIntegration`, `kritaProxyGuard`, `authSecurity`.
- Frontend review build succeeded (2082 modules). Its existing large-chunk warning
  remains; the build is not deployed.
- Real plugin Qt HTTP/WS transport passed against an isolated fixture; only metadata
  GETs were forwarded to ComfyUI at `192.168.50.191:8188`. Uploads remained local,
  zero GPU jobs were submitted. Qwen21 and Krea2 were recognized among 47 models.
- Consent page browser approval, RU/EN language switching, redemption and revocation
  passed with a fake local account. No production account or balance was used.

Pending device approvals are in memory: a server restart invalidates pending codes,
so restart the login. Issued device keys persist in the existing API-key table.
The flow is a custom JSON device flow; it is not an assertion of full OAuth compliance.
Plugin screenshots used a Qt5/mock Krita document; real Krita installation and
production proxy generation remain separate acceptance steps.
