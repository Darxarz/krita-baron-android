"""ComfyUI transport for Acly's workflow compiler; does not enqueue inference."""

import asyncio
import importlib
import json
import sys
from pathlib import Path

from aiohttp import web

LIMIT = 48 * 1024 * 1024
OUTPUT_LIMIT = 64 * 1024 * 1024
POOL = asyncio.Semaphore(2)


def config():
    data = json.loads(Path(__file__).with_name("config.json").read_text(encoding="utf-8"))
    python, compiler = Path(data["python"]), Path(data["compiler"])
    if not python.is_absolute() or not compiler.is_absolute() or not python.is_file() or not compiler.is_file():
        raise ValueError("Configure the Baron workflow compiler in config.json")
    return python, compiler


async def read_limited(stream, maximum):
    result = bytearray()
    while chunk := await stream.read(65536):
        result.extend(chunk)
        if len(result) > maximum:
            raise ValueError("Compiler response is too large")
    return bytes(result)


async def compile_payload(payload):
    python, compiler = config()
    raw = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    if len(raw) > LIMIT:
        raise ValueError("Request is too large")
    async with POOL:
        process = await asyncio.create_subprocess_exec(
            str(python), str(compiler), stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE,
        )
        stdin = process.stdin
        assert stdin is not None and process.stdout is not None and process.stderr is not None

        async def communicate():
            stdin.write(raw)
            await stdin.drain()
            stdin.close()
            out, _ = await asyncio.gather(
                read_limited(process.stdout, OUTPUT_LIMIT), read_limited(process.stderr, 1024 * 1024)
            )
            await process.wait()
            data = json.loads(out)
            if not isinstance(data, dict):
                raise ValueError("Invalid compiler response")
            if data.get("error"):
                raise ValueError(str(data["error"])[:4000])
            if process.returncode:
                raise ValueError("Workflow compiler failed; inspect the server logs")
            return data

        try:
            return await asyncio.wait_for(communicate(), 90)
        finally:
            if process.returncode is None:
                process.kill()
                await process.wait()


async def snapshot(request):
    PromptServer = importlib.import_module("server").PromptServer

    for definition in PromptServer.instance.routes:
        if getattr(definition, "method", None) == "GET" and getattr(definition, "path", None) == "/object_info":
            response = await definition.handler(request)
            info = json.loads(response.body)
            break
    else:
        raise ValueError("ComfyUI object_info route is unavailable")
    inspector = next(
        (module for module in tuple(sys.modules.values())
         if "comfyui-tooling-nodes" in str(getattr(module, "__file__", ""))
         and callable(getattr(module, "inspect_models", None))), None
    )
    if inspector is None:
        raise ValueError("Install Acly's comfyui-tooling-nodes to detect model architectures")
    checkpoints, diffusion = await asyncio.gather(
        asyncio.to_thread(inspector.inspect_models, "checkpoints", {}),
        asyncio.to_thread(inspector.inspect_models, "diffusion_models", {}),
    )
    return {"object_info": info, "checkpoints": json.loads(checkpoints.body),
            "diffusion_models": json.loads(diffusion.body)}


async def handle(request, discover=False):
    try:
        if discover:
            data = {"operation": "discover"}
        else:
            raw = bytearray()
            async for chunk in request.content.iter_chunked(65536):
                raw.extend(chunk)
                if len(raw) > LIMIT:
                    return web.json_response({"error": "Request is too large"}, status=413)
            data = json.loads(raw)
            if not isinstance(data, dict):
                raise ValueError("Invalid request")
        payload = await snapshot(request)
        payload["request"] = data
        return web.json_response(await compile_payload(payload))
    except (ValueError, KeyError, OSError, TimeoutError, json.JSONDecodeError) as error:
        return web.json_response({"error": str(error)[:4000]}, status=400)


async def models(request):
    return await handle(request, True)


async def prepare(request):
    return await handle(request)
