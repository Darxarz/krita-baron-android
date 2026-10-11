# Krita Baron Edition Android 0.1.37

- Move the official connection, account/browser login, author page link and
  updates to https://orchestrion.ink, keeping custom ComfyUI and Acly cloud
  connections unchanged.
- Migrate the saved official website address and re-seal the existing device
  credential for the new domain using Android Keystore.
- Choosing a LoRA in the gallery appends `<lora:folder/name>` at the end of the
  current prompt. It preserves the active checkpoint/style and existing LoRA
  weights; it does not add trigger words implicitly or create a style.
- Refresh uploaded LoRAs on gallery opening and every minute, updating completion
  without re-selecting the model or resetting prompt/style settings.
- A separate old-domain feed continues to offer this update to old installations.
  New installations use the new-domain stable-ink.json feed.

The base remains stable Krita 5.3.4, with the same package/signing key and all
features/fonts from 0.1.36. versionCode 5050437, package org.krita.baron.debug.

Validation: native Qt suite 128 passed, zero failed, two optional live/cross-runtime
checks skipped; additional actual panel/catalog/completion test passed. Python
companion 1.53.0-baron.17 available fast suite 509 passed, 111 skipped; Pyright
zero errors. Existing saved Windows device credential was accepted by the new
website, and both website and ComfyUI listed the same 559 installed LoRAs.

The APK is checked for package/version, original signer, 16K alignment, expected
native code and QML/Keystore helpers, public fonts/licenses, and goat icon.
Physical Samsung Android/IME testing remains separate.
