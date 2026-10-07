# Android 0.1.30: remembered portrait and landscape layouts

The Android application remembers QMainWindow dock/toolbar state separately for
portrait and landscape. This includes dock areas, sizes, tab groups, visibility
and toolbar placement. The controller is installed on the Krita main window even
when the AI docker is hidden. No canvas pixels, document sizes or generation
settings are changed by restoring a layout.

The first orientation begins with the user's current workspace. An orientation
without a saved layout keeps the currently fitted arrangement; adjust it once,
and subsequent stable changes are remembered automatically. Saved layouts survive
application restarts and switching documents. Inverted portrait/landscape share
their respective layout with the normal orientation.

Screen orientation is read from QScreen, not the dock or window aspect ratio.
Rotation is debounced before restoring state. Saving is suppressed during restore,
when the keyboard is visible and in Canvas Only mode, preserving the previous
layout. Rapid turns do not save a partially restored layout. Native startup waits
for the normal workspace initialization before restoring an orientation profile.

Settings are under AI Diffusion > Settings > Interface > Layouts by Orientation.
The feature defaults to enabled. Toggle it off to retain manual workspace behavior.
Forget Saved Layouts clears only the two orientation profiles; named Krita
workspaces remain available. The three settings strings are translated into all
14 bundled interface languages.

The 0.1.28 background result processing/prompt actions and 0.1.29 fixed result grid
are retained. The public update feed had remained at 0.1.27 because the last two
builds were delivered locally under the earlier read-only website instruction.
This request authorizes publishing the latest APK and matching GPL source archive
to the dedicated baron-updates directories, followed by an atomic stable.json
update. Site application code and the production frontend build are not rebuilt.

Validation covers separate dock/toolbar areas, tab groups, repeated rotation,
inverted orientation, changed window dimensions without a screen turn, disabled
mode, restart persistence, Canvas Only protection and rapid turns. Full native
tests are run on Windows Qt 5.15.2 and Linux Qt 5.15.13 with AddressSanitizer.
The packaged Android module, version, signature and public downloadable bytes are
verified separately. Physical Samsung Tab S8+ rotation/keyboard acceptance remains
open; test arranging the landscape layout, turning/arranging portrait, turning
back, restarting and opening/closing the keyboard.

Version 0.1.30 / code 5050430 retains org.krita.baron.debug, the existing signing
certificate and the 96 licensed public fonts. Windows plugin stays at baron.13.
