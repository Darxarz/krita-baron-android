# Android 0.1.26: upscale and resolution parity

The native upscale workspace now follows Acly's `ai_diffusion/ui/upscale.py`:
upscaler selector, 1-4x factor slider in 0.5x increments (typed fractional values
are preserved), target dimensions, checkable diffusion refinement, shared style
selection, strength, image guidance, automatic/custom tile overlap and Use Prompt.
Generation prompt/control rows and queue batch/resolution rows are hidden in the
upscale workspace. Settings persist in document state and generation history.
Existing generation, clipboard, touch scrolling and queue fixes are retained.

The selected upscaler is used in both simple and tiled paths. The Orchestrion and
Comfy adapter compiler calls the original vendored `WorkflowKind.upscale_tiled`
implementation instead of building a different sampling graph. Interstice uses
the equivalent native WorkflowInput serializer. At 1x diffusion mode refines the
image without an upscaler model; edit-only architectures use full strength, as in
the original. Image guidance is enabled when a compatible tile model is present.
The whole document is captured even when a selection is active. On enqueue its
existing layers are scaled with Krita's bilinear image scaling, matching the
Python document API; the returned upscale is automatically applied as a layer.
Region masks in result metadata are scaled to the target document size.

Resolution means internal diffusion processing dimensions, not output canvas
dimensions. The 0.3-1.5x slider passes its value into the original resolution
preparation, preserving checkpoint minimums, model alignment and two-pass
generation. At 1x the global performance resolution multiplier applies. The
generation ceiling defaults to the original 6MP and is configurable in
Performance; the cloud backend clamps it to 1-8MP. The 5% threshold tolerance is
preserved: increasing the multiplier after reaching the cap can slightly reduce
rounded inference dimensions. The displayed website quote is still an approximate
dimension-based estimate; the prepared graph supplies the actual job quote.

Validation: 22 Python compiler/adapter tests; 106 native Qt tests passed on Windows
Qt 5.15.2 and Linux Qt 5.15.13 with AddressSanitizer (two optional tests skipped).
Tests cover chosen upscalers, tile sizes/overlap, factor snapping and typed values,
resolution values 0.3/0.5/0.8/1/1.5, preserved target size, full-canvas capture,
global multiplier inheritance, queue submission, document scaling and automatic
result application. Ruff checks and formatting pass for the changed Python files;
Pyright reports zero errors for those files with the configured Python runtime.
Whole-project Pyright still reports pre-existing errors in font curation scripts.

APK uses versionCode 5050426 and the existing `org.krita.baron.debug` application
identity and signing certificate. The public bundle contains the existing 96
licensed public fonts, not private Windows fonts. Final APK, deployment and public
download verification reports are under `artifacts/`. Samsung Tab S8+ runtime and
real GPU upscale acceptance remain untested; standalone screenshots are UI previews.
