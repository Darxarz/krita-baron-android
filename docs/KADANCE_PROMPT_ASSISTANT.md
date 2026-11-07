# Kadance prompt assistant — Windows .24 / Android .40

The assistant is opened with **Kadance…** beside the style selector. It proposes an
English positive prompt and explains its changes in the interface language. The artist
can edit the proposal, insert it, or undo the insertion. It never starts a generation.

## Model and operation context

Both clients send the actual selected checkpoint and architecture, not the style's
display name. The server checks exact filename matches against its model catalogue;
known catalogue architecture takes precedence over a stale client value. Unknown
architectures fail explicitly instead of silently using SDXL rules.

The request distinguishes instruction-based **Edit**, **Generate**, denoising
**Refine**, and selection/region **Inpaint**. It includes strength, selection presence,
global versus active regional prompt, Comfy/A1111 syntax, and Krea reference purpose.

Generation rules reuse the site's `services/promptFamilyGuides.js`. Edit rules override
caption length and scene-building advice for FLUX 3, FLUX, Qwen and Krea. SDXL/Pony
refinement uses tags; Anima uses tags plus sentences; MOSS regional prompts retain the
character/quoted-text distinction. The assistant does not change sampling settings,
negative prompts, model choice, region masks, or LoRA parameters. Existing LoRA
directives are retained byte for byte by the server.

Instruction-edit capability is checked independently of the UI's Edit label. SDXL,
MOSS, Anima and ordinary FLUX.1 receive denoising captions/tags and a limitation tip,
not image-number commands intended for Qwen/FLUX.2/FLUX.3/Kontext/Krea editors. Older
Qwen architectures are labelled separately from Qwen 2.1. Caption length advice never
authorizes inventing a pose, background or other details missing from the request.

The proposal is rejected if the document, selected model, operation, region, strength
or prompt changed while it was being reviewed. Open Kadance again after changing its
context. Changing a prompt after the request requires asking again before insertion.

## Images and reference transfer

The checkbox includes the source canvas for editing/refinement/inpainting and the
configured reference controls. At most **four images including the canvas** are
accepted, reduced to a maximum long side of 1024 pixels and encoded as JPEG. For
editing, the source is image 1 and references follow. Native Android shows numbered
thumbnails. When disabled, this is a text-only review; the assistant is instructed
not to invent colors or claim it inspected references.

Example instruction, with the source in image 1 and character reference in image 2:

> Correct the existing character in image 1 to match the fur colors and markings in
> image 2. Keep image 1's pose, proportions, expression, composition, background and
> lighting. Transfer appearance only; do not add another character.

Images are sent only after **Ask Kadance**, through the existing authenticated site
connection and the site's AI provider. Existing site image moderation runs first;
temporary moderation images are removed afterwards. Provider retention policies still
apply. No additional generation jobs or conversation history are saved by this endpoint.

This is an LLM suggestion, not a guarantee of the image model's response. Verify the
proposal's colors, image numbers and intended changes before inserting. The assistant
does not automatically fix model/workflow limitations or align reference poses.

## Server installation

Dedicated branch/worktree: `D:\orchestrator-krita-kadance`, `codex/krita-kadance`.
Server files:

- `routes/kritaIntegration.js`: mount and `prompt-assist:1` feature.
- `routes/kritaPromptAssistant.js`: authenticated `POST /api/krita/prompt-assist`.
- `services/kritaPromptAssistant.js`: bounded context/images, model rules, AI call.
- `tests/kritaPromptAssistant.test.js`: context, images, auth, rate limits and adapters.

No database migration, npm dependency or portal frontend build is needed. Restart the
existing site service after copying the three source files. The endpoint uses existing
configured `kadanceTips` (text) and `videoVision` (images) provider/model roles. It does
not expose provider credentials. Limit: ten requests per user per minute, two concurrent
requests overall. Provider/image-moderation failure leaves generation controls intact.

Expected request fields: `instruction`, `prompt`, `model`, `architecture`, `mode`,
`has_selection`, `scope`, `strength`, `prompt_mode`, `reference_purpose`, `language`,
`images: [{role: 'canvas'|'reference', label, data: 'data:image/...;base64,...'}]`.
Response: `prompt`, `summary`, `tips`, `images_used`, `family`.

## Validation and limitations

- Python: targeted transport/context/image tests, full fast suite excluding external
  ComfyUI client tests, Ruff formatting and Pyright with the checkout's venv.
  Final results: **609 passed, 111 skipped**; Pyright **0 errors**; all files formatted.
  Full-repo Ruff reports 68 existing findings. A comparison with HEAD verified
  **zero introduced findings**; the assistant's new files pass lint.
- Native: Qt tests cover transmitted context, review before insert, stale text,
  provider failure isolation, plus existing canvas/history/IME tests.
  Final result: **134 passed, 2 skipped, 0 failed** (offscreen desktop Qt).
- Server: family and mode matrix includes opaque filenames, FLUX 3/Qwen/Krea Edit,
  SDXL/A1111 refinement, MOSS regions, image order, moderation, limits and catalogue
  override. The existing integration test still expects 14 consent languages while
  the baseline already has 15; this unrelated expectation is unchanged.
  Latest assistant tests: **23 passed**. Existing integration tests: ten passed,
  that one pre-existing failure. Live authenticated text and two-image vision
  requests both returned HTTP 200; FLUX 3 Edit correctly proposed a reference color
  transfer with explicit geometry/background preservation on synthetic circles.
- UI strings have Russian and English translations; other locales use English for
  the newly added assistant strings.
- Android must still be tried on the user's Samsung tablet. Linux/offscreen Qt tests
  and APK verification do not prove physical Android keyboard/rendering behavior.

Source branches are retained locally. No unrelated dirty files were reset or committed.
