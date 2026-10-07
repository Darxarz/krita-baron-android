from collections import deque

from .api import WorkflowInput
from .comfy_workflow import ComfyObjectInfo, ComfyWorkflow
from .resources import Arch

encode_node = "smZ CLIPTextEncode"
settings_node = "smZ Settings"
sampling_nodes = {
    "KSampler",
    "KSamplerAdvanced",
    "CFGGuider",
    "BasicGuider",
    "DualCFGGuider",
    "DetailerForEach",
    "DetailerForEachDebug",
    "FaceDetailer",
}


def validate_prompt_mode(work: WorkflowInput, nodes: ComfyObjectInfo):
    if work.prompt_mode not in ("comfy", "a1111"):
        raise ValueError("Invalid prompt syntax")
    if work.prompt_mode != "a1111":
        return
    if not work.models or not (
        work.models.version is Arch.sd15 or work.models.version.is_sdxl_like
    ):
        raise ValueError("A1111 prompt syntax supports SD 1.5 and SDXL models only")
    required = [encode_node] + ([settings_node] if work.a1111_gpu_noise else [])
    missing = [name for name in required if name not in nodes]
    if missing:
        raise ValueError("Install ComfyUI_smZNodes on the server: " + ", ".join(missing))
    if not 0 <= work.a1111_ensd <= 4294967295:
        raise ValueError("ENSD must be between 0 and 4294967295")
    if work.sampling:
        schedulers = nodes.options("KSampler", "scheduler")
        if work.sampling.scheduler not in schedulers:
            raise ValueError(
                "A1111 prompt syntax requires a KSampler scheduler (for example normal or karras)"
            )


def _reference(value):
    return isinstance(value, (tuple, list)) and len(value) == 2 and isinstance(value[1], int)


def _consumers(graph):
    result: dict[str, list[tuple[str, str]]] = {}
    for id, node in graph.items():
        for name, value in node["inputs"].items():
            if _reference(value):
                result.setdefault(str(value[0]), []).append((id, name))
    return result


def _steps(graph, consumers, id, fallback):
    queue = deque([(id, 0)])
    seen = {id}
    while queue:
        id, depth = queue.popleft()
        for target, _ in consumers.get(id, []):
            if target in seen:
                continue
            seen.add(target)
            value = graph[target]["inputs"].get("steps")
            if isinstance(value, (int, float)) and value >= 1:
                return int(value)
            if depth < 4:
                queue.append((target, depth + 1))
    return fallback


def apply_prompt_mode(workflow: ComfyWorkflow, work: WorkflowInput):
    if work.prompt_mode != "a1111":
        return
    # Match services/a1111PromptMode.js in Orchestrion, including its smZ defaults.
    graph = workflow.root
    consumers = _consumers(graph)
    fallback = work.sampling.total_steps if work.sampling else 20
    for id, node in graph.items():
        if node["class_type"] != "CLIPTextEncode":
            continue
        text, clip = node["inputs"].get("text"), node["inputs"].get("clip")
        if not _reference(clip) or not (isinstance(text, str) or _reference(text)):
            continue
        refs = consumers.get(id, [])
        negative = bool(refs) and all(name in ("negative", "negative_cond") for _, name in refs)
        node["class_type"] = encode_node
        node["inputs"] = {
            "text": text,
            "clip": clip,
            "parser": "A1111",
            "mean_normalization": True,
            "multi_conditioning": not negative,
            "use_old_emphasis_implementation": False,
            "with_SDXL": False,
            "ascore": 6.0,
            "width": 1024,
            "height": 1024,
            "crop_w": 0,
            "crop_h": 0,
            "target_width": 1024,
            "target_height": 1024,
            "text_g": "",
            "text_l": "",
            "smZ_steps": _steps(graph, consumers, id, fallback),
        }
    if not work.a1111_gpu_noise:
        return
    wrapped = {}
    for node in list(graph.values()):
        model = node["inputs"].get("model")
        if node["class_type"] not in sampling_nodes or not _reference(model):
            continue
        key = tuple(model)
        if key not in wrapped:
            inputs = {
                "*": model,
                "extra": '{"show_headings":true,"show_descriptions":false,"mode":"*"}',
                "Prompt word wrap length limit": 20,
                "enable_emphasis": True,
                "RNG": "gpu",
                "disable_nan_check": True,
                "eta": 1.0,
                "s_churn": 0.0,
                "s_tmin": 0.0,
                "s_tmax": 0.0,
                "s_noise": 1.0,
                "ENSD": work.a1111_ensd,
                "skip_early_cond": 0.0,
                "sgm_noise_multiplier": False,
                "upcast_sampling": True,
                "NGMS": 0.0,
                "NGMS all steps": False,
                "pad_cond_uncond": False,
                "batch_cond_uncond": True,
                "Use previous prompt editing timelines": True,
                "Use CFGDenoiser": False,
                "debug": False,
            }
            wrapped[key] = workflow.add(settings_node, 1, **inputs)
        output = wrapped[key]
        node["inputs"]["model"] = [str(output.node), output.output]
