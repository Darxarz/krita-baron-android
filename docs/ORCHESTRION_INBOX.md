# Orchestrion inbox ("Send to Krita")

Pictures sent from the website with **Send to Krita** (gallery, comic panel picture) open on the tablet as new Krita documents. **Return to website** sends the edited picture back. The Windows plugin implements the same contract.

- `OrchestrionInbox` (native) starts after an Orchestrion sign-in when `GET /api/krita/info` lists `inbox:1`. It long-polls `GET /api/krita/inbox?since=<cursor>&wait=25`, re-reads the full list after errors and every 30 minutes, and applies `removed` ids.
- New items are claimed with `POST /api/krita/inbox/:id/ack` (`deviceId` stored in QSettings `inboxDeviceId`), downloaded only from the website origin (`imageUrl` must be `/api/krita/inbox/<id>/image`), verified by size and SHA-256 off the GUI thread, and opened with `CanvasHost::openDocument` (Krita `KisPart` + `KisMainWindow::addViewAndNotifyLoadingCompleted`).
- Items already open on another device are skipped automatically; choosing one in the **Inbox** menu asks before taking it over (`force`).
- The new document carries the `ai_diffusion/orchestrion_inbox` annotation (item id, title, return target, website origin), shared with the Windows plugin.
- **Return to website** flattens the visible image, encodes PNG in the background and posts it to `/api/krita/inbox/:id/return?width=&height=` with `Idempotency-Key` = first 48 hex digits of SHA-256(item id, 0, PNG). Retrying the same picture never creates a duplicate; a changed picture becomes a new version.
- The device token is only sent as `Authorization: Bearer` to the website origin; redirects are not followed.

Automatic opening can be disabled with the QSettings key `inboxAutoOpen=false`.
