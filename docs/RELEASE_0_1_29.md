# Android 0.1.29: fixed result thumbnails

Result history explicitly uses QListView's Static movement mode and disables
drag/drop on the view and its viewport. Switching to IconMode therefore cannot
enable free placement of thumbnails. Images keep their automatic grid positions,
generation groups and ordering; resizing still reflows the grid.

Touch swipes remain handled by the existing kinetic scroller, including swipes
starting on thumbnails and the apply overlay. Tap-to-preview, double-tap-to-apply
and tapping empty space to hide the preview retain their existing behavior.

Validation includes a simulated drag across the result grid checking every item's
position and identity, plus the existing touch inertia, preview and grouping tests.
This change removes unintended item movement; no device performance improvement
is claimed from changing the QListView movement setting alone.

Version 0.1.29 / code 5050429 retains the Android package and signing certificate,
the 0.1.28 background result processing and prompt actions, and the existing 96
licensed public fonts. The APK and matching source archive are prepared locally.
The website and its update feed are not modified. Physical Samsung runtime
acceptance remains untested.
