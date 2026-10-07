# Current native build: 0.1.6

Startup correction and matching Krita build flags:
[ANDROID_STARTUP_016.md](ANDROID_STARTUP_016.md). Native Qt suites: 43 pass
on Windows and Linux; one optional live test skipped on each.

## Interface reconstruction in 0.1.5

The main generation panel has been reconstructed from the Python plugin. Current
mapping, verification and remaining parity work are recorded in
[PLUGIN_UI_RECONSTRUCTION.md](PLUGIN_UI_RECONSTRUCTION.md). Native Qt tests: 30 pass
on Windows/Linux; compiler tests: 12 pass. English/Russian: 205/205 strings.
Windows update feed remains at 1.53.0-baron.6. The entries below describe older releases.

# Baron native 0.1.4 / Windows plugin 1.53.0-baron.5

## Native 0.1.4 queue and document settings

- Up to eight independent native jobs can be prepared/submitted while previous jobs
  run on the server. Each freezes its own prompt, seed, guidance pixels, document and
  selected bounds. Server queue ordering remains authoritative.
- Jobs menu lists preparing/submitting/queued/running/downloading/error states.
  Counters distinguish the current document and all native panel jobs.
- Cancel selected / queued / all only operates on this panel's recorded job IDs.
  Cancelling while the submission response is in flight waits for its prompt ID
  before interrupting it. It never clears the shared server queue.
- Download failures can resume the same server job without submitting again. Failed
  preparation cannot be retried as though it were an existing inference job.
- Checking Fixed with a random/invalid sentinel creates one valid seed and keeps it.
- Prompts, model, LoRAs, sampling settings, references, regions and linked control
  layer IDs are stored as a bounded annotation in the Krita document. Save as .kra
  to keep them across reopening. Changing documents restores their own setup.
  Generation settings changes mark the document modified. Results remain in memory.
- Queue inputs/results have memory and job-count limits. Closed/changed documents
  still require their original matching canvas before preview/apply.
- 28 Qt tests passed on Windows and Linux, one optional live image test skipped.
  Added coverage for independent document targets, retry without resubmission,
  cancellation during submission, control/reference persistence and fixed seed.
  Physical tablet runtime and paid production inference remain untested.
- English and Russian cover all 168 current native UI strings. Other languages
  retain available translations and fall back to English for missing additions.

The Windows plugin is installed on this notebook. Restart Krita to load it.
Install native 0.1.4 once over the previous Baron APK. Later updates are checked
on startup and can be downloaded inside Krita. Android asks the user to approve
installation. This is an update mechanism for Baron, not stock Krita or Acly.

## Delivered native interaction

- Results live under generation. Selecting a thumbnail creates a locked temporary
  paint layer on the original canvas. Selecting another replaces it. Clicking the
  selected thumbnail again removes the preview and adds a permanent undoable layer.
  Hide preview removes it. Switching documents removes the temporary layer.
- Canvas dimensions are automatic. Active selections determine generation bounds;
  selection alpha limits both preview and committed pixels. Old results retain their
  original document and offset. Canvas size changes are checked before applying.
- Generation and instruction Edit are separate choices next to Generate; generation
  below 100 percent strength refines the captured canvas. Strength is a slider.
- One control menu contains Reference, Style, Composition, Face, Scribble, Line Art,
  Soft Edge, Canny Edge, Depth, Normal, Pose, Segmentation, Blur, Stencil and Hands.
  Sources are linked Krita layers or files. Strength and start/end range are sent to
  the existing Baron workflow engine. Layer lists refresh when opened. A removed
  linked layer is reported instead of silently switching to another source.
- The star on supported control rows creates a control map through the same server
  preprocessors used by the plugin, inserts it as an editable Krita layer, then links
  the row to that layer. No Python interpreter is included in the APK.
- Regions derive masks from layer alpha or the active selection, have separate
  prompts, and can receive their own control rows. Supported families match the
  original engine: SD1.5, SDXL, Illustrious and Anima. Qwen/Krea regions are rejected
  explicitly because this engine does not support regional attention for them.
- The settings popup contains steps, CFG, sampler, scheduler, batch, fixed/random
  seed, queue position, reference settings and resolution multiplier. The multiplier
  affects internal sampling resolution; output dimensions remain those of the
  canvas/selection. It is separate from actual Upscale factor.
- Only the active native job can be cancelled; cancellation names its own prompt ID.
  This does not clear the shared server queue or other users' jobs.
- Prompt, negative prompt, workspace, checkpoint, steps, CFG, strength, seed, batch,
  sampler/scheduler and resolution settings survive normal saved session settings.
- History is limited to 64 cards / 256 MiB; document captures retain FIFO target
  records and weak references to original layers. Closed documents are not kept
  alive by the history targets.

## Update channel and site changes

Feed: https://orchestrion.su/baron-updates/stable.json.
Packages are HTTPS files under /baron-updates/releases/ with SHA-256 and byte count.
Windows stages and validates the entire ZIP, protects user data and backs up replaced
files; partial installation failures restore previously replaced files. Android streams
the APK into its private cache, verifies its digest, then checks package, increasing
version code and signing certificate before calling the system installer.

The site worktree is D:/orchestrion-baron-updates, branch codex/baron-update-channel.
Only new static channel files and a reviewed native compiler copy live there.
Production JavaScript and frontend files were not replaced. The site's active
compile_workflow.py was backed up and updated after verifying its previous digest
and all 428 unchanged vendor files. The compiler is started per request, so no server
restart is needed for that change. Static channel files also need no restart.

Compiler backup: G:/orchestrator/baron-update-delivery-20260930/compile_workflow-before.py.
Build copies: G:/orchestrator/user-portal/build/baron-updates.
Persistent frontend public copies: G:/orchestrator/user-portal/public/baron-updates.
Publisher: scripts/publish-updates.py. Publish immutable files first and manifest last.

## Evidence

- Windows plugin: 415 fast checks passed, 111 skipped; 6 URL client checks passed.
  Seven additional client checks need tests/server, which is absent here.
  Pyright: 0 errors. Changed updater/network/translation code passes Ruff.
  Whole-tree Ruff still reports existing issues elsewhere; no blanket lint-clean claim.
- Native Qt tests: 25 passed on Linux and Windows, optional live image test skipped.
  Includes preview switching / second-click commit and corrupted update rejection.
- Compiler: 10 tests passed against captured ComfyUI object_info/model metadata.
  Includes selected-area generation, both edit families, resolution multiplier,
  sampler/scheduler overrides, reference instructions, region validation and maps.
- ARM64 APK built against full Krita source and validated for native module, update
  Java helpers and absence of Python. App ID remains org.krita.baron.debug;
  versionCode is 5050404. Certificate matches the previous APK:
  e3934eb57ff0d5215766a327a40bdb266444bc630633c979bc12eae06ea8001b.
- Public .5 ZIP and .2 APK downloads were verified against their published digests.
  Actual Qt Windows update check/download/install to a temporary folder succeeded
  and preserved settings. The installed notebook plugin was updated with a backup.
- No tablet is attached. Android installer interaction and native canvas behavior
  have not been exercised on a physical tablet in this run. No paid inference job
  was submitted. An authenticated production prepare attempt from CLI returned 403;
  successful production preparation is not claimed.
- English/Russian native dictionaries cover all 153 UI strings. Other languages
  reuse existing translations and fall back to English for untranslated additions.
- Source changes remain uncommitted WIP over d4ff3ba; source ZIP is a working-source
  export, not a claim that GitHub already contains these changes.

## Remaining parity work

This release does not represent complete parity with the Python plugin. Live drawing,
animation frame batches, custom graph importing, style/preset management, diffusion
tile Upscale, star/export/context actions for history,
control preset interpolation and selection
context/feather options still need their native UI and integration. History currently
lives in memory for the running panel. Upscale currently uses the simple model path.
These gaps must remain visible in future handoffs rather than being advertised as
completed because the server ships the full Python engine.
