import base64
import json
import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "server"))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "server/vendor"))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "server/vendor/tests/mock"))
from ai_diffusion.backend.resources import Arch
from ai_diffusion.image import Extent
from compile_workflow import compile_request, guidance, workflow
from PyQt5.QtCore import QBuffer, QIODevice
from PyQt5.QtGui import QImage, qRgba

ROOT = Path(__file__).resolve().parents[1]


def image(mask=False):
    value = QImage(512, 512, QImage.Format_Grayscale8 if mask else QImage.Format_ARGB32)
    value.fill(0 if mask else qRgba(110, 70, 130, 160))
    if mask:
        for y in range(180, 330):
            for x in range(180, 330):
                value.setPixel(x, y, qRgba(255, 255, 255, 255))
    data = QBuffer()
    data.open(QIODevice.OpenModeFlag.WriteOnly)
    value.save(data, "PNG")
    return base64.b64encode(bytes(data.data())).decode()


class EngineTests(unittest.TestCase):
    def test_compositing_preserves_manual_feather_and_plugin_blend(self):
        mask = workflow.Output(100, 0)
        for blend in (0, 1, 5, 20):
            graph = workflow.ComfyWorkflow()
            params = workflow.InpaintParams(workflow.InpaintMode.custom, workflow.Bounds(0, 0, 64, 64))
            params.blend = blend
            result = workflow.denoise_to_compositing_mask(graph, mask, params)
            if blend == 0:
                self.assertEqual(result, mask)
                self.assertEqual(graph.root, {})
            else:
                self.assertEqual(graph.root["1"]["class_type"], "ThresholdMask")
                self.assertEqual(graph.root["2"]["inputs"]["blur"], blend)

    def test_direct_comfy_discovery_uses_original_model_detection(self):
        result = compile_request(
            {
                "request": {"operation": "discover"},
                "object_info": self.info,
                "diffusion_models": self.metadata,
            }
        )
        items = result["items"]
        names = {item["name"]: item for item in items}
        self.assertEqual(names[self.qwen]["architecture"], "qwen2")
        self.assertEqual(names[self.krea]["architecture"], "krea2")
        self.assertTrue(all(item["kind"] in ("checkpoint", "diffusion", "lora") for item in items))
        self.assertIn(self.qwen, result["resources"]["checkpoints"])
        self.assertNotIn("prompt", result)

    def test_control_pixels_match_plugin_reference_preprocessing(self):
        source = QImage(300, 600, QImage.Format_ARGB32)
        source.fill(qRgba(0, 0, 0, 0))
        buffer = QBuffer()
        buffer.open(QIODevice.OpenModeFlag.WriteOnly)
        source.save(buffer, "PNG")
        raw = base64.b64encode(bytes(buffer.data())).decode()
        data = {"controls": [{"mode": "reference", "image": raw}]}
        controls, _ = guidance(data, Extent(512, 512), Arch.sdxl)
        self.assertEqual(controls[0].image.extent, Extent(224, 224))
        controls, _ = guidance(data, Extent(512, 512), Arch.qwen2)
        self.assertEqual(controls[0].image.extent, Extent(256, 512))
        for mode in ("scribble", "line_art", "soft_edge", "canny_edge", "stencil", "segmentation"):
            with self.subTest(mode=mode):
                controls, _ = guidance(
                    {"controls": [{"mode": mode, "image": raw}]}, Extent(300, 600), Arch.sdxl
                )
                self.assertEqual(
                    controls[0].image._qimage.pixelColor(0, 0).getRgb(), (255, 255, 255, 255)
                )

    @classmethod
    def setUpClass(cls):
        folder = ROOT / "tests/fixtures"
        cls.info = json.loads((folder / "object_info.json").read_text(encoding="utf-8"))
        cls.metadata = json.loads((folder / "model_info.json").read_text(encoding="utf-8"))
        cls.qwen = next(name for name in cls.metadata if "Qwen" in name)
        cls.krea = next(name for name in cls.metadata if "krea2Turbo18" in name)

    def compile(self, request):
        return compile_request(
            {
                "request": {
                    "width": 512,
                    "height": 512,
                    "prompt": "change the clothes while preserving identity",
                    "seed": 123,
                    **request,
                },
                "object_info": self.info,
                "diffusion_models": self.metadata,
            }
        )["prompt"]

    def test_generation_uses_requested_model_and_batch(self):
        for model in (self.qwen, self.krea):
            with self.subTest(model=model):
                graph = self.compile({"mode": "generate", "model": model, "batch": 3})
                inputs = [node["inputs"] for node in graph.values()]
                self.assertTrue(any(model in values.values() for values in inputs))
                self.assertTrue(any(values.get("batch_size") == 3 for values in inputs))
                self.assertTrue(any(node["class_type"] == "SaveImage" for node in graph.values()))
                self.assertFalse(any("Cache" in node["class_type"] for node in graph.values()))

    def test_null_style_options_use_default_model_preset(self):
        for model in (self.qwen, self.krea):
            with self.subTest(model=model):
                default = self.compile({"mode": "generate", "model": model})
                legacy = self.compile({"mode": "generate", "model": model, "style_options": None})
                self.assertEqual(legacy, default)
        for invalid in ([], "", 0, False):
            with (
                self.subTest(invalid=invalid),
                self.assertRaisesRegex(ValueError, "Invalid style options"),
            ):
                self.compile({"mode": "generate", "model": self.qwen, "style_options": invalid})

    def test_imported_style_prompt_and_flags_are_used(self):
        prepare = workflow.prepare
        with patch.object(workflow, "prepare", wraps=prepare) as captured:
            self.compile(
                {
                    "mode": "generate",
                    "model": self.qwen,
                    "style_options": {"style_prompt": "watercolor", "clip_skip": 2},
                }
            )
        args = captured.call_args.args
        self.assertEqual(args[2].style, "watercolor")
        self.assertEqual(args[3].clip_skip, 2)
        for options in (
            {"vae": "missing.safetensors"},
            {"clip_skip": 100},
            {"self_attention_guidance": "true"},
            {"style_prompt": []},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.compile({"mode": "generate", "model": self.qwen, "style_options": options})

    def test_low_strength_matches_the_plugin_slider(self):
        self.compile({"mode": "edit", "model": self.qwen, "image": image(), "strength": 0.01})
        with self.assertRaises(ValueError):
            self.compile({"mode": "edit", "model": self.qwen, "image": image(), "strength": 0})

    def test_edit_references_and_selection_for_both_families(self):
        for model in (self.qwen, self.krea):
            with self.subTest(model=model):
                graph = self.compile(
                    {
                        "mode": "edit",
                        "model": model,
                        "image": image(),
                        "mask": image(True),
                        "references": [image()],
                        "strength": 0.7,
                    }
                )
                classes = [node["class_type"] for node in graph.values()]
                self.assertIn("ETN_LoadImageBase64", classes)
                self.assertIn("ETN_LoadMaskBase64", classes)
                self.assertIn("SaveImage", classes)
                if model == self.krea:
                    self.assertTrue(any("lora_name" in node["inputs"] for node in graph.values()))

    def test_krea_v2_adapter_and_reference_placement(self):
        import copy

        for purpose in ("identity_v2", "general_v2"):
            info = copy.deepcopy(self.info)
            family = "identity" if purpose == "identity_v2" else "general"
            adapter = f"krea_v2/krea2_{family}_edit_v2_r128.safetensors"
            info["LoraLoader"]["input"]["required"]["lora_name"][0].append(adapter)
            info["Krea2EditModelPatch"]["input"].setdefault("optional", {}).update(
                {key: [["aligned (trained)", "beside (right of target)", "bag of features"]]
                 for key in ("ref_position", "ref_position_a")}
            )
            request = {"mode": "edit", "model": self.krea, "image": image(), "width": 512, "height": 512,
                       "reference_purpose": purpose, "reference_placement": "beside (right of target)"}
            graph = compile_request({"request": request, "object_info": info,
                                     "diffusion_models": self.metadata})["prompt"]
            patch = next(n["inputs"] for n in graph.values() if n["class_type"] == "Krea2EditModelPatch")
            self.assertEqual(patch["ref_position"], "beside (right of target)")
            self.assertEqual(patch["fit_mode"], "fit")
            self.assertTrue(any(n["inputs"].get("lora_name") == adapter for n in graph.values()))

    def test_invalid_reference_purpose_or_placement_is_rejected(self):
        for options in ({"reference_purpose": "missing"}, {"reference_placement": "missing"}):
            with self.assertRaisesRegex(ValueError, "Invalid reference"):
                self.compile({"mode": "edit", "model": self.krea, "image": image(), **options})

    def test_background_is_person_only_with_chair_excluded(self):
        graph = self.compile({"mode": "background", "image": image()})
        classes = [node["class_type"] for node in graph.values()]
        self.assertIn("BiRefNetRMBG", classes)
        self.assertIn("SegmDetectorSEGS", classes)
        detector = next(node for node in graph.values() if node["class_type"] == "SegmDetectorSEGS")
        self.assertEqual(detector["inputs"]["labels"], "person")
        self.assertTrue(
            any(
                node["class_type"] == "MaskComposite" and node["inputs"]["operation"] == "multiply"
                for node in graph.values()
            )
        )

    def test_upscale_and_validation(self):
        graph = self.compile({"mode": "upscale", "image": image(), "scale": 2})
        self.assertTrue(
            any(node["class_type"] == "ImageUpscaleWithModel" for node in graph.values())
        )
        for request in (
            {"mode": "generate", "model": "missing"},
            {"mode": "generate", "model": self.qwen, "batch": 100},
            {"mode": "edit", "model": self.qwen, "image": "invalid"},
            {"mode": "background", "image": image(), "width": 500},
            {
                "mode": "generate",
                "model": self.krea,
                "loras": [{"name": "missing", "strength": 1}],
            },
        ):
            with (
                self.subTest(request=request),
                self.assertRaises((ValueError, AssertionError)),
            ):
                self.compile(request)

    def test_tiled_upscale_matches_original_workflow(self):
        from ai_diffusion.backend import api

        for factor in (1, 1.5, 2):
            with (
                self.subTest(factor=factor),
                patch.object(workflow, "create", wraps=workflow.create) as created,
            ):
                self.compile(
                    {
                        "mode": "upscale",
                        "model": self.qwen,
                        "image": image(),
                        "scale": factor,
                        "upscale_options": {
                            "use_diffusion": True,
                            "strength": 0.3,
                            "tile_overlap_mode": "custom",
                            "tile_overlap": 64,
                        },
                    }
                )
                work = created.call_args.args[0]
                self.assertIs(work.kind, api.WorkflowKind.upscale_tiled)
                self.assertEqual(work.extent.target, Extent(512, 512) * factor)
                self.assertEqual(work.extent.input, Extent(512, 512))
                self.assertEqual(work.extent.desired, Extent(896, 896))
                self.assertEqual(work.upscale.tile_overlap, 64)
                self.assertAlmostEqual(work.sampling.denoise_strength, 0.3, delta=1 / 25)
                self.assertEqual(bool(work.upscale.model), factor > 1)
                self.assertEqual(work.conditioning.positive, "4k uhd")

    def test_upscale_model_and_prompt_are_used_and_validated(self):
        chosen = "OmniSR_X4_DIV2K.safetensors"
        for refine in (False, True):
            with (
                self.subTest(refine=refine),
                patch.object(workflow, "create", wraps=workflow.create) as created,
            ):
                self.compile(
                    {
                        "mode": "upscale",
                        "model": self.qwen,
                        "image": image(),
                        "scale": 2,
                        "prompt": "watercolor city",
                        "upscale_options": {
                            "model": chosen,
                            "use_diffusion": refine,
                            "use_prompt": True,
                        },
                    }
                )
                work = created.call_args.args[0]
                self.assertEqual(work.upscale.model, chosen)
                if refine:
                    self.assertEqual(work.conditioning.positive, "watercolor city")
        for options in (None, [], {"use_diffusion": "true"}, {"model": "missing"}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.compile({"mode": "upscale", "image": image(), "upscale_options": options})

    def test_resolution_slider_changes_inference_but_preserves_canvas(self):
        from ai_diffusion.backend.resolution import apply_resolution_settings
        from ai_diffusion.settings import PerformanceSettings

        extents = []
        for multiplier in (0.3, 0.5, 0.8, 1, 1.5):
            with (
                self.subTest(multiplier=multiplier),
                patch.object(workflow, "create", wraps=workflow.create) as created,
            ):
                self.compile(
                    {
                        "mode": "generate",
                        "model": self.qwen,
                        "width": 3072,
                        "height": 2048,
                        "resolution_multiplier": multiplier,
                        "max_pixel_count": 6,
                    }
                )
                work = created.call_args.args[0]
                self.assertEqual(work.extent.target, Extent(3072, 2048))
                expected = apply_resolution_settings(
                    Extent(3072, 2048),
                    PerformanceSettings(resolution_multiplier=multiplier, max_pixel_count=6),
                ).multiple_of(32)
                self.assertEqual(work.extent.desired, expected)
                extents.append(work.extent.desired.pixel_count)
        self.assertEqual(extents[:3], sorted(extents[:3]))
        self.assertLess(extents[0], extents[2])

    def test_cli_transports_russian_prompt(self):
        payload = {
            "request": {
                "mode": "generate",
                "model": self.qwen,
                "width": 512,
                "height": 512,
                "prompt": "РїРµСЂСЃРѕРЅР°Р¶ РІ РєСЂР°СЃРЅРѕРј РїР»Р°С‰Рµ",
                "seed": 42,
            },
            "object_info": self.info,
            "diffusion_models": self.metadata,
        }
        process = subprocess.run(
            [sys.executable, str(ROOT / "server/compile_workflow.py")],
            input=json.dumps(payload, ensure_ascii=False),
            encoding="utf-8",
            capture_output=True,
            timeout=20,
            check=True,
        )
        graph = json.loads(process.stdout)["prompt"]
        self.assertIn("РїРµСЂСЃРѕРЅР°Р¶ РІ РєСЂР°СЃРЅРѕРј РїР»Р°С‰Рµ", json.dumps(graph, ensure_ascii=False))

    def test_custom_inpaint_matches_plugin_flags_and_output_bounds(self):
        from ai_diffusion.backend import api
        from ai_diffusion.image import Bounds

        for editing in (False, True):
            for fill in ("none", "neutral", "blur", "border", "inpaint"):
                with (
                    self.subTest(editing=editing, fill=fill),
                    patch.object(workflow, "prepare", wraps=workflow.prepare) as prepare,
                ):
                    self.compile(
                        {
                            "mode": "edit" if editing else "generate",
                            "model": self.qwen,
                            "image": image(),
                            "mask": image(True),
                            "selection_bounds": [96, 96, 320, 320],
                            "inpaint_options": {
                                "mode": "custom",
                                "fill": fill,
                                "use_inpaint_model": False,
                                "use_condition_mask": True,
                            },
                        }
                    )
                    params = prepare.call_args.kwargs["inpaint"]
                    self.assertEqual(params.target_bounds, Bounds(96, 96, 320, 320))
                    self.assertEqual(
                        params.fill, api.FillMode.none if editing else api.FillMode[fill]
                    )
                    self.assertFalse(params.use_inpaint_model)
                    self.assertTrue(params.use_condition_mask)
                    mask = prepare.call_args.kwargs["mask"]
                    self.assertEqual(mask.bounds, params.target_bounds)
                    self.assertEqual(mask.to_image().extent.width, 320)

    def test_rejects_out_of_context_mask_and_invalid_custom_flags(self):
        for options in (
            {"selection_bounds": [-1, 0, 512, 512]},
            {"selection_bounds": [0, 0, 513, 512]},
            {"selection_bounds": [0, 0, 1.5, 512]},
            {"inpaint_options": {"mode": "bad"}},
            {"inpaint_options": {"mode": "custom", "use_inpaint_model": "yes"}},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.compile(
                    {
                        "mode": "generate",
                        "model": self.qwen,
                        "image": image(),
                        "mask": image(True),
                        **options,
                    }
                )

    def test_generate_in_selection_and_sampling_override(self):
        for model in (self.qwen, self.krea):
            graph = self.compile(
                {
                    "mode": "generate",
                    "model": model,
                    "image": image(),
                    "mask": image(True),
                    "sampler": "euler",
                    "scheduler": "simple",
                    "steps": 13,
                    "batch": 2,
                }
            )
            self.assertTrue(
                any(node["class_type"] == "ETN_LoadMaskBase64" for node in graph.values())
            )
            self.assertTrue(
                any(node["inputs"].get("sampler_name") == "euler" for node in graph.values())
            )
            self.assertTrue(
                any(node["inputs"].get("scheduler") == "simple" for node in graph.values())
            )
        with self.assertRaises(ValueError):
            self.compile({"mode": "generate", "model": self.qwen, "sampler": "not-installed"})

    def test_resolution_multiplier_preserves_target_size(self):
        outputs = []
        for multiplier in (0.5, 1, 2):
            prepared = []
            original = workflow.prepare

            def capture(*args, **kwargs):
                work = original(*args, **kwargs)
                prepared.append(work)
                return work

            with patch.object(workflow, "prepare", side_effect=capture):
                graph = self.compile(
                    {"mode": "generate", "model": self.qwen, "resolution_multiplier": multiplier}
                )
            outputs.append(json.dumps(graph, sort_keys=True))
            self.assertEqual(prepared[0].extent.target, Extent(512, 512))
        self.assertNotEqual(outputs[0], outputs[2])

    def test_controls_and_regions_are_scoped_and_validated(self):
        data = {
            "prompt": "whole scene",
            "controls": [{"mode": "reference", "image": image(), "region": "r1", "strength": 0.7}],
            "regions": [{"id": "r1", "mask": image(True), "prompt": "red clothes"}],
        }
        controls, regions = guidance(data, Extent(512, 512), Arch.sdxl)
        self.assertEqual(len(controls), 0)
        self.assertEqual(len(regions), 2)
        self.assertEqual(regions[0].positive, "whole scene")
        self.assertEqual(regions[1].control[0].strength, 0.7)
        with self.assertRaisesRegex(ValueError, "not supported"):
            guidance(data, Extent(512, 512), Arch.qwen2)
        data["controls"][0]["region"] = "missing"
        with self.assertRaisesRegex(ValueError, "missing region"):
            guidance(data, Extent(512, 512), Arch.sdxl)

    def test_unified_reference_modes_use_model_instructions(self):
        graph = self.compile(
            {
                "mode": "edit",
                "model": self.qwen,
                "image": image(),
                "controls": [{"mode": "composition", "image": image(), "strength": 0.8}],
                "instruction_edit": True,
            }
        )
        self.assertIn("Maintain the structure and composition", json.dumps(graph))

    def test_control_preprocessor_creates_map_and_rejects_missing_nodes(self):
        graph = self.compile(
            {
                "mode": "generate",
                "operation": "control_image",
                "control_mode": "canny_edge",
                "image": image(),
            }
        )
        self.assertTrue(
            any(node["class_type"] == "CannyEdgePreprocessor" for node in graph.values())
        )
        with patch.dict(self.info, clear=True):
            with self.assertRaises((ValueError, LookupError)):
                self.compile(
                    {
                        "mode": "generate",
                        "operation": "control_image",
                        "control_mode": "canny_edge",
                        "image": image(),
                    }
                )

    def test_a1111_full_graph_and_optional_gpu_noise(self):
        model = next(
            name
            for name in self.info["CheckpointLoaderSimple"]["input"]["required"]["ckpt_name"][0]
            if "illustrious" in name.lower()
        )
        for noise in (False, True):
            graph = self.compile(
                {
                    "mode": "generate",
                    "model": model,
                    "prompt_mode": "a1111",
                    "a1111_gpu_noise": noise,
                    "a1111_ensd": 31337,
                    "prompt": "[night:day:0.5] AND (watercolor:1.3) BREAK portrait",
                    "negative": "blur AND grain",
                }
            )
            self.assertTrue(
                any(node["class_type"] == "smZ CLIPTextEncode" for node in graph.values())
            )
            self.assertFalse(
                any(
                    node["class_type"] in ("CLIPTextEncode", "SamplerCustomAdvanced")
                    for node in graph.values()
                )
            )
            samplers = [node for node in graph.values() if node["class_type"] == "KSamplerAdvanced"]
            self.assertTrue(samplers)
            settings = [node for node in graph.values() if node["class_type"] == "smZ Settings"]
            self.assertEqual(bool(settings), noise)
            if noise:
                for sampler in samplers:
                    wrapped = graph[sampler["inputs"]["model"][0]]
                    self.assertEqual(wrapped["class_type"], "smZ Settings")
                    self.assertEqual(wrapped["inputs"]["RNG"], "gpu")
                    self.assertEqual(wrapped["inputs"]["ENSD"], 31337)
        for data in (
            {"model": self.qwen},
            {"model": model, "a1111_gpu_noise": "true"},
            {"model": model, "a1111_ensd": -1},
        ):
            with self.assertRaises(ValueError):
                self.compile({"mode": "generate", "prompt_mode": "a1111", **data})

    moss = "moss\\moss_image_sft_int8_convrot.safetensors"
    moss_lora = "moss\\characters\\Baron_Satyr_MOSS.safetensors"

    def moss_server(self, clip_type=True):
        import copy

        info = copy.deepcopy(self.info)
        required = lambda node: info[node]["input"]["required"]  # noqa: E731
        required("UNETLoader")["unet_name"][0].append(self.moss)
        required("CLIPLoader")["clip_name"][0] += [
            "qwen3.5_9b_qwen_image_2.1_pe_t2i.int8_convrot.safetensors",
            "qwen3.5_9b_w4a8_gptq.safetensors",
        ]
        types = required("CLIPLoader")["type"][0]
        if clip_type and "moss_image" not in types:
            types.append("moss_image")
        elif not clip_type and "moss_image" in types:
            types.remove("moss_image")
        required("LoraLoader")["lora_name"][0].append(self.moss_lora)
        metadata = {**self.metadata, self.moss: {"base_model": "moss-image", "is_inpaint": False}}
        return info, metadata

    def compile_moss(self, request, clip_type=True):
        info, metadata = self.moss_server(clip_type)
        payload = {"width": 1024, "height": 1024, "prompt": 'a sign that says "MOSS"', "seed": 7}
        return compile_request(
            {
                "request": {**payload, "model": self.moss, **request},
                "object_info": info,
                "diffusion_models": metadata,
            }
        )

    def test_moss_discovery_requires_moss_clip_type(self):
        for clip_type in (True, False):
            with self.subTest(clip_type=clip_type):
                info, metadata = self.moss_server(clip_type)
                result = compile_request(
                    {
                        "request": {"operation": "discover"},
                        "object_info": info,
                        "diffusion_models": metadata,
                    }
                )
                names = {item["name"]: item for item in result["items"]}
                if clip_type:
                    self.assertEqual(names[self.moss]["architecture"], "moss")
                else:
                    self.assertNotIn(self.moss, names)

    def test_moss_generate_uses_model_preset(self):
        graph = self.compile_moss({"mode": "generate", "negative": "blurry"})["prompt"]
        nodes = {node["class_type"]: node["inputs"] for node in graph.values()}
        self.assertEqual(nodes["UNETLoader"]["unet_name"], self.moss)
        self.assertEqual(nodes["CLIPLoader"]["clip_name"], "qwen3.5_9b_w4a8_gptq.safetensors")
        self.assertEqual(nodes["CLIPLoader"]["type"], "moss_image")
        self.assertEqual(nodes["VAELoader"]["vae_name"], "flux2-vae.safetensors")
        self.assertEqual(nodes["SkipLayerGuidanceDiTSimple"]["single_layers"], "22")
        self.assertIn("EmptyFlux2LatentImage", nodes)
        self.assertEqual(nodes["KSamplerSelect"]["sampler_name"], "euler")
        self.assertEqual(nodes["BasicScheduler"]["scheduler"], "sgm_uniform")
        self.assertEqual(nodes["BasicScheduler"]["steps"], 30)
        self.assertAlmostEqual(nodes["CFGGuider"]["cfg"], 3)
        self.assertFalse(any(t.startswith("ModelSampling") for t in nodes))
        texts = [n["inputs"]["text"] for n in graph.values() if n["class_type"] == "CLIPTextEncode"]
        self.assertIn("blurry", texts)

    def test_moss_sampling_override_and_model_only_lora(self):
        request = {
            "mode": "generate",
            "steps": 24,
            "cfg": 4.5,
            "loras": [{"name": self.moss_lora, "strength": 0.7}],
        }
        graph = self.compile_moss(request)["prompt"]
        nodes = list(graph.values())
        scheduler = next(n["inputs"] for n in nodes if n["class_type"] == "BasicScheduler")
        self.assertEqual(scheduler["steps"], 24)
        guider = next(n["inputs"] for n in nodes if n["class_type"] == "CFGGuider")
        self.assertAlmostEqual(guider["cfg"], 4.5)
        loras = [n["inputs"] for n in nodes if n["class_type"] == "LoraLoaderModelOnly"]
        self.assertEqual([(l["lora_name"], l["strength_model"]) for l in loras], [(self.moss_lora, 0.7)])
        self.assertFalse(any(n["class_type"] == "LoraLoader" for n in nodes))

    def test_moss_selection_uses_differential_diffusion(self):
        for mode, inpaint in (("generate", None), ("edit", {"mode": "custom", "fill": "neutral"})):
            with self.subTest(mode=mode):
                request = {"mode": mode, "width": 512, "height": 512, "image": image(),
                           "mask": image(True), "instruction_edit": False}
                if inpaint:
                    request["inpaint_options"] = inpaint
                graph = self.compile_moss(request)["prompt"]
                types = {node["class_type"] for node in graph.values()}
                self.assertIn("DifferentialDiffusion", types)
                self.assertIn("SetLatentNoiseMask", types)
                self.assertFalse(any("ControlNet" in t or "Fooocus" in t for t in types))
                self.assertFalse(any(t.startswith("ModelSampling") for t in types))

    def test_moss_regions_become_layout_prompt(self):
        def rect_mask(x0, y0, x1, y1):
            value = QImage(1024, 1024, QImage.Format_Grayscale8)
            value.fill(0)
            for y in range(y0, y1):
                for x in range(x0, x1):
                    value.setPixel(x, y, qRgba(255, 255, 255, 255))
            data = QBuffer()
            data.open(QIODevice.OpenModeFlag.WriteOnly)
            value.save(data, "PNG")
            return base64.b64encode(bytes(data.data())).decode()

        girl = "a red-haired girl in a green cloak"
        sign = 'a wooden tavern sign that says "TAVERN"'
        request = {
            "mode": "generate",
            "prompt": "улица в сумерках",
            "negative": "blurry",
            "regions": [
                {"id": "girl", "mask": rect_mask(40, 100, 460, 900), "prompt": girl},
                {"id": "sign", "mask": rect_mask(560, 50, 960, 350), "prompt": f"object: {sign}"},
            ],
        }
        graph = self.compile_moss(request)["prompt"]
        types = {node["class_type"] for node in graph.values()}
        self.assertFalse(types & {"ETN_AttentionMask", "ETN_DefineRegion", "ETN_BackgroundRegion"})
        texts = [n["inputs"]["text"] for n in graph.values() if n["class_type"] == "CLIPTextEncode"]
        self.assertIn("blurry", texts)
        layouts = [t for t in texts if t.startswith("{")]
        self.assertEqual(len(layouts), 1)
        sign_box = [50 - 512, 560 - 512, 350 - 512, 960 - 512]
        self.assertEqual(
            json.loads(layouts[0]),
            {
                "summary": {"caption": "улица в сумерках"},
                "items": [
                    {"label": "character", "caption": girl, "bbox": [-412, -472, 388, -52]},
                    {"label": "object", "caption": sign, "bbox": sign_box},
                    {"label": "text", "caption": sign, "text": "TAVERN", "bbox": sign_box},
                ],
                "bbox_coord_space": "abs_centralized",
                "bbox_format": "yxyx",
            },
        )
        self.assertIn("улица", layouts[0])  # ensure_ascii=False, like MOSSImageLayoutPrompt

        # Regions are characters by default; two or more get the box position instead of
        # left/right words. Quoted text adds a text item whatever the label.
        bakery = "a bakery sign that says \"BARON'S BAKERY\""
        request["regions"] = [
            {"id": "satyr", "mask": rect_mask(40, 260, 480, 1000), "prompt": "a tall satyr on the right"},
            {"id": "nymph", "mask": rect_mask(600, 260, 1000, 1000), "prompt": "a nymph on the left"},
            {"id": "sign", "mask": rect_mask(300, 20, 720, 220), "prompt": bakery},
        ]
        graph = self.compile_moss(request)["prompt"]
        texts = [n["inputs"]["text"] for n in graph.values() if n["class_type"] == "CLIPTextEncode"]
        items = json.loads(next(t for t in texts if t.startswith("{")))["items"]
        self.assertEqual(items[0]["bbox"], [-252, -472, 488, -32])
        self.assertEqual(
            [(i["label"], i["caption"], i.get("text")) for i in items],
            [
                ("character", "a tall satyr, on the left of the frame", None),
                ("character", "a nymph, on the right of the frame", None),
                ("character", f"{bakery}, in the center", None),
                ("text", bakery, "BARON'S BAKERY"),
            ],
        )

    flux3 = "flux3\\flux3-image-klein-int8-convrot.safetensors"
    flux3_lora = "flux3\\Baron_Satyr_FLUX3.safetensors"

    def flux3_server(self, clip_type=True, reference_node=True):
        import copy

        info = copy.deepcopy(self.info)
        if reference_node:  # /object_info/ReferenceMultiLatent on ComfyUI 0.41.0, tooltips removed
            megapixels = ["FLOAT", {"default": 1.0, "min": 0.01, "max": 16.0, "step": 0.01}]
            resolution = [
                {"key": "keep resolution", "inputs": {"required": {}}},
                {"key": "resize", "inputs": {"required": {"megapixels": megapixels}}},
            ]
            images = {"input": {"required": {}, "optional": {"image": ["IMAGE", {}]}}}
            info["ReferenceMultiLatent"] = {
                "input": {
                    "required": {
                        "positive": ["CONDITIONING", {}],
                        "resolution": ["COMFY_DYNAMICCOMBO_V3", {"options": resolution}],
                    },
                    "optional": {
                        "negative": ["CONDITIONING", {}],
                        "vae": ["VAE", {}],
                        "images": [
                            "COMFY_AUTOGROW_V3",
                            {"template": {**images, "prefix": "image", "min": 1, "max": 100}},
                        ],
                    },
                },
                "output": ["CONDITIONING", "CONDITIONING", "LATENT"],
                "output_name": ["positive", "negative", "empty_latent"],
            }
        required = lambda node: info[node]["input"]["required"]  # noqa: E731
        required("UNETLoader")["unet_name"][0].append(self.flux3)
        types = required("CLIPLoader")["type"][0]
        if clip_type and "flux3" not in types:
            types.append("flux3")
        elif not clip_type and "flux3" in types:
            types.remove("flux3")
        required("LoraLoader")["lora_name"][0].append(self.flux3_lora)
        # As reported by comfyui-tooling-nodes on ComfyUI 0.41.0
        metadata = {**self.metadata, self.flux3: {"base_model": "flux3-klein", "is_inpaint": False}}
        return info, metadata

    def compile_flux3(self, request, clip_type=True, reference_node=True):
        info, metadata = self.flux3_server(clip_type, reference_node)
        payload = {"width": 1024, "height": 1024, "prompt": "a red fox in fresh snow", "seed": 7}
        return compile_request(
            {
                "request": {**payload, "model": self.flux3, **request},
                "object_info": info,
                "diffusion_models": metadata,
            }
        )

    def test_flux3_discovery_requires_flux3_clip_type(self):
        for clip_type in (True, False):
            with self.subTest(clip_type=clip_type):
                info, metadata = self.flux3_server(clip_type)
                result = compile_request(
                    {
                        "request": {"operation": "discover"},
                        "object_info": info,
                        "diffusion_models": metadata,
                    }
                )
                names = {item["name"]: item for item in result["items"]}
                if clip_type:
                    self.assertEqual(names[self.flux3]["architecture"], "flux3")
                else:
                    self.assertNotIn(self.flux3, names)

    def test_flux3_generate_uses_model_preset(self):
        graph = self.compile_flux3({"mode": "generate", "negative": "blurry"})["prompt"]
        nodes = {node["class_type"]: node["inputs"] for node in graph.values()}
        self.assertEqual(nodes["UNETLoader"]["unet_name"], self.flux3)
        self.assertEqual(nodes["CLIPLoader"]["clip_name"], "qwen3vl_4b_fp8_scaled.safetensors")
        self.assertEqual(nodes["CLIPLoader"]["type"], "flux3")
        self.assertEqual(nodes["VAELoader"]["vae_name"], "flux2-vae.safetensors")
        self.assertIn("EmptyFlux2LatentImage", nodes)
        self.assertEqual(nodes["KSamplerSelect"]["sampler_name"], "euler")
        self.assertEqual(nodes["BasicScheduler"]["scheduler"], "simple")
        self.assertEqual(nodes["BasicScheduler"]["steps"], 16)
        self.assertNotIn("CFGGuider", nodes)  # CFG 1
        self.assertIn("BasicGuider", nodes)
        self.assertFalse(any(t.startswith("ModelSampling") for t in nodes))
        texts = [n["inputs"]["text"] for n in graph.values() if n["class_type"] == "CLIPTextEncode"]
        self.assertEqual(texts, ["a red fox in fresh snow"])  # no negative prompt

    def test_flux3_cfg_override_zeroes_negative_and_model_only_lora(self):
        request = {
            "mode": "generate",
            "steps": 24,
            "cfg": 2,
            "negative": "blurry",
            "loras": [{"name": self.flux3_lora, "strength": 0.7}],
        }
        graph = self.compile_flux3(request)["prompt"]
        nodes = list(graph.values())
        scheduler = next(n["inputs"] for n in nodes if n["class_type"] == "BasicScheduler")
        self.assertEqual(scheduler["steps"], 24)
        guider = next(n["inputs"] for n in nodes if n["class_type"] == "CFGGuider")
        self.assertAlmostEqual(guider["cfg"], 2)
        self.assertEqual(graph[guider["negative"][0]]["class_type"], "ConditioningZeroOut")
        loras = [n["inputs"] for n in nodes if n["class_type"] == "LoraLoaderModelOnly"]
        self.assertEqual([(l["lora_name"], l["strength_model"]) for l in loras], [(self.flux3_lora, 0.7)])
        self.assertFalse(any(n["class_type"] == "LoraLoader" for n in nodes))

    def test_flux3_edit_references_use_reference_multi_latent(self):
        request = {
            "mode": "edit",
            "width": 512,
            "height": 512,
            "image": image(),
            "references": [image(), image()],
            "prompt": "put the jacket from <ref_image_1> on the person in <ref_image_0>",
        }
        graph = self.compile_flux3(request)["prompt"]
        refs = [n["inputs"] for n in graph.values() if n["class_type"] == "ReferenceMultiLatent"]
        self.assertEqual(len(refs), 1)
        self.assertEqual(refs[0]["resolution"], "resize")
        self.assertAlmostEqual(refs[0]["resolution.megapixels"], 1.0)
        images = sorted(k for k in refs[0] if k.startswith("images."))
        self.assertEqual(images, ["images.image0", "images.image1", "images.image2"])
        self.assertEqual(graph[refs[0]["negative"][0]]["class_type"], "ConditioningZeroOut")
        self.assertFalse(any(n["class_type"] == "ReferenceLatent" for n in graph.values()))
        texts = [n["inputs"]["text"] for n in graph.values() if n["class_type"] == "CLIPTextEncode"]
        self.assertEqual(texts, [request["prompt"]])
        with self.assertRaisesRegex(ValueError, "ReferenceMultiLatent"):
            self.compile_flux3(request, reference_node=False)

    def test_flux3_selection_uses_differential_diffusion_and_high_strength(self):
        for mode, inpaint in (("generate", None), ("edit", {"mode": "custom", "fill": "neutral"})):
            with self.subTest(mode=mode):
                request = {"mode": mode, "width": 512, "height": 512, "image": image(),
                           "mask": image(True), "instruction_edit": False, "strength": 0.4}
                if inpaint:
                    request["inpaint_options"] = inpaint
                graph = self.compile_flux3(request)["prompt"]
                types = {node["class_type"] for node in graph.values()}
                self.assertIn("DifferentialDiffusion", types)
                self.assertIn("SetLatentNoiseMask", types)
                self.assertFalse(any("ControlNet" in t or "Fooocus" in t for t in types))
                self.assertNotIn("ReferenceMultiLatent", types)
                if mode == "edit":  # refine at 0.4 is raised to 0.9: 16 steps start at step 2
                    scheduler = next(
                        n["inputs"] for n in graph.values() if n["class_type"] == "BasicScheduler"
                    )
                    self.assertEqual(scheduler["steps"], 16)
                    split = next(
                        n["inputs"] for n in graph.values() if n["class_type"] == "SplitSigmas"
                    )
                    self.assertEqual(split["step"], 2)

    def test_flux3_regions_become_layout_table(self):
        def rect_mask(x0, y0, x1, y1):
            value = QImage(1024, 1024, QImage.Format_Grayscale8)
            value.fill(0)
            for y in range(y0, y1):
                for x in range(x0, x1):
                    value.setPixel(x, y, qRgba(255, 255, 255, 255))
            data = QBuffer()
            data.open(QIODevice.OpenModeFlag.WriteOnly)
            value.save(data, "PNG")
            return base64.b64encode(bytes(data.data())).decode()

        girl = "a red-haired girl in a green cloak"
        sign = 'a wooden tavern sign that says "TAVERN"'
        prompt = "улица в сумерках, <obj_1> под <obj_2>"
        request = {
            "mode": "generate",
            "prompt": prompt,
            "regions": [
                {"id": "girl", "mask": rect_mask(40, 100, 460, 900), "prompt": girl},
                {"id": "sign", "mask": rect_mask(560, 50, 960, 350), "prompt": f"text: {sign}"},
            ],
        }
        graph = self.compile_flux3(request)["prompt"]
        types = {node["class_type"] for node in graph.values()}
        self.assertFalse(types & {"ETN_AttentionMask", "ETN_DefineRegion", "ETN_BackgroundRegion"})
        texts = [n["inputs"]["text"] for n in graph.values() if n["class_type"] == "CLIPTextEncode"]
        rows = [
            {"id": "obj_1", "bbox": [98, 39, 879, 449], "desc": girl},
            {"id": "obj_2", "bbox": [49, 547, 342, 938], "desc": sign},
        ]
        # Same text as ComfyUI's Flux3LayoutPrompt (json.dumps with ensure_ascii=False)
        self.assertEqual(texts, [f"{prompt} {json.dumps(rows, ensure_ascii=False)}"])


if __name__ == "__main__":
    unittest.main()
