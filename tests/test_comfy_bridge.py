import asyncio
import importlib.util
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch

from aiohttp import web

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "baron_bridge", ROOT / "server/comfyui-baron-native/bridge.py"
)
assert spec is not None and spec.loader is not None
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


class BridgeTests(unittest.IsolatedAsyncioTestCase):
    async def test_subprocess_uses_server_config_and_returns_compiler_error(self):
        with tempfile.TemporaryDirectory() as folder:
            script = Path(folder) / "compiler.py"
            script.write_text(
                "import json,sys\ndata=json.load(sys.stdin)\n"
                'print(json.dumps({"prompt":data["request"]}))\n',
                encoding="utf-8",
            )
            with patch.object(bridge, "config", return_value=(Path(sys.executable), script)):
                result = await bridge.compile_payload(
                    {"request": {"python": "untrusted", "mode": "edit"}}
                )
                self.assertEqual(result["prompt"]["mode"], "edit")
            script.write_text(
                'import sys\nprint(\'{"error":"Missing ControlNet model"}\')\nsys.exit(1)\n',
                encoding="utf-8",
            )
            with patch.object(bridge, "config", return_value=(Path(sys.executable), script)):
                with self.assertRaisesRegex(ValueError, "Missing ControlNet"):
                    await bridge.compile_payload({})

    async def test_bounded_output(self):
        reader = asyncio.StreamReader()
        reader.feed_data(b"too large")
        reader.feed_eof()
        with self.assertRaisesRegex(ValueError, "too large"):
            await bridge.read_limited(reader, 4)

    async def test_snapshot_uses_registered_comfy_routes_and_loaded_tooling(self):
        async def object_info(_):
            return web.json_response({"KSampler": {"input": {}}})

        module = types.ModuleType("tooling")
        module.__file__ = "custom_nodes/comfyui-tooling-nodes/api.py"
        setattr(
            module,
            "inspect_models",
            lambda kind, _: web.json_response({kind: {"base_model": "sdxl"}}),
        )
        server = types.ModuleType("server")
        setattr(
            server,
            "PromptServer",
            types.SimpleNamespace(
                instance=types.SimpleNamespace(
                    routes=[
                        types.SimpleNamespace(
                            method="GET", path="/object_info", handler=object_info
                        )
                    ]
                )
            ),
        )
        with patch.dict(sys.modules, {"server": server, "baron_test_tooling": module}):
            data = await bridge.snapshot(None)
        self.assertIn("KSampler", data["object_info"])
        self.assertEqual(data["diffusion_models"]["diffusion_models"]["base_model"], "sdxl")

    async def test_routes_do_not_submit_inference_or_execute_client_paths(self):
        async def snapshot(_):
            return {"object_info": {}}

        async def compile_payload(payload):
            self.assertEqual(payload["request"], {"operation": "discover"})
            return {"items": []}

        with (
            patch.object(bridge, "snapshot", snapshot),
            patch.object(bridge, "compile_payload", compile_payload),
        ):
            result = await bridge.models(None)
        self.assertEqual(result.status, 200)
        self.assertEqual(json.loads(result.body), {"items": []})


if __name__ == "__main__":
    unittest.main()
