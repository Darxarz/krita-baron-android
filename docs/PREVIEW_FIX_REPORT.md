# Model preview fix — 2026-09-30

Android preview 0.1.1 and Windows plugin 1.53.0-baron.4 fix the Civitai gallery
loader. No website deployment, server restart or GPU job is required.

The real catalog's original-image URLs returned HTTP 301 to
`https://blobs-b2.civitai.com/`. The previous Android loader used manual redirect
handling but never followed that response. Both clients now request a 320-pixel
JPEG and follow at most three redirects within the two exact public Civitai
hosts. Device credentials are never used by the separate thumbnail transport.
Private addresses, foreign hosts, HTTP downgrades and URL credentials remain
excluded. Downloads stop at 5 MiB, and image dimensions are checked before
decoding. Android keeps up to 8 MiB of successful thumbnails in memory and retries
failed cards when reopening the gallery. Obsolete catalog replies cannot replace
a newer model's picture.

A real Sung Jin Woo catalog URL downloaded a 31,807-byte 320 × 480 JPEG using the
new Windows Qt downloader, compared with a 2,193,804-byte original. The native Qt
loader also fetched and decoded a real Civitai URL through the CDN redirect.

Verification:

- Native Qt tests: 22 passed on Linux, 22 on Windows; the optional live test is
  skipped in the regular suite and passed separately on Linux.
- Python preview and Orchestrion tests: 35 passed.
- Desktop CI checks: 400 passed, 111 skipped; the six non-server client cases
  passed separately. Seven client cases need the missing local test ComfyUI
  installation and were excluded from this fast rerun after their setup failures.
- Pyright: zero errors/warnings. Changed Python files pass Ruff; the complete
  existing tree has 75 pre-existing Ruff findings. All 108 Python files are
  formatted. Workflow/installer/cloud code was not changed by this fix.
- Windows ZIP integrity and exact packaged source verified.
- Android ARM64 APK rebuilt; current CDN loader strings, lack of a Python
  interpreter, Keystore helper and signing certificate verified.

The new APK still needs confirmation on the user's actual tablet. The previously
reported successful Android sign-in does not establish generation acceptance.
Install 0.1.1 over the existing Baron APK; it retains the same package identity
and debug signing key. Import the Baron .4 ZIP in desktop Krita and restart Krita.
Server workflow engine snapshots are unchanged.

These source changes remain local working-tree changes over
`d4ff3ba5a53dd1870b3ed795b3650b52f0ac206e`; a complete updated source ZIP is supplied
with the preview-fix artifacts. No new source commit or production deployment was
made during this fix.
