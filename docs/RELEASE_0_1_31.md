# Krita Baron Edition 0.1.31

Generation and Edit now remember separate positive/negative prompts and disabled
fragments. Switching workspace does not clear either bank. Both banks persist in
the document's `baron-native-state` annotation and application preferences.
Legacy documents/settings migrate their current prompt to the active mode;
the other mode starts empty. Prompt replacement uses the existing Android IME
safe path, including cancellation of delayed paste/completion operations.

Settings → Styles → Sampling settings adds ComfyUI / AUTOMATIC1111 / Forge
prompt syntax, optional A1111 GPU noise, and ENSD. These settings are stored in
styles, document state, and generation history. Default remains ComfyUI.
The Windows plugin adds the same style settings in 1.53.0-baron.14.

The backend follows Orchestrion's `services/a1111PromptMode.js`: smZ text encoding,
mean normalization, schedules/BREAK/AND, GPU RNG and the same model settings.
All encoder branches are converted, including second passes and regions.
A1111 uses KSamplerAdvanced so smZ's sampler hooks run; seed, sampler, CFG,
total/start/end steps and denoise are retained. Requires ComfyUI_smZNodes,
SD 1.5 / SDXL / Illustrious / Pony / NoobAI, and a native KSampler scheduler
(normal/karras/etc.; separate scheduler nodes such as AYS are rejected clearly).
Qwen and other architectures retain their normal encoding. Interstice does not
provide smZ nodes and rejects this optional mode. Missing nodes are reported,
not silently downgraded. A matching seed alone does not guarantee an identical
image across different applications, models, samplers, devices or workflows.

The server compiler is updated separately; website source is only read for this
feature. Each compiler call starts a new process, so deployment of its reviewed
files needs no website restart. ComfyUI users running their own native bridge must
update its compiler/vendor snapshot too.

Validation: Windows Qt tests; Linux Qt tests with AddressSanitizer; Python engine
tests including full A1111 graph compilation; a golden graph generated from the
website implementation; API/style round trips; real ComfyUI SDXL smoke generations
with identical seed and GPU noise off/on. APK version/signature/content and public
download hashes are checked before publishing. No physical Samsung tablet test.
The full Python CI suite has pre-existing server installation fixture failures;
full Ruff has pre-existing lint findings and format differences. Changed Python
code passes focused lint (excluding existing float comparisons), format, Pyright.
