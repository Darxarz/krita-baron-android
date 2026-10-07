"""Native Android request preparation; original workflow implementation by Acly."""

from server import PromptServer

from . import bridge

PromptServer.instance.routes.get("/baron/native/models")(bridge.models)
PromptServer.instance.routes.post("/baron/native/prepare")(bridge.prepare)
NODE_CLASS_MAPPINGS = {}
NODE_DISPLAY_NAME_MAPPINGS = {}
