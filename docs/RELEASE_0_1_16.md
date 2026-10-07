# Android 0.1.16 — native preview projection refresh

History presentation now groups consecutive results with matching parameters,
ignoring the seed, fixed-seed UI flag and batch count. Native-only timestamps,
favorites and application marks do not split a group. Individual persisted
jobs, seeds and image indices are retained; this only changes presentation.
Header text is left-aligned and includes time, strength below 100%, and prompt,
as in the Python HistoryWidget. Previously every single queued result started
a full-width row, and centered header text lay outside the visible viewport.
A Qt regression reproduces the old seven-header/seven-image column, then
checks the grouped grid at 320 pixels, wrapping and document reopening.

Reported on 0.1.13: the generated image existed in the temporary layer thumbnail,
but the canvas displayed it only after permanent application. The Qt panel was
calling preview successfully; it was not a missing server result.

The native host wrote preview pixels and marked only the root layer dirty.
That requests a projection update starting at the root and can reuse stale
descendant projections. Python's `Layer.refresh` instead updates the changed
layer through Krita's node command path. The native host now explicitly requests
a recursive graph refresh after preview changes, rather than depending on a
root-only dirty request.

Refresh is requested after showing/replacing/hiding/removing a temporary layer,
and after both hiding it for a canvas capture and restoring its visibility.
Capture waits for projection work before reading pixels, so generated previews
remain excluded from new inputs. Removing a preview restores the hidden original
nodes while holding the same image barrier lock. Refreshes are scheduled after
unlocking; preview/apply semantics and undoable permanent layers remain separate.

Verification includes compiling the real KisImage host in the Android build,
the standalone Qt UI/queue/history tests, APK module/version/signature/source
checks, and public package download verification. The standalone tests use a
CanvasHost double and do not verify Krita's actual canvas projection. Physical
Samsung tablet rendering remains unverified and needs the reported scenario
retested on the device. This release retains the ControlNet parity improvements
and default-style compatibility fix from 0.1.14 and 0.1.15.

The full-canvas recursive refresh favors correctness; very complex documents
may take longer to redraw. Complete plugin parity is still incomplete, as
documented in the earlier audit.
