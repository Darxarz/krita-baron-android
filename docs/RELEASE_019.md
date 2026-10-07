# Krita Baron Edition 0.1.9

This is the same unofficial Krita 5.4.0-prealpha base as 0.1.8, with the same
Android package and signing identity. Install over the previous Baron APK.

The Interface settings page follows the Python plugin's row order, descriptions,
right-aligned controls, animated theme-aware switches, horizontal tag file list,
scrolling area and dialog footer. Original Krita refresh/folder icons are bundled.
The System language choice now also handles an old saved empty language correctly.

Connected settings:

- Language (after restart), prompt line count, negative prompt visibility and
  the evaluated/total steps suffix on the strength field.
- Recent styles at the top of the selector, configurable count and persistent MRU.
- Original four tag datasets plus custom CSVs. Refresh rescans the tag folder;
  the folder button opens it on desktop and imports CSV files on Android.
- Finished generation: do nothing, preview, or apply. New automatic previews
  preserve a manually selected preview already on the canvas.
- Apply as a new top layer, above the active layer, or replace an unlocked paint
  layer. Replacement uses a Krita paint transaction and the captured selection.
- Region result placement: leave regions unchanged, modify their paint layers,
  create layer groups, use editable transparency masks, or retain previous layers.
  Region group links are stored per document. Layer changes form an undo macro.
- History image export to fast/compressed PNG, WebP, lossless WebP or JPEG;
  optional PNG generation metadata excludes embedded region mask image data.
- Optional latest workflow JSON in the local logs folder and history thumbnail size.
- Restore Defaults resets the Interface page without deleting styles/history.

Live controls are present and disabled with an explanatory tooltip: this build
still does not implement the Live workspace. Prompt translation is also disabled
because the native Orchestrion connection does not expose translation support.
The page does not claim these unavailable mechanisms are functional.

The public APK retains 96 original OFL/Apache font files, licenses and the text
properties QML dependencies. No proprietary Windows fonts are bundled.

Windows plugin 1.53.0-baron.8 bundles the same fonts, registers them with Qt, and
copies missing files into Krita's own font resources for its text engine. Existing
font files are preserved. A separate per-user Windows installation registers the
96 families for other programs. Restart already running applications to refresh
their font lists. Fonts affect text rendering, not the diffusion model's prompt.

Validation records: `build/tests-v019.txt`, `build/tests-v019-asan.txt`, and
`artifacts/apk-v019-verification.json`. Desktop Qt tests and package inspection
are separate from tablet acceptance. Samsung runtime and live generation have
not been exercised for this version. See RELEASE_018.md for upstream/dependencies.
