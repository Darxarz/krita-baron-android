# Android 0.1.23 — touch scrolling in result history

Result history now registers Qt's single-finger touch scrolling gesture on its
entire viewport and uses pixel scrolling. A drag follows the finger; a flick
continues with inertia using Qt's platform defaults. Horizontal/vertical
overshoot is disabled so the thumbnail overlays remain within the panel.
The scrollbar is still available, but touching the screen edge is unnecessary.

Dragging does not count as clicking a result, applying it, or tapping empty
space to hide the preview. On a scroll/drag release the list receives an
off-item cancellation release to clear Qt's pressed-item state. Apply/context
overlays hide while scrolling and return when motion stops. Short thumbnail
and empty-space taps retain their existing preview behavior, and mouse/double
click actions remain available. No history data format or prompt handling
changes; the 0.1.22 IME crash fix is included.

Synthetic touch regression cases begin on a thumbnail, empty space, and the
selected thumbnail's Apply overlay. They verify scrolling after release,
absence of click/apply/hide signals during swipes, and normal short touches
after scrolling stops. Existing canvas-preview and grouped-history tests
cover the surrounding behavior. Physical Samsung gesture acceptance remains
pending a device test.

Reference: https://doc.qt.io/qt-6/qscroller.html
