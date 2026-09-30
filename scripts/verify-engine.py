import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1] / "server/vendor"
manifest = json.loads((root / "SNAPSHOT.json").read_text(encoding="utf-8"))
invalid = [
    name
    for name, expected in manifest["files"].items()
    if not (root / name).is_file()
    or hashlib.sha256((root / name).read_bytes()).hexdigest() != expected
]
if invalid:
    raise SystemExit("Engine snapshot changed: " + ", ".join(invalid))
print(f"Verified {len(manifest['files'])} engine files ({manifest['edition']})")
