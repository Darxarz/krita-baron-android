# 0.1.32 — Separate generation and editing styles

Generation and instruction editing now remember separate style selections and sampling
parameters. Each generation preset remembers its editing counterpart. When no linked
editing preset exists, switching to editing creates an independent copy of the preset.
Existing manually selected models continue to work without requiring a preset.

The Styles page now includes a generation/editing selector, searchable preset chooser,
linked editing style, server VAE choices, model architecture information, sampler
presets, style LoRA enable/disable and strength, and export. Clip skip and preferred
resolution show “Model default” when their overrides are zero. Krea 2 reference options
are saved with styles and appear only for Krea models. Empty LoRA controls are hidden.
Controls use the active Krita theme and the settings pages support touch scrolling.

Edits save automatically; the existing Save Style action is retained. Imported preset
fields, fallback checkpoints, advanced sampler fields, and live sampler settings that
are not represented by native controls are preserved on save. Preset exports use JSON.
The native Styles page is more complete; this release does not claim complete parity
with every Python style editor control or implement the Live workspace.

Document annotations now include both style banks and their preset definitions. A
project can restore missing native presets on another installation. Existing local
presets with the same ID take precedence, and older documents/settings are migrated
without requiring new fields. Generation history restore chooses its mode before its
style so the saved model and sampling parameters remain authoritative.

Validation: native Qt tests on Windows and Linux with AddressSanitizer; targeted
coverage for linked styles, independent edits, restart, multiple documents, recovery
of presets embedded in a project, and automatic saving without losing imported fields.
APK packaging, version, signing certificate, and matching GPL source archive are
verified before publication. Actual Samsung Android 16 interaction is not tested here.

Android version: 5.4.0-baron.0.1.32, version code 5050432. Package/signing key unchanged.
Windows plugin remains 1.53.0-baron.14. No website frontend or compiler change required.
