from dataclasses import replace
from secrets import randbits

from ..image import Image
from ..localization import translate as _
from .api import BackgroundRemovalInput, ImageInput, WorkflowInput, WorkflowKind
from .comfy_workflow import ComfyObjectInfo, ComfyWorkflow


def available_models(nodes: ComfyObjectInfo):
    result = []
    if "BiRefNetRMBG" in nodes:
        result.extend(nodes.options("BiRefNetRMBG", "model"))
    if "RMBG" in nodes:
        result.extend(nodes.options("RMBG", "model"))
    return result


def validate(params: BackgroundRemovalInput, nodes: ComfyObjectInfo):
    required = {"SplitImageWithAlpha", "MaskToImage", "ImageBatch", "ImageAddNoise"}
    if params.isolate_subject:
        required.update({"MaskComposite", "GrowMask"})
        if params.subject_detector != "text":
            required.update({
                "UltralyticsDetectorProvider",
                "SegmDetectorSEGS",
                "SegsToCombinedMask",
            })
            if params.refine_subject:
                required.update({"SAMLoader", "SAMDetectorCombined"})
    if params.edge_offset:
        required.add("GrowMask")
    if params.edge_blur:
        required.add("MaskBlur+")
    if missing := sorted(required.difference(nodes.nodes)):
        raise RuntimeError(
            _("Background separation requires server nodes: {nodes}", nodes=", ".join(missing))
        )
    if params.model not in available_models(nodes):
        raise RuntimeError(_("Background removal requires ComfyUI-RMBG and the selected model"))
    if params.isolate_subject:
        if params.subject_detector == "text":
            if "Segment" not in nodes:
                raise RuntimeError(_("Text isolation requires Segment and GroundingDINO"))
        elif not params.subject_detector.startswith(
            "segm/"
        ) or params.subject_detector not in nodes.options(
            "UltralyticsDetectorProvider", "model_name"
        ):
            raise RuntimeError(_("Person isolation requires an installed YOLO segmentation model"))
        if (
            params.subject_detector != "text"
            and params.refine_subject
            and params.sam_model not in nodes.options("SAMLoader", "model_name")
        ):
            raise RuntimeError(
                _("Contour refinement requires the selected SAM model on the server")
            )
        if params.subject_detector == "text" and not params.subject.strip():
            raise ValueError(_("Describe the subject to keep, for example: person"))
    if not 0.05 <= params.threshold <= 0.95:
        raise ValueError(_("Detection threshold must be between 0.05 and 0.95"))
    if not 0 <= params.detail_margin <= 32:
        raise ValueError(_("Detail margin must be between 0 and 32 pixels"))
    if not 256 <= params.process_resolution <= 2048:
        raise ValueError(_("Processing resolution must be between 256 and 2048"))
    if not -20 <= params.edge_offset <= 20 or not 0 <= params.edge_blur <= 8:
        raise ValueError(_("Edge offset or softness is outside the supported range"))


def prepare(image: Image, params: BackgroundRemovalInput, nodes: ComfyObjectInfo):
    validate(params, nodes)
    images = ImageInput.from_extent(image.extent)
    images.initial_image = image
    return WorkflowInput(
        WorkflowKind.remove_background,
        images,
        background_removal=replace(params, output_seed=randbits(32)),
    )


def create(w: ComfyWorkflow, image: Image, params: BackgroundRemovalInput):
    validate(params, w.node_defs)
    source = w.load_image(image)
    options = {
        "image": source,
        "model": params.model,
        "mask_blur": 0,
        "mask_offset": 0,
        "invert_output": False,
        "refine_foreground": params.refine_foreground,
        "background": "Alpha",
        "background_color": "#222222",
    }
    if params.model.startswith("BiRefNet"):
        matte, mask, _ = w.add("BiRefNetRMBG", 3, **options)
    else:
        matte, mask, _ = w.add(
            "RMBG", 3, **options, sensitivity=1.0, process_res=params.process_resolution
        )
    foreground, _ = w.add("SplitImageWithAlpha", 2, image=matte)

    def segment(prompt: str):
        _, selected, _ = w.add(
            "Segment",
            3,
            image=source,
            prompt=prompt,
            sam_model="sam_vit_b (375MB)",
            dino_model="GroundingDINO_SwinT_OGC (694MB)",
            threshold=params.threshold,
            mask_blur=0,
            mask_offset=0,
            invert_output=False,
            background="Alpha",
            background_color="#222222",
        )
        return selected

    def combine(destination, source, operation):
        return w.add(
            "MaskComposite",
            1,
            destination=destination,
            source=source,
            x=0,
            y=0,
            operation=operation,
        )

    if params.isolate_subject:
        if params.subject_detector == "text":
            selected = segment(params.subject.strip())
        else:
            _, detector = w.add(
                "UltralyticsDetectorProvider", 2, model_name=params.subject_detector
            )
            segments = w.add(
                "SegmDetectorSEGS",
                1,
                segm_detector=detector,
                image=source,
                threshold=params.threshold,
                dilation=0,
                crop_factor=1.0,
                drop_size=1,
                labels="person",
            )
            selected = w.add("SegsToCombinedMask", 1, segs=segments)
            if params.refine_subject:
                sam = w.add("SAMLoader", 1, model_name=params.sam_model, device_mode="AUTO")
                selected = w.add(
                    "SAMDetectorCombined",
                    1,
                    sam_model=sam,
                    segs=segments,
                    image=source,
                    detection_hint="mask-area",
                    dilation=0,
                    threshold=0.93,
                    bbox_expansion=8,
                    mask_hint_threshold=0.7,
                    mask_hint_use_negative="Outter",
                )
        if params.detail_margin:
            selected = w.add(
                "GrowMask", 1, mask=selected, expand=params.detail_margin, tapered_corners=True
            )
        mask = combine(mask, selected, "multiply")
        if params.subject_detector == "text" and params.exclude.strip():
            mask = combine(mask, segment(params.exclude.strip()), "subtract")
    if params.edge_offset:
        mask = w.add("GrowMask", 1, mask=mask, expand=params.edge_offset, tapered_corners=True)
    if params.edge_blur:
        mask = w.add("MaskBlur+", 1, mask=mask, amount=params.edge_blur, device="cpu")
    # A single ordered batch avoids dependence on output-node execution order.
    batch = w.batch_image(foreground, w.mask_to_image(mask))
    # Strength zero leaves every pixel unchanged while refreshing the output cache.
    # Expensive segmentation/matting remains cached. ETN image IDs expire and are
    # omitted from ComfyUI history, so a fully cached output cannot be retrieved.
    batch = w.add("ImageAddNoise", 1, image=batch, seed=params.output_seed, strength=0.0)
    w.send_image(batch)
    return w
