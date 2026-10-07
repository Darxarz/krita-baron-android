"""Verify a private font bundle with the FreeType/Fontconfig stack used by Krita."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("bundle", type=Path)
parser.add_argument("--report", type=Path, required=True)
args = parser.parse_args()
manifest = json.loads((args.bundle / "manifest.json").read_text(encoding="utf-8"))
recognized, unsupported = [], []
families = set()
for row in manifest["files"]:
    path = args.bundle / row["file"]
    if hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
        raise ValueError(f"Damaged font: {path.name}")
    result = subprocess.run(
        ["fc-scan", "--format", "%{family}\\n", str(path)],
        capture_output=True, text=True, check=False,
    )
    if result.returncode == 0 and result.stdout.strip():
        names = set(result.stdout.strip().splitlines())
        families.update(names)
        recognized.append({"file": path.name, "families": sorted(names)})
    else:
        unsupported.append(path.name)
report = {
    "files": len(manifest["files"]), "bytes": manifest["bytes"],
    "recognized_files": len(recognized), "families": sorted(families),
    "unsupported_files": unsupported, "recognized": recognized,
    "verified_with": "Desktop FreeType/Fontconfig, not physical Android runtime",
}
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({key: report[key] for key in ("files", "bytes", "recognized_files", "unsupported_files")}))
