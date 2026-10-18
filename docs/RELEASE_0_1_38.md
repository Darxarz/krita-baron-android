# Krita Baron Edition Android 0.1.38 — Krea 2 Edit V2 evaluation

Adds explicit Identity Edit V2 and General Edit V2 reference purposes and reference placement controls in Styles. Placement is preserved in document settings, generation history, separate prompt banks, and imported/exported Python styles. Legacy Identity/Style purposes retain their previous behavior.

The workflow engine selects one matching functional adapter, rejects mismatched/combined adapters, verifies the V2 node inputs before submission, and wires scene/subject placement plus the shared sampler target latent. No model weights are included in the APK. See KREA2_EDIT_V2.md for setup and license boundaries.

Krita stable base 5.3.4, package org.krita.baron.debug, versionCode 5050438; original signer retained. Desktop companion 1.53.0-baron.19.

Validation: 525 available Python fast tests passed, 111 skipped; full CI invocation: 531 passed, 111 skipped, 7 local-server fixture errors. Workflow inference suite: 8 graph tests passed, then local ComfyUI connection refused. Pyright: zero errors. Ruff check: unchanged baseline of 68 existing findings, format check passed. Native Qt: 129 passed, 2 optional skipped. Native server engine: 24 passed. APK package/version, signer, 16K ZIP alignment, native version, Qt helpers, 96 public fonts/licenses, and branding verified. Physical Android and live V2 inference have not been tested.

The new weights and node pack are installed in a separate personal evaluation directory. The shared ComfyUI/site model catalog is unchanged. The private instance must be started on its server host, then accessed directly on port 8190.
