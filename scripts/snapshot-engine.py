"""Copy a reviewed Baron engine snapshot; never modify the source plugin."""

import hashlib
import json
import shutil
import sys
from pathlib import Path

source = Path(sys.argv[1]).resolve()
target = Path(__file__).resolve().parents[1] / "server" / "vendor"
if target.exists():
    raise SystemExit("Snapshot exists. Review changes before replacing it.")
manifest = {}
for folder in ("ai_diffusion", "tests/mock"):
    for path in (source / folder).rglob("*"):
        if not path.is_file() or "__pycache__" in path.parts or ".git" in path.parts:
            continue
        rel = path.relative_to(source)
        if path.suffix not in (".py", ".json") and "websockets" not in path.parts:
            continue
        destination = target / rel
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
        manifest[rel.as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
shutil.copyfile(source / "LICENSE", target / "LICENSE")
(target / "SNAPSHOT.json").write_text(
    json.dumps({"edition": "1.53.0-baron.3", "files": manifest}, indent=2),
    encoding="utf-8",
)
print(f"Copied {len(manifest)} files, with SHA-256 manifest")
