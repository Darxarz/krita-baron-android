# Krita Baron Edition 0.1.35

- Orientation profiles are flushed to disk after settled changes and on background,
  close and quit. Startup restoration cannot overwrite profiles with an incomplete
  set of dockers; late-created dockers receive their saved placement.
- Added the plugin custom inpaint/refine row: Seamless, Focus, instruction Edit,
  pre-fill choice and Automatic Context / Selection Bounds / Entire Image /
  named mask-layer context. Model capabilities and strength govern availability.
- Added Fill, Expand, Add Content, Remove Content, Replace Background and Custom
  modes to the action menu. Automatic mode uses original selection/canvas geometry.
- Input context, selection mask and output bounds now remain separate. Results
  and preview map back to the selected area rather than the full context crop.
- The Orchestrion compiler uses the original Python detect_inpaint and prepare
  functions, including custom flags and pre-fill. Legacy APK requests retain their
  prior behavior. The Interstice serializer receives the same explicit options.
- Existing manual selection feathering is preserved; no extra automatic growth
  or feather is imposed by default. Settings persist globally and in document state.

- Final parity corrections: diffusion-aligned selection bounds use the original
  16-pixel multiple (32 for Qwen 2), base Qwen exposes Seamless where supported,
  and context/pre-fill controls use the original flat combo styling.

Validation: Windows and Linux Qt suites (124 passed, 2 existing skips each),
25 Python engine/bridge tests with 54 subtests, 32 geometry golden cases obtained
from the original Python functions, cross-process profile persistence and
late-docker restoration. Authenticated production native/prepare returned HTTP
200 and a 29-node workflow without submitting inference or charging coins.
Targeted Python formatting, Ruff and Pyright passed; whole-project checks retain
unrelated existing font-script/import diagnostics. APK packaging/signature and
public downloads are verified separately in artifacts.

Physical Samsung tablet restart, touch interactions, inference image quality and
all Interstice model combinations have not been validated by these automated
checks. Source branch: codex/native-inpaint-v0134.
