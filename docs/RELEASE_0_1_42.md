# Krita Baron Edition Android 0.1.42 — website inbox fixes

- **Pictures sent from the website are visible on the canvas.** The new document's layer was created with the opacity of a transparent background colour (0%), so only the layer thumbnail showed the picture and later strokes. The layer is now fully opaque, as in Krita's "New image from clipboard", and the document is marked as unsaved so Krita asks before closing it.
- **A picture from the website brings its settings**: prompt (installed LoRAs appended with their weights, missing ones skipped), negative prompt, model, steps, CFG, sampler and scheduler are filled in once when the document opens. Later changes are kept.
- Includes the Seamless fix from 0.1.41 and the Kadance prompt assistant from 0.1.40.

Krita stable base 5.3.4, package org.krita.baron.debug, versionCode 5050442; original signer retained. Desktop companion 1.53.0-baron.25. Needs the website's extended inbox meta (deployed).

Validation: native Qt 136 passed, 2 optional skipped. Not yet tested on a physical Android device.
