# Krea 2 Edit V2 — personal evaluation

The node pack is Apache-2.0. The model weights have separate terms: see [the model author's V2 license](https://huggingface.co/conradlocke/krea2-identity-edit/blob/main/LICENSE_V2.md). No weights are bundled with Baron Edition.

## Choosing a task

- **Identity Edit V2**: preserve a person/character while changing pose, clothes, or scene.
- **General Edit V2**: instruction-based edits: recolor, relight, change background, add or remove objects.
- **Identity**: existing V1.x behavior remains unchanged.
- **Style**: existing style-reference workflow remains unchanged.

Use one functional Identity/General Edit LoRA at a time. Character/style LoRAs may be added separately. In Styles, the new V2 presets automatically select the installed R128, R64, then full-rank adapter. An explicitly selected matching adapter takes precedence.

## References and placement

The canvas is the first reference in editing mode. An added reference becomes the second. Two references total are supported (canvas included).

- **Automatic**: a single edit image stays aligned; Identity V2 places the second/character image beside the canvas for restaging. General V2 stays aligned.
- **Aligned with canvas**: preserves spatial arrangement.
- **Beside canvas**: allows a new composition while using the subject reference.
- **Bag of features**: experimental upstream option; quality has not been verified by Baron Edition.

For a character in a scene, use the scene on the canvas and the character as the added reference. Describe the requested edit as an instruction and identify each image's role.

Suggested starting settings from the author: Turbo, Euler/simple, 8 steps, CFG 1, grounding 768; fidelity 2 for Identity V2 and 4 for General V2. For difficult removal edits the author recommends Raw, about 20 steps, CFG 3.

## Requirements

ComfyUI with native Krea 2, its Qwen3-VL encoder and VAE, and [comfyui-krea2edit 2.0.0 or newer](https://github.com/lbouaraba/comfyui-krea2edit). Baron checks the reference-placement inputs before accepting a V2 request. A server with old nodes gives a clear update message rather than a broken graph.

The personal installation is separate from the shared site's model folders. Merely updating the plugin or APK does not add or license V2 weights on a shared service.
