import base64
import json
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "server"))
from compile_workflow import compile_request
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


if __name__ == "__main__":
    unittest.main()
