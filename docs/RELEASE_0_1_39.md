# Krita Baron Edition Android 0.1.39 — MOSS-Image, FLUX 3 Klein and the website inbox

- **MOSS-Image**: the workflow engine knows the MOSS architecture (Qwen3.5 9B encoder, Flux 2 VAE/latent, skip-layer guidance on single block 22, model-only LoRAs, no ModelSampling shift). Defaults 30 steps / CFG 3, euler + sgm_uniform. Regional prompts are sent as a MOSS layout JSON prompt. See MOSS_IMAGE.md.
- **FLUX 3 Image Klein**: new architecture with its distilled defaults (16 steps, CFG 1), references and layout boxes. See the FLUX 3 notes in the engine docs.
- **Website inbox ("Send to Krita")**: pictures sent from the Orchestrion gallery or a comic panel open as new documents; **Return to website** sends the edited picture back as a new version. See ORCHESTRION_INBOX.md.
- Control and reference rows are hidden for architectures that have no ControlNet / IP-Adapter.

The website compiler must run the same engine snapshot (431 files, `scripts/verify-engine.py`). The inbox needs no server change.

Krita stable base 5.3.4, package org.krita.baron.debug, versionCode 5050439; original signer retained. Desktop companion 1.53.0-baron.20. Includes the unreleased 0.1.38 Krea 2 Edit V2 changes.

Validation on this build machine: Python fast tests 589 passed, 111 skipped; Pyright zero errors; Ruff unchanged baseline of 68 findings, format check passed; native Qt 132 passed, 2 optional skipped; engine 35 tests passed, snapshot verified. MOSS was generated once on the website's ComfyUI by the server session. The inbox, FLUX 3 and MOSS regions were only tested with mocks; physical Android, live inbox and FLUX 3 inference have not been tested yet.
