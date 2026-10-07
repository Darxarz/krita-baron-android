"""Server-side compiler. JSON stdin/stdout; never executes an inference job."""

import base64
import contextlib
import json
import os
import secrets
import sys
from io import TextIOWrapper
from pathlib import Path
from typing import cast

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "vendor"))
sys.path.insert(0, str(ROOT / "vendor/tests/mock"))
from ai_diffusion.backend import api, background, comfy_client, workflow
from ai_diffusion.backend.client import ClientModels
from ai_diffusion.backend.comfy_workflow import ComfyObjectInfo
from ai_diffusion.backend.resources import Arch, ControlMode
from ai_diffusion.files import File, FileCollection, FileLibrary, FileSource
from ai_diffusion.image import Bounds, Extent, Image, Mask
from ai_diffusion.settings import PerformanceSettings
from ai_diffusion.style import Style
from PyQt5.QtCore import QBuffer, QByteArray, QCoreApplication, QIODevice, Qt
from PyQt5.QtGui import QImage, QImageReader

APP = QCoreApplication.instance() or QCoreApplication([])


def integer(data, key, default, minimum, maximum):
    value = data.get(key, default)
    if (
        isinstance(value, bool)
        or not isinstance(value, (int, float))
        or int(value) != value
        or not minimum <= value <= maximum
    ):
        raise ValueError(f"Invalid {key}")
    return int(value)


def number(data, key, default, minimum, maximum):
    value = data.get(key, default)
    if (
        isinstance(value, bool)
        or not isinstance(value, (int, float))
        or not minimum <= value <= maximum
    ):
        raise ValueError(f"Invalid {key}")
    return float(value)


def decode(value):
    if not isinstance(value, str) or len(value) > 32 * 1024 * 1024:
        raise ValueError("Invalid image")
    raw = base64.b64decode(value, validate=True)
    buffer = QBuffer()
    buffer.setData(QByteArray(raw))
    buffer.open(QIODevice.OpenModeFlag.ReadOnly)
    size = QImageReader(buffer).size()
    if not size.isValid() or size.width() * size.height() > 16_777_216:
        raise ValueError("Image exceeds 16 megapixels or is invalid")
    image = Image.from_bytes(raw)
    if image.extent.pixel_count > 16_777_216:
        raise ValueError("Image exceeds 16 megapixels")
    return image


def guidance(data, extent, arch):
    controls, regions = data.get("controls", []), data.get("regions", [])
    if not isinstance(controls, list) or len(controls) > 16:
        raise ValueError("Too many control layers")
    if not isinstance(regions, list) or len(regions) > 8:
        raise ValueError("Too many regions")
    if regions and not arch.supports_regions:
        raise ValueError(f"Regional prompts are not supported by {arch.value}")
    global_control, regional_control = [], {}
    for control in controls:
        if not isinstance(control, dict):
            raise ValueError("Invalid control layer")
        name = control.get("mode")
        if (
            not isinstance(name, str)
            or name not in ControlMode.__members__
            or ControlMode[name].is_internal
        ):
            raise ValueError("Invalid control mode")
        start = number(control, "start", 0, 0, 1)
        end = number(control, "end", 1, 0, 1)
        if start > end:
            raise ValueError("Control start must not exceed its end")
        mode = ControlMode[name]
        image = decode(control.get("image"))
        if mode.is_lines or mode in (ControlMode.stencil, ControlMode.segmentation):
            image.make_opaque()
        if mode.is_ip_adapter:
            if arch.supports_edit:
                if image.extent.height > extent.height:
                    width = image.extent.width * extent.height // image.extent.height
                    image = Image.scale(image, Extent(width, extent.height))
            else:
                image = Image.scale(image, Extent(224, 224))
        elif image.extent != extent:
            image = Image.scale(image, extent)
        item = api.ControlInput(mode, image, number(control, "strength", 1, 0, 2), (start, end))
        region = control.get("region", "")
        if not isinstance(region, str):
            raise ValueError("Invalid control region")
        if region:
            regional_control.setdefault(region, []).append(item)
        else:
            global_control.append(item)
    result, seen = [], set()
    for region in regions:
        if not isinstance(region, dict):
            raise ValueError("Invalid region")
        id, prompt = region.get("id"), region.get("prompt", "")
        if not isinstance(id, str) or not id or id in seen:
            raise ValueError("Invalid or duplicate region ID")
        seen.add(id)
        if not isinstance(prompt, str) or len(prompt) > 32000:
            raise ValueError("Invalid region prompt")
        mask = decode(region.get("mask"))
        if mask.extent != extent:
            raise ValueError("Region mask must match the canvas or selected area")
        mask = Image(mask._qimage.convertToFormat(QImage.Format_Grayscale8))
        result.append(
            api.RegionInput(mask, Bounds.from_extent(extent), prompt, regional_control.pop(id, []))
        )
    if regional_control:
        raise ValueError("The control layer references a missing region")
    if result:
        result.insert(
            0,
            api.RegionInput(
                Image(
                    Image.create(extent, fill=Qt.GlobalColor.white)._qimage.convertToFormat(
                        QImage.Format_Grayscale8
                    )
                ),
                Bounds.from_extent(extent),
                data.get("prompt", ""),
            ),
        )
    return global_control, result


def discover(info, checkpoints=None, diffusion_models=None):
    nodes = ComfyObjectInfo(info)
    models = ClientModels()
    models.node_inputs = nodes
    lists = [
        ("_find_text_encoder_models", "DualCLIPLoader", "clip_name1"),
        ("_find_text_encoder_models", "CLIPLoader", "clip_name"),
        ("_find_vae_models", "VAELoader", "vae_name"),
        ("_find_control_models", "ControlNetLoader", "control_net_name"),
        ("_find_clip_vision_model", "CLIPVisionLoader", "clip_name"),
        ("_find_ip_adapters", "IPAdapterModelLoader", "ipadapter_file"),
        ("_find_model_patches", "ModelPatchLoader", "name"),
        ("_find_style_models", "StyleModelLoader", "style_model_name"),
        ("_find_upscalers", "UpscaleModelLoader", "model_name"),
        ("_find_inpaint_models", "INPAINT_LoadInpaintModel", "model_name"),
        ("_find_loras", "LoraLoader", "lora_name"),
    ]
    for finder, node, field in lists:
        models.resources.update(getattr(comfy_client, finder)(nodes.options(node, field)))
    models.upscalers = nodes.options("UpscaleModelLoader", "model_name")
    client = object.__new__(comfy_client.ComfyClient)
    client.models = models

    def clean(values):
        return {k: v for k, v in (values or {}).items() if k != "_meta"}

    client._refresh_models(nodes, clean(checkpoints) or None, clean(diffusion_models) or None)
    return models


def compile_request(payload):
    data, info = payload["request"], payload["object_info"]
    if not isinstance(data, dict) or not isinstance(info, dict):
        raise TypeError("Invalid request")
    models = discover(info, payload.get("checkpoints"), payload.get("diffusion_models"))
    if data.get("operation") == "discover":
        resources = {
            "checkpoints": {name: item.to_dict() for name, item in models.checkpoints.items()},
            "loras": models.loras,
            "vae": models.vae,
            "upscalers": models.upscalers,
            "resources": models.resources,
            "a1111_prompt": "smZ CLIPTextEncode" in info,
            "a1111_gpu_noise": "smZ Settings" in info,
        }
        items = [
            {
                "name": name,
                "title": Path(name).stem,
                "kind": item.format.name,
                "architecture": item.arch.name,
                "family": item.arch.name,
            }
            for name, item in models.checkpoints.items()
        ]
        items += [{"name": name, "title": Path(name).stem, "kind": "lora"} for name in models.loras]
        return {"items": items, "resources": resources}
    mode = data.get("mode")
    if mode not in ("generate", "edit", "upscale", "background"):
        raise ValueError("Unknown workspace")
    upscale_options = data.get("upscale_options", {})
    if not isinstance(upscale_options, dict):
        raise ValueError("Invalid upscale options")
    for key in ("use_diffusion", "use_prompt"):
        if key in upscale_options and not isinstance(upscale_options[key], bool):
            raise ValueError("Invalid upscale options")
    width = integer(data, "width", 1024, 1, 8192)
    height = integer(data, "height", 1024, 1, 8192)
    if width * height > 16_777_216:
        raise ValueError("Canvas exceeds 16 megapixels")
    source = (
        decode(data["image"])
        if mode != "generate" or data.get("mask") or data.get("operation") == "control_image"
        else None
    )
    if source and source.extent != Extent(width, height):
        raise ValueError("Canvas size does not match the image")
    metadata = {}
    if data.get("operation") == "control_image":
        name = data.get("control_mode")
        if (
            not isinstance(name, str)
            or name not in ControlMode.__members__
            or not ControlMode[name].has_preprocessor
        ):
            raise ValueError("This control mode has no preprocessor")
        if source is None:
            raise ValueError("Choose a source image for the control map")
        work = workflow.prepare_create_control_image(
            source, ControlMode[name], PerformanceSettings()
        )
    elif mode == "background":
        assert source is not None
        work = background.prepare(source, api.BackgroundRemovalInput(), models.node_inputs)
    elif mode == "upscale" and not upscale_options.get("use_diffusion", False):
        assert source is not None
        factor = number(data, "scale", 2, 1, 4)
        if width * height * factor * factor > 67_108_864:
            raise ValueError("Upscale exceeds 64 megapixels")
        upscaler = upscale_options.get("model") or models.default_upscaler
        if upscaler not in models.upscalers:
            raise ValueError("Upscale model is not installed on this server")
        work = workflow.prepare_upscale_simple(source, upscaler, factor)
    else:
        name = data.get("model")
        if name not in models.checkpoints:
            raise ValueError("Model is not installed on this server")
        arch = models.checkpoints[name].arch
        preset = (
            "qwen21.json"
            if arch is Arch.qwen2
            else "krea2-edit-baron.json"
            if arch is Arch.krea2
            else None
        )
        style = (
            Style.load(ROOT / "vendor/ai_diffusion/styles" / preset)
            if preset
            else Style(Path("native.json"))
        )
        if style is None:
            raise ValueError("Model preset could not be loaded")
        style.name = "Baron Android"
        style.checkpoints = [name]
        style.architecture = arch
        prompt_mode = data.get("prompt_mode", "comfy")
        if prompt_mode not in ("comfy", "a1111"):
            raise ValueError("Invalid prompt syntax")
        gpu_noise = data.get("a1111_gpu_noise", False)
        if not isinstance(gpu_noise, bool):
            raise ValueError("Invalid GPU noise option")
        if prompt_mode == "a1111" and not (arch is Arch.sd15 or arch.is_sdxl_like):
            raise ValueError("A1111 prompt syntax supports SD 1.5 and SDXL models only")
        style.prompt_mode = prompt_mode
        style.a1111_gpu_noise = gpu_noise and prompt_mode == "a1111"
        style.a1111_ensd = integer(data, "a1111_ensd", 0, 0, 4294967295)
        options = data.get("style_options", {})
        if options is None:
            options = {}
        if not isinstance(options, dict):
            raise ValueError("Invalid style options")
        for key in ("style_prompt", "negative_prompt"):
            value = options.get(key, "")
            if not isinstance(value, str) or len(value) > 32000:
                raise ValueError("Invalid style prompt")
            if key in options:
                setattr(style, key, value)
        for key in ("v_prediction_zsnr", "self_attention_guidance"):
            value = options.get(key, False)
            if not isinstance(value, bool):
                raise ValueError("Invalid style flag")
            if key in options:
                setattr(style, key, value)
        style.clip_skip = integer(options, "clip_skip", 0, 0, 12)
        style.preferred_resolution = integer(options, "preferred_resolution", 0, 0, 4096)
        vae = options.get("vae", "Checkpoint Default")
        if not isinstance(vae, str) or (vae != "Checkpoint Default" and vae not in models.vae):
            raise ValueError("Style VAE is not installed on this server")
        style.vae = vae
        purpose = data.get("reference_purpose", "identity")
        if purpose not in ("identity", "style"):
            raise ValueError("Invalid reference purpose")
        style.krea2_reference_mode = purpose
        style.krea2_ref_boost = number(data, "reference_fidelity", 4, 0, 12)
        style.krea2_grounding_px = integer(data, "reference_detail", 768, 512, 1024)
        style.sampler_steps = integer(
            data,
            "steps",
            25 if arch is Arch.qwen2 else 10 if arch is Arch.krea2 else 20,
            1,
            100,
        )
        style.cfg_scale = number(data, "cfg", 1 if arch in (Arch.qwen2, Arch.krea2) else 4, 0, 30)
        prompt, negative = data.get("prompt", ""), data.get("negative", "")
        if (
            not isinstance(prompt, str)
            or not isinstance(negative, str)
            or max(len(prompt), len(negative)) > 32000
        ):
            raise ValueError("Invalid prompt")
        refs = data.get("references", [])
        loras = data.get("loras", [])
        if (
            not isinstance(refs, list)
            or len(refs) > 8
            or not isinstance(loras, list)
            or len(loras) > 5
        ):
            raise ValueError("Too many references or LoRAs")
        for lora in loras:
            if not isinstance(lora, dict) or lora.get("name") not in models.loras:
                raise ValueError("LoRA is not installed on this server")
        controls, regions = guidance(data, Extent(width, height), arch)
        instruction_edit = data.get("instruction_edit", mode == "edit")
        if not isinstance(instruction_edit, bool):
            raise ValueError("Invalid edit mode")
        cond = api.ConditioningInput(
            prompt,
            negative,
            control=[api.ControlInput(ControlMode.reference, decode(image)) for image in refs]
            + controls,
            regions=regions,
            edit_reference=mode == "edit" and instruction_edit,
        )
        upscale = None
        factor = 1
        strength = number(data, "strength", 1, 0.01, 1)
        if mode == "upscale":
            assert source is not None
            upscale_options = data["upscale_options"]
            factor = number(data, "scale", 2, 1, 4)
            if width * height * factor * factor > 67_108_864:
                raise ValueError("Upscale exceeds 64 megapixels")
            upscaler = upscale_options.get("model") or models.default_upscaler
            if factor > 1 and upscaler not in models.upscalers:
                raise ValueError("Upscale model is not installed on this server")
            overlap_mode = upscale_options.get("tile_overlap_mode", "auto")
            if overlap_mode not in ("auto", "custom"):
                raise ValueError("Invalid tile overlap mode")
            overlap = integer(upscale_options, "tile_overlap", 48, 0, 128)
            upscale = api.UpscaleInput(upscaler, overlap if overlap_mode == "custom" else -1)
            strength = 1 if arch.is_edit else number(upscale_options, "strength", 0.3, 0, 1)
            if not upscale_options.get("use_prompt", False):
                cond = api.ConditioningInput(
                    "Enhance image quality. Preserve original content."
                    if arch.is_edit
                    else "4k uhd"
                )
            unblur = number(upscale_options, "unblur_strength", 0.5, 0, 1)
            if (
                not arch.is_edit
                and unblur > 0
                and models.for_arch(arch).find_control(ControlMode.blur)
            ):
                cond.control.append(api.ControlInput(ControlMode.blur, None, unblur))
        mask = decode(data["mask"]) if data.get("mask") else None
        if mask and (source is None or mask.extent != source.extent):
            raise ValueError("Selection size does not match the image")
        kind = (
            api.WorkflowKind.upscale_tiled
            if mode == "upscale"
            else api.WorkflowKind.inpaint
            if mode == "generate" and mask
            else api.WorkflowKind.generate
            if mode == "generate"
            else api.WorkflowKind.refine_region
            if mask
            else api.WorkflowKind.refine
        )
        inpaint = None
        mask_bounds = Bounds(0, 0, width, height)
        if mask:
            raw_bounds = data.get("selection_bounds", [0, 0, width, height])
            if (
                not isinstance(raw_bounds, list)
                or len(raw_bounds) != 4
                or any(isinstance(v, bool) or not isinstance(v, int) for v in raw_bounds)
            ):
                raise ValueError("Invalid selection bounds")
            mask_bounds = Bounds(*raw_bounds)
            if (
                mask_bounds.x < 0
                or mask_bounds.y < 0
                or mask_bounds.width <= 0
                or mask_bounds.height <= 0
                or mask_bounds.x + mask_bounds.width > width
                or mask_bounds.y + mask_bounds.height > height
            ):
                raise ValueError("Selection bounds exceed the context image")
            custom = data.get("inpaint_options")
            if custom is None:
                inpaint = api.InpaintParams(
                    api.InpaintMode.custom, mask_bounds, fill=api.FillMode.none
                )
            else:
                if not isinstance(custom, dict):
                    raise ValueError("Invalid inpaint options")
                try:
                    inpaint_mode = api.InpaintMode[custom.get("mode", "automatic")]
                    fill = api.FillMode[custom.get("fill", "neutral")]
                except (KeyError, TypeError):
                    raise ValueError("Invalid inpaint mode") from None
                if inpaint_mode is api.InpaintMode.automatic:
                    resolved = custom.get("resolved_mode")
                    if resolved is not None and resolved not in ("fill", "expand"):
                        raise ValueError("Invalid automatic inpaint mode")
                    inpaint_mode = (
                        api.InpaintMode[resolved]
                        if resolved
                        else workflow.detect_inpaint_mode(Extent(width, height), mask_bounds)
                    )
                if inpaint_mode is api.InpaintMode.custom:
                    inpaint = api.InpaintParams(
                        inpaint_mode, mask_bounds, api.FillMode.none if instruction_edit else fill
                    )
                    for field in ("use_inpaint_model", "use_condition_mask"):
                        flag = custom.get(field, field == "use_inpaint_model")
                        if not isinstance(flag, bool):
                            raise ValueError("Invalid inpaint flag")
                        setattr(inpaint, field, flag)
                else:
                    inpaint = workflow.detect_inpaint(
                        inpaint_mode, mask_bounds, arch, cond, strength
                    )
                for field in ("grow", "feather", "blend"):
                    setattr(inpaint, field, integer(custom, field, 0, 0, 499))
        seed = integer(data, "seed", secrets.randbits(32), 0, 4294967295)
        files = FileLibrary(FileCollection(), FileCollection())
        files.loras.update([File.remote(name) for name in models.loras], FileSource.remote)
        prompts = workflow.prepare_prompts(
            cond, style, seed, arch, inpaint.mode if inpaint else None, files=files
        )
        metadata = prompts.metadata
        work = workflow.prepare(
            kind,
            source or Extent(width, height),
            prompts.conditioning,
            style,
            seed,
            models,
            files,
            PerformanceSettings(
                batch_size=1,
                max_pixel_count=integer(data, "max_pixel_count", 6, 0, 16),
                resolution_multiplier=number(data, "resolution_multiplier", 1, 0.1, 2),
            ),
            mask=Mask(
                mask_bounds,
                mask._qimage.copy(*mask_bounds).convertToFormat(QImage.Format_Grayscale8),
            )
            if mask
            else None,
            strength=strength,
            upscale_factor=factor,
            upscale=upscale,
            loras=prompts.loras
            + [api.LoraInput(lora["name"], number(lora, "strength", 1, -2, 2)) for lora in loras],
            inpaint=inpaint,
        )
        work.batch_count = integer(data, "batch", 1, 1, 4)
        for field, node, option in (
            ("sampler", "KSampler", "sampler_name"),
            ("scheduler", "KSampler", "scheduler"),
        ):
            value = data.get(field, "")
            if not isinstance(value, str):
                raise ValueError(f"Invalid {field}")
            if value:
                choices = models.node_inputs.options(node, option)
                if value not in choices:
                    raise ValueError(f"The server does not support {field}: {value}")
                setattr(work.sampling, field, value)
    graph = workflow.create(work, models).embed_images().root
    for node in graph.values():
        if node["class_type"] == "PreviewImage":
            node["class_type"] = "SaveImage"
            node["inputs"]["filename_prefix"] = "baron_native"
    missing = sorted({node["class_type"] for node in graph.values()} - info.keys())
    if missing:
        raise ValueError("Install these ComfyUI nodes first: " + ", ".join(missing))
    return {
        "prompt": graph,
        "output": "foreground-and-mask" if mode == "background" else "images",
        "batch": work.batch_count,
        "metadata": metadata,
    }


if __name__ == "__main__":
    cast(TextIOWrapper, sys.stdin).reconfigure(encoding="utf-8")
    cast(TextIOWrapper, sys.stdout).reconfigure(encoding="utf-8")
    cast(TextIOWrapper, sys.stderr).reconfigure(encoding="utf-8")
    try:
        with contextlib.redirect_stdout(sys.stderr):
            result = compile_request(json.load(sys.stdin))
        print(json.dumps(result, separators=(",", ":")))
    except (
        ValueError,
        RuntimeError,
        AssertionError,
        LookupError,
        TypeError,
        OSError,
    ) as error:
        print(json.dumps({"error": str(error)}))
        sys.exit(1)
