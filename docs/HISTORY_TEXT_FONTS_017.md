# Baron 0.1.7 — history, progress, settings and text

## What changed

- Native generation history uses the Python plugin's version-1 `ai_diffusion/ui.json`
  and result annotations. Batches, placement, transparency, prompts, seed and applied
  flags were tested in both directions with the actual Python `ModelSync`.
- History also has an atomic local recovery journal. It follows the document UUID;
  separately changed Windows history takes precedence over an older Android cache.
  Save the document as `.kra` to carry the archive to another device. Unsaved canvases
  still depend on Krita's own document recovery after an application restart.
- Baron-created archives retain all results until explicitly deleted. Windows plugin
  `1.53.0-baron.7` preserves this flag and keeps archived results when evicting RAM.
- Completed jobs for inactive open documents now update that document's annotations.
- WebSocket execution/step events use the same client ID as submission and a Bearer
  header. The bar stays below 100% until results arrive. History polling and reconnect
  remain available if the socket disconnects.
- Settings have the plugin's left category list and separate Connection, Styles,
  Diffusion, Interface, Performance and Plugin pages. This does not claim complete
  parity with every Python workspace (for example Live is still separate future work).

## Empty Text Properties panel

The installed Qt QML modules were located underneath androiddeployqt's application
QML scan root. Qt therefore skipped them as application-local files. The scan root
is now staged outside the dependency install prefix. The finished APK's RCC archive
was opened with Qt and verified to contain `QtQuick.Controls`, `QtQuick.Layouts`,
and `org.krita.components`; their native libraries are present too. This fixes the
confirmed missing-module packaging defect, not every possible upstream text issue.
The pinned base remains Krita `5.4.0-prealpha`, revision `96434e2`.

## Private Windows fonts build

`scripts/bundle-personal-fonts.py` gathers system fonts, per-user fonts and external
font files referenced by Windows' registry. The private bundle contains 699 distinct
files, 417921171 bytes. Desktop FreeType/Fontconfig recognized 696 of them, with 366
unique family records. The old vector `.fon` files Modern, Roman and Script could not
be decoded. CAD SHX files and Windows composite-font configuration are not text fonts
supported by Krita and were excluded.

The personal APK includes all 699 font files; each packaged SHA-256 was checked.
Fontconfig scans the bundled directory and Qt registers its TrueType/OpenType fonts.
Normal builds do not bundle these private files. Later normal updates retain the
previously installed font directory; uninstalling or clearing app data removes it.
The private APK and font manifest stay in ignored build/delivery directories, outside
the shared public update feed.

Both APKs use package `org.krita.baron.debug`, code `5050407`, matching the previous
signing certificate. Install over the old application without uninstalling it.
The private APK's first launch must copy about 418 MB of font data before starting.

## Evidence and limits

- Native Qt: 47 passed, 0 failed, 1 optional live-preview test skipped.
- Python fast suite: 432 passed, 111 skipped; 7 setup errors because the local test
  ComfyUI installation is absent. Persistence-specific tests passed.
- Changed Python files pass Ruff formatting/lint; Pyright reports zero errors.
  Full-repository Ruff still reports 181 existing issues, including extracted vendor
  examples; full formatting check lists 19 unrelated files.
- Live authenticated website WebSocket connected and received a `status` event.
  This read-only check did not start or charge a generation.
- Final module code matches the cross-compiled output; both APKs have identical
  native modules. QtCore matches 0.1.6 exactly; no emulator-only patches were shipped.
- Physical Samsung rendering, progress during a new generation, text editing and
  font selection have not been verified on the user's tablet.

The public feed contains ordinary Android 0.1.7 and Windows 1.53.0-baron.7.
No website backend/compiler change or server restart is needed for this update.
