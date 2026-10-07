"""Exchange actual Python plugin annotations with the native Qt history test."""

import argparse
import base64
import json
import sys
import tempfile
from dataclasses import asdict
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--plugin-root", type=Path, required=True)
parser.add_argument("--phase", choices=("prepare", "verify"), required=True)
parser.add_argument("--fixture", type=Path, required=True)
parser.add_argument("--prompt-file", type=Path)
args = parser.parse_args()
sys.path.insert(0, str(args.plugin_root))
sys.path.insert(0, str(args.plugin_root / "tests/mock"))

from PyQt5.QtCore import QByteArray, QCoreApplication

app = QCoreApplication([])

from ai_diffusion.image import Bounds, Extent, Image, ImageCollection
from ai_diffusion.model.jobs import JobKind, JobParams
from ai_diffusion.model.model import (
    DocumentModel,  # noqa: F401 - initialize the model before persistence
)
from ai_diffusion.persistence import ModelSync, _HistoryResult
from ai_diffusion.util import encode_json

from krita import Krita

if args.phase == "prepare":
    prompt = (
        args.prompt_file.read_text(encoding="utf-8").removesuffix("\n")
        if args.prompt_file
        else "python character"
    )
    images = ImageCollection(
        [
            Image.create(Extent(32, 24), fill=0xFFFF0000),
            Image.create(Extent(32, 24), fill=0x46112233),
        ]
    )
    blob, offsets = images.to_bytes()
    params = JobParams(
        Bounds(11, 29, 32, 24),
        prompt,
        seed=4294967295,
        metadata={"prompt": prompt, "negative_prompt": "blurry", "strength": 0.43},
    )
    history = _HistoryResult("python-job", 0, offsets, params, JobKind.diffusion, {1: True})
    state = {
        "version": 1,
        "root": {"positive": "python document prompt"},
        "history": [asdict(history)],
        "custom": {},
        "control": [],
        "regions": [],
    }
    annotations = {
        "ai_diffusion/ui.json": json.dumps(state, default=encode_json).encode(),
        "ai_diffusion/result0.webp": bytes(blob),
    }
    args.fixture.parent.mkdir(parents=True, exist_ok=True)
    args.fixture.write_text(
        json.dumps({k: base64.b64encode(v).decode() for k, v in annotations.items()})
    )
    print("Python WebP batch fixture exported.")
else:
    from ai_diffusion.settings import settings
    from tests.test_persistence import _make_model  # pyright: ignore[reportMissingImports]

    document = Krita.instance().openDocument("native history exchange")
    annotations = json.loads(args.fixture.read_text())
    for key, encoded in annotations.items():
        document.setAnnotation(key, "AI Diffusion Plugin", QByteArray(base64.b64decode(encoded)))
    with tempfile.TemporaryDirectory() as temporary:
        model = _make_model(document, Path(temporary))
        sync = ModelSync(model)
        assert model.regions.positive == "native document prompt"
        assert type(model.upscale.factor) is float and model.upscale.factor == 2.0
        assert model.upscale.unblur_strength == 0.0
        assert len(sync._history) == 2
        native = sync._history[1]
        job = model.jobs.find(native.id)
        assert job is not None and len(job.results) == 2
        assert job.params.bounds == Bounds(42, 80, 32, 24)
        assert job.params.seed == 123 and job.params.prompt == "native character"
        assert job.params.strength == 0.43 and job.result_was_used(1)
        assert job.results[0]._qimage.pixelColor(0, 0).getRgb() == (10, 120, 240, 70)
        assert job.params.metadata["baron"]["favorites"]["1"]
        settings.history_storage = 0
        sync._prune()
        assert len(sync._history) == 2
        settings.history_size = 0
        model.jobs.prune(keep=job)
        assert len(sync._history) == 2
        sync._save()
        state = json.loads(bytes(document.annotation("ai_diffusion/ui.json")))
        assert state["baron_history_unlimited"] is True
        assert len(state["history"]) == 2
        assert state["history"][1]["params"]["metadata"]["baron"]["favorites"]["1"]
    print(
        "Native PNG batches loaded and re-saved by actual Python ModelSync; bounds, alpha, seed, applied flags and archive retention verified."
    )
