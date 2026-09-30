from __future__ import annotations

import asyncio
import base64
import ctypes
import ipaddress
import sys
from urllib.parse import urlsplit, urlunsplit

from PyQt5.QtCore import QByteArray

from ..localization import translate as _
from .api import WorkflowInput
from .client import User
from .comfy_client import ComfyClient
from .network import RequestManager
from .resources import Arch


def service_url(value: str):
    parts = urlsplit(value.strip().rstrip("/"))
    if parts.scheme not in ("https", "http") or not parts.hostname:
        raise ValueError(_("Enter the website address: https://orchestrion.su"))
    if parts.username or parts.password or parts.query or parts.fragment:
        raise ValueError(_("Enter the website address without a token, password or parameters"))
    if parts.scheme == "http":
        try:
            local = ipaddress.ip_address(parts.hostname).is_private
        except ValueError:
            local = parts.hostname == "localhost"
        if not local:
            raise ValueError(_("Internet connections require HTTPS"))
    if parts.path not in ("", "/"):
        raise ValueError(_("Enter the website root address without /krita/TOKEN"))
    return urlunsplit((parts.scheme, parts.netloc, "", "", ""))


def protect_token(token: str, decrypt=False):
    if not token:
        return ""
    if sys.platform != "win32":
        raise ValueError(_("Persistent Orchestrion login currently requires Windows"))

    class Blob(ctypes.Structure):
        _fields_ = [("size", ctypes.c_uint32), ("data", ctypes.POINTER(ctypes.c_ubyte))]

    raw = base64.b64decode(token.removeprefix("dpapi:")) if decrypt else token.encode()
    buffer = ctypes.create_string_buffer(raw)
    source = Blob(len(raw), ctypes.cast(buffer, ctypes.POINTER(ctypes.c_ubyte)))
    result = Blob()
    crypt = ctypes.windll.crypt32
    fn = crypt.CryptUnprotectData if decrypt else crypt.CryptProtectData
    fn.argtypes = [
        ctypes.POINTER(Blob),
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.POINTER(Blob),
    ]
    fn.restype = ctypes.c_int
    if not fn(ctypes.byref(source), None, None, None, None, 1, ctypes.byref(result)):
        raise ValueError(_("Cannot open the saved login. Sign in through your browser again."))
    try:
        data = ctypes.string_at(result.data, result.size)
        return data.decode() if decrypt else "dpapi:" + base64.b64encode(data).decode()
    finally:
        ctypes.windll.kernel32.LocalFree.argtypes = [ctypes.c_void_p]
        ctypes.windll.kernel32.LocalFree(result.data)


def quote_text(quote: dict, count=1):
    coins = float(quote["coins"]) * count
    return _("≈ {amount} bleatbucks", amount=f"{coins:g}")


def estimate_prompt(work: WorkflowInput):
    # Pricing uses the largest rendering/output extent. No canvas pixels or
    # user prompts are transferred just to display a preliminary price.
    extents = [work.extent.initial, work.extent.desired, work.extent.target]
    if work.crop_upscale_extent:
        extents.append(work.crop_upscale_extent)
    size = max(extents, key=lambda e: e.pixel_count)
    return {
        "1": {
            "class_type": "EmptyLatentImage",
            "inputs": {"width": size.width, "height": size.height, "batch_size": work.batch_count},
        }
    }


def lora_compatible(family: str, arch: Arch) -> bool | None:
    value = family.lower().replace(" ", "").replace("-", "").replace("_", "")
    if "krea" in value:
        return arch is Arch.krea2
    if "qwen21" in value or "qwenimage2.1" in value:
        return arch is Arch.qwen2
    if "qwen" in value:
        return arch in (Arch.qwen, Arch.qwen_e, Arch.qwen_e_p, Arch.qwen_l)
    if any(name in value for name in ("sdxl", "illustrious", "noobai", "pony")):
        return arch.is_sdxl_like
    if "sd1.5" in value or "sd15" in value:
        return arch is Arch.sd15
    if "flux2" in value:
        return arch in (Arch.flux2_4b, Arch.flux2_9b)
    if "flux" in value:
        return arch in (Arch.flux, Arch.flux_k)
    if "zimage" in value:
        return arch is Arch.zimage
    if "anima" in value:
        return arch is Arch.anima
    return None


class OrchestrionAPI:
    def __init__(self, url: str, token=""):
        self.url = service_url(url)
        self.requests = RequestManager(same_origin_only=True)
        self.requests.set_auth(token)

    async def sign_in(self):
        data = await self.requests.post(f"{self.url}/api/krita/device/start", {})
        yield data
        interval = max(5, int(data["interval"]))
        deadline = asyncio.get_running_loop().time() + min(600, int(data["expires_in"]))
        while asyncio.get_running_loop().time() < deadline:
            await asyncio.sleep(interval)
            result = await self.requests.post(
                f"{self.url}/api/krita/device/poll", {"device_code": data["device_code"]}
            )
            error = result.get("error")
            if error == "authorization_pending":
                continue
            if error == "slow_down":
                interval += 5
                continue
            if error:
                raise ValueError(_("Connection denied or code expired. Start sign-in again."))
            yield result["access_token"]
            return
        raise ValueError(_("Sign-in timed out. Start again."))

    async def account(self):
        return await self.requests.get(f"{self.url}/api/krita/account", timeout=15)

    async def catalog(self):
        return (await self.requests.get(f"{self.url}/api/krita/models", timeout=30))["items"]

    async def quote(self, prompt: dict):
        return await self.requests.http(
            "POST",
            f"{self.url}/api/krita/quote",
            {"prompt": prompt},
            timeout=15,
        )

    async def logout(self):
        await self.requests.http("POST", f"{self.url}/api/krita/logout", {}, timeout=15)


class DeviceRequests(RequestManager):
    def __init__(self, token: str):
        super().__init__(same_origin_only=True)
        self.token = token
        self.set_auth(token)

    async def upload(
        self,
        url: str,
        data: QByteArray | bytes,
        sha256: str | None = None,
        bearer: str | None = None,
    ):
        async for progress in super().upload(url, data, sha256, bearer=self.token):
            yield progress


class OrchestrionClient(ComfyClient):
    def __init__(self, url: str, token: str):
        self.api = OrchestrionAPI(url, token)
        self.account_data: dict = {}
        self._user: User | None = None
        super().__init__(self.api.url + "/krita/connection", token)
        # ComfyClient normally uses the custom server credential for HTTP only.
        # This transport uses the same dedicated device key for HTTP and WS.
        self._requests = DeviceRequests(token)

    @property
    def user(self):
        return self._user

    async def refresh_account(self):
        self.account_data = await self.api.account()
        if self._user is None:
            self._user = User(self.account_data["id"], self.account_data["name"])
        self._user.credits = round(self.account_data["coins"])
        return self.account_data

    async def connect(self):
        await self.refresh_account()
        await super().connect()

    async def refresh(self):
        await super().refresh()
        await self.refresh_account()
