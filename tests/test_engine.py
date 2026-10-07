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
                "prompt": "персонаж в красном плаще",
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
        self.assertIn("персонаж в красном плаще", json.dumps(graph, ensure_ascii=False))

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


if __name__ == "__main__":
    unittest.main()
