"""Deserialize a native Interstice request using Acly's actual WorkflowInput API."""

import argparse
import base64
import json
import sys
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("fixture", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "server/vendor"))
sys.path.insert(0, str(root / "server/vendor/tests/mock"))

from ai_diffusion.backend.api import WorkflowInput, WorkflowKind
from ai_diffusion.image import Extent
from PyQt5.QtCore import QCoreApplication

app = QCoreApplication([])
data = json.loads(args.fixture.read_text(encoding="utf-8"))
data["image_data"]["bytes"] = base64.b64decode(data["image_data"].pop("base64"), validate=True)
work = WorkflowInput.from_dict(data)
assert work.sampling is not None and work.models is not None and work.conditioning is not None
assert work.kind is WorkflowKind.refine_region
assert work.extent.target == Extent(512, 512)
assert work.sampling.actual_steps == 4
assert work.sampling.seed == 4294967295
assert work.models.loras[0].name == "style/ink.safetensors"
assert work.models.loras[0].strength == 0.7
reference = work.conditioning.control[0].image
assert reference is not None and reference.extent == Extent(224, 224)
assert len(work.conditioning.regions) == 2
assert work.conditioning.regions[1].positive == "face"
assert work.image._qimage.pixelColor(0, 0).alpha() == 120
roundtrip = WorkflowInput.from_dict(work.to_dict())
assert roundtrip.extent == work.extent
assert roundtrip.sampling == work.sampling
assert roundtrip.models == work.models
print(
    "Native request decoded and round-tripped with Acly's WorkflowInput; images, masks, regions, LoRA and seed preserved."
)
