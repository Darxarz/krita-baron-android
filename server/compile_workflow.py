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
from ai_diffusion.files import FileCollection, FileLibrary
from ai_diffusion.image import Bounds, Extent, Image, Mask
from ai_diffusion.settings import PerformanceSettings
from ai_diffusion.style import Style
from PyQt5.QtCore import QBuffer, QByteArray, QCoreApplication, QIODevice
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
    mode = data.get("mode")
    if mode not in ("generate", "edit", "upscale", "background"):
        raise ValueError("Unknown workspace")
    width = integer(data, "width", 1024, 64, 8192)
    height = integer(data, "height", 1024, 64, 8192)
    if width * height > 16_777_216:
        raise ValueError("Canvas exceeds 16 megapixels")
    source = decode(data["image"]) if mode != "generate" else None
    if source and source.extent != Extent(width, height):
        raise ValueError("Canvas size does not match the image")
    if mode == "background":
        assert source is not None
        work = background.prepare(source, api.BackgroundRemovalInput(), models.node_inputs)
    elif mode == "upscale":
        assert source is not None
        factor = number(data, "scale", 2, 1, 4)
        if width * height * factor * factor > 67_108_864:
            raise ValueError("Upscale exceeds 64 megapixels")
        work = workflow.prepare_upscale_simple(source, models.default_upscaler, factor)
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
        cond = api.ConditioningInput(
            prompt,
            negative,
            control=[api.ControlInput(ControlMode.reference, decode(image)) for image in refs],
            edit_reference=mode == "edit",
        )
        mask = decode(data["mask"]) if mode == "edit" and data.get("mask") else None
        if mask and (source is None or mask.extent != source.extent):
            raise ValueError("Selection size does not match the image")
        kind = (
            api.WorkflowKind.generate
            if mode == "generate"
            else api.WorkflowKind.refine_region
            if mask
            else api.WorkflowKind.refine
        )
        inpaint = (
            api.InpaintParams(
                api.InpaintMode.custom,
                Bounds(0, 0, width, height),
                fill=api.FillMode.none,
            )
            if mask
            else None
        )
        work = workflow.prepare(
            kind,
            source or Extent(width, height),
            cond,
            style,
            integer(data, "seed", secrets.randbits(32), 0, 4294967295),
            models,
            FileLibrary(FileCollection(), FileCollection()),
            PerformanceSettings(batch_size=1, max_pixel_count=4),
            mask=Mask(
                Bounds(0, 0, width, height),
                mask._qimage.convertToFormat(QImage.Format_Grayscale8),
            )
            if mask
            else None,
            strength=number(data, "strength", 1, 0.05, 1),
            loras=[
                api.LoraInput(lora["name"], number(lora, "strength", 1, -2, 2)) for lora in loras
            ],
            inpaint=inpaint,
        )
        work.batch_count = integer(data, "batch", 1, 1, 4) if mode == "generate" else 1
    graph = workflow.create(work, models).embed_images().root
    for node in graph.values():
        if node["class_type"] == "PreviewImage":
            node["class_type"] = "SaveImage"
            node["inputs"]["filename_prefix"] = "baron_native"
    return {
        "prompt": graph,
        "output": "foreground-and-mask" if mode == "background" else "images",
        "batch": work.batch_count,
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
