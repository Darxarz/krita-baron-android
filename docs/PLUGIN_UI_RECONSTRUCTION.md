# Python plugin UI reconstruction: native 0.1.5

Reference checkout: `D:/krita defusion baron edition`, current Baron Python plugin.
Native checkout: `D:/krita-baron-android-native`, uncommitted work over d4ff3ba.

## Deconstruction and implementation

| Python reference | Native implementation | Behavior |
| --- | --- | --- |
| ui/generation.py: GenerationWidget | BaronPanel, generation workspace | Workspace icon, style selector and gear; positive/negative prompts; controls; strength and add buttons; Generate/Edit; price and Models; progress/error; inline history. No main tabs or extra heading. |
| ui/widget.py: WorkspaceSelectWidget, StyleSelectWidget | PluginUi resources, styleSelect | Original icons copied from the plugin. Colors and fonts inherit Krita. Settings open through the gear. |
| ui/widget.py: TextPromptWidget | PluginUi::PromptEditor | LoRA highlighting/completion, Ctrl+Enter, draggable editor height. Default positive height matches this user's ten-line plugin preference. |
| ui/control.py: ControlWidget | GuidancePanel::ControlRow | Compact mode/source row and original icons; fifteen control/reference modes; folded strength, start/end and external image controls; preprocessor action where supported. Visible projection and document layers are available. |
| ui/region.py: RegionPromptWidget | GuidancePanel and RegionRow | Active regional prompt replaces the root editor. Root/other regions become summaries. Link and remove controls remain beside the header. Region state and selected region persist with the document. |
| ui/widget.py: StrengthWidget | denoiseSlider and percentage input | Linked slider/input and Generate/Refine labels. Instruction Edit remains a separate action. |
| ui/widget.py: QueuePopup | generationSettings menu | Document/all job counts, batch slider, fixed seed, resolution multiplier, queue position, scoped cancellation. Replacement waits for cancellation acknowledgements. Batch jobs use sequential seeds from the captured base seed. |
| ui/generation.py: HistoryWidget | resultHistory and context menu | Transparent 96-pixel thumbnails, time/strength/prompt headings, click to preview, second click to commit, applied star, prompt/evaluated prompt/strength/style/seed copy, export, discard and clear. |
| ui/settings.py and style presets | Native settings dialog | Save/import a plugin JSON preset; checkpoint, LoRAs, style prompts, sampling, VAE, clip skip, preferred resolution, V-prediction and SAG. Matching checkpoint filenames are required. |
| OrchestrionBar | Automatic quote and Models row | Estimated bleatbucks only. Separate quote client reads dimensions without capturing pixels or disturbing canvas preview. |

`server/compile_workflow.py` accepts validated style options and returns the evaluated
prompt metadata. Actual workflow construction continues to use the vendored plugin
engine on the server. The Android app contains native Qt/C++ UI, without Python.

## Verification

- Qt tests: 30 pass on Linux and Windows; one optional live image test is skipped.
- Server compiler: 12 tests pass, including imported style flags, the 1% strength boundary and invalid options.
- Ruff check, Ruff format check and Pyright pass for the native repository's Python tooling.
- UI inspected in dark/light palettes and a narrow dock. Preview thumbnails are synthetic
  test images, not evidence of a production inference job.
- Server compiler replaced atomically after verifying 430 vendor source/resource files;
  previous compiler retained in `Z:/orchestrator/baron-ui-delivery-20261001`.
  The server starts a compiler process per preparation, so this change needs no restart.
- No physical tablet runtime or paid generation was tested in this run.

Release APK: `krita-baron-android-arm64-v0.1.5.apk`, 181584954 bytes.
SHA-256: `820b3e6005602d51474c9801aded3261d99b627e3ee9d17b77f409f32f62b7c4`.
Package: `org.krita.baron.debug`, versionCode 5050405, versionName 5.4.0-baron.0.1.5.
APK signature verification passes; certificate matches 0.1.4:
`e3934eb57ff0d5215766a327a40bdb266444bc630633c979bc12eae06ea8001b`.
Native module, current UI markers and Java helpers are present; Python libraries absent.

## Remaining full-parity work

This is reconstruction of the main generation workspace, not a claim of complete or
pixel-perfect parity for every plugin workspace. Native Live, Animation and Graph
workspaces are not implemented. Upscale still uses the simple model path; tiled
diffusion refinement and the original separate Upscale UI remain to be ported.
Background separation still lacks the original full set of native configuration fields.
Selection fill/context/feather modes, regional alpha-only generation, control preset
interpolation, persistent result history and sampled progress percentages remain open.
The native queue is bounded to eight jobs and history to 64 items / 256 MiB.

English and Russian cover every current native UI string. Other dictionaries reuse
available original translations and fall back to English for missing strings.
Private user presets and credentials are not embedded in the public APK or source export.
