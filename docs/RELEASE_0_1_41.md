# Krita Baron Edition Android 0.1.41 — Seamless inpaint fix

- **Seamless** (custom inpaint) could look permanently enabled and refuse to be unchecked. With the Orchestrion website catalog, model families arrive as names such as "Flux", "SD 1.5" or "Z-Image", which did not match the internal ids, so the option was greyed out for models that support it while staying ticked. It is now enabled for SD 1.5, SDXL/Illustrious/Pony, Flux, Z-Image, Qwen Image and Anima (as in the Windows plugin), and shown unticked and disabled, with an explanation, for models without an inpaint model (Krea 2, FLUX 2/3 Klein, Qwen Image 2.1, MOSS…). The user's choice is kept and comes back on a supported model.
- One model-id normalizer is shared by inpaint, control/reference and upscale settings, so website family names, website `arch` ids and engine ids are understood the same way (for example, Z-Image now gets its tile/unblur upscale guidance and natural-language prompt ordering).
- Includes the Kadance prompt assistant from 0.1.40.

Krita stable base 5.3.4, package org.krita.baron.debug, versionCode 5050441; original signer retained. No server engine change.

Validation: native Qt 135 passed, 2 optional skipped. Not yet tested on a physical Android device.
