# Android 0.1.28: background results and prompt fragment actions

## Receiving results

PNG decoding, result PNG packing, selection-mask decoding, thumbnail generation,
foreground-mask composition and the local history journal now run on bounded
worker pools. The queue retains the downloading state until preparation finishes.
Cancellation, retry and changing documents discard obsolete callbacks. The GUI
receives prepared images and thumbnails rather than encoding and decoding them again.

New results append to existing history groups without rebuilding all older tiles.
Imported history thumbnails load asynchronously. Document annotations are updated
on the GUI thread immediately to preserve `.kra` saving; disk hashing and journal
writes use a separate serial worker. Krita canvas layer insertion still runs on the
GUI thread because the host document API requires it.

## Prompt actions

Tap a comma-separated prompt fragment to open its action bar. The first row offers
weight adjustment, translation, copy, paste after the fragment, deletion and More.
Drag the grip to reorder a fragment. Long presses and text selections retain text
editing behavior. Buttons do not take focus from the prompt editor.

More offers weight presets, duplicate/cut, moving left/right/start/end or between
logical groups, positive/negative prompt transfer, temporarily disabling fragments,
space/underscore conversion, BREAK insertion, sentence-to-tag translation,
multi-selection and sorting the whole prompt. Dictionary suggestions and typo
correction use the existing tag datasets. Disabled fragments have restore/discard
buttons and persist with the document, but are excluded from generation requests.

Sorting presents a preview and an explicit family selector. Duplicate removal is
off by default. Optional AI classification uses the site's existing capability and
label endpoints, with validation against changing or losing prompt fragments.

The original website parsing, weighting, reordering and lint algorithms are copied
under `native/prompt/`, with source hashes in `provenance.json`. The checked-in
`logic.js` is compiled for Qt 5's JavaScript engine; no Python interpreter is added.
Localization comes from the website prompt editor messages. Existing browser-login
credentials authorize the site's translation and prompt-label endpoints.

The new controls retain the previous Android IME, safe plain-text clipboard and
selection protections. Qt 5 Concurrent and Qml are now explicit dependencies.

## Validation and delivery

112 native Qt tests pass on Windows Qt 5.15.2 and Linux Qt 5.15.13 with AddressSanitizer;
two optional runtime-dependent tests are skipped. Tests include 280 golden cases
from the original website algorithms, asynchronous 2048x2048 image preparation
while GUI heartbeat events continue, cancellation during decoding, deleted owners,
prepared-history alpha integrity, clipboard/selection/undo and mocked website APIs.
No paid translation or generation requests are needed by these checks.

Standalone dark/light prompt action previews verify the layout. These checks do not
establish physical Samsung Tab S8+ performance or keyboard behavior; test receiving
large results, scrolling, editing and selection on that tablet after installation.

Version is 0.1.28, Android versionCode 5050428, package `org.krita.baron.debug`.
The existing signing key is retained for installation over the previous build.
The APK contains the existing 96 licensed public fonts, no private Windows fonts.
APK, matching GPL source archive and verification reports are prepared locally
under `artifacts/`. The website at `Z:\orchestrator` was inspected read-only: no
server files, production build, download links or automatic-update feed were changed.
