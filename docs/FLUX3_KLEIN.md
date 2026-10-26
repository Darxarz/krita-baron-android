# FLUX 3 Image Klein

FLUX 3 Image Klein (Black Forest Labs, distilled) generates and edits images with up to ten reference images. No weights are bundled with Baron Edition; access is handled by the server (FLUX Kommunity license).

## Server requirements

- ComfyUI with FLUX 3 support (`CLIPLoader` type `flux3`, nodes `ReferenceMultiLatent` and `EmptyFlux2LatentImage`; checked on ComfyUI 0.41.0). Older servers hide FLUX 3 checkpoints instead of failing at generation time.
- comfyui-tooling-nodes must report the model as `flux3-klein`, otherwise the checkpoint is not listed.
- Files:
  - `diffusion_models/flux3/flux3-image-klein-int8-convrot.safetensors` (`UNETLoader`)
  - `text_encoders/qwen3vl_4b_fp8_scaled.safetensors` (`CLIPLoader`, type `flux3`), the same Qwen3-VL 4B encoder as Krea 2
  - `vae/flux2-vae.safetensors`

## Workflow

- Base resolution about 1 MP (sides aligned to 16, at least 512 px, at most 1024² pixels), independent of the general pixel limit. At 2 MP the model produces single-pixel colour noise, chopped shapes and weak edits.
- Larger canvases use 2 passes for generate, edit (refine at strength 0.7 or more), selection edits (strength 0.7 or more) and inpaint: the base pass at about 1 MP, the plugin's upscaler (OmniSR 2x for factors below 1.5), a resize to the exact canvas size, then a second pass at strength 0.3 (5 of 16 steps) with the same conditioning and references as the base pass.
- `EmptyFlux2LatentImage` for generation.
- Style `FLUX 3 Klein` with sampler preset `FLUX 3 Klein - Euler`: euler / simple, 16 steps (minimum 8 when the strength is reduced), CFG 1. Steps between 8 and 30 work.
- The negative prompt is not used: the unconditional is `ConditioningZeroOut` of the positive (only sampled when CFG is above 1).
- No `ModelSampling*` nodes. LoRAs are applied to the model only (`LoraLoaderModelOnly`).
- Inpaint, outpaint and refine use the generic DifferentialDiffusion + `SetLatentNoiseMask` path (no inpaint model, no ControlNet). Inpaint runs at full strength; the server recommends at least 0.9 for selection refines.

## Edit and references

- The canvas (edit reference) goes into a `ReferenceMultiLatent` with `keep resolution`: it is already scaled to the sampled base size, so output and reference match (a 1408² output with a 1 MP reference drifted 10-19 px to the right). Reference control layers (Reference, Style, Composition, Face, and Scribble/Line art/Canny/Depth/Pose as instructions) follow in a second `ReferenceMultiLatent`, each resized to about 1 MP. Both are appended to the positive and the zeroed negative. At most 10 images, including the canvas.
- The prompt names them `<ref_image_0>`, `<ref_image_1>`, … in order: the canvas is `<ref_image_0>` when it is a reference, control layers follow. Generated instructions use these names, e.g. `Apply the style from <ref_image_1>.`
- Write a full caption of the result with the edit folded in.
- The output size follows the canvas (base pass, then 2 passes as above), `ReferenceMultiLatent`'s empty latent is not used.

## Regions

With two or more regions, the prompt gets the layout table that ComfyUI's `CreateBoundingBoxes` + `Flux3LayoutPrompt` build:

```text
a fantasy street at dusk, <obj_1> waves at <obj_2> [{"id": "obj_1", "bbox": [98, 39, 879, 449], "desc": "a red-haired girl in a green cloak"}, {"id": "obj_2", "bbox": [49, 547, 342, 938], "desc": "a wooden tavern sign that says \"TAVERN\""}]
```

- `bbox` is `[top, left, bottom, right]` of the region mask on a 0–1000 grid of the sampled image (cropped and scaled like the mask for inpaint and upscale tiles).
- Ids follow the region order from the bottom layer up; the background region (whose prompt is the global prompt) is left out. Cite boxes in the global prompt as `<obj_1>`, `<obj_2>`… to tie words to them.
- The global prompt Krita appends to region prompts is removed; a `character:`/`object:`/`text:` prefix (MOSS syntax) is dropped. Regional LoRAs are rejected.

## Tablet (Orchestrion)

- The workflow is compiled by the vendored engine (`server/vendor`, same code as the plugin). `server/compile_workflow.py` selects the `FLUX 3 Klein` preset with 16 steps and CFG 1 unless the panel sends other values.
- Selection edits (inpaint and refine with a mask) run at a strength of at least 0.9.
- In Edit mode with instruction edit the canvas is `<ref_image_0>` and the panel's references follow (`<ref_image_1>`…); the panel allows 8 references.
- The panel picks 16 steps / CFG 1 when a FLUX 3 model is selected, shows the FLUX 3 icon, enables the inpaint Edit toggle and allows Reference, Style, Composition, Face and Scribble/Line art/Canny/Depth/Pose layers (sent as references, no ControlNet).
