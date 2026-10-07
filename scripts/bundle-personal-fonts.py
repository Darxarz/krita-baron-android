"""Copy this user's Windows fonts to a private Android packaging directory."""

import argparse
import hashlib
import json
import os
import shutil
import winreg
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--destination", type=Path, required=True)
args = parser.parse_args()
destination = args.destination.resolve()
destination.mkdir(parents=True, exist_ok=True)
system = Path(os.environ["WINDIR"]) / "Fonts"
personal = Path(os.environ["LOCALAPPDATA"]) / "Microsoft/Windows/Fonts"
files = set(system.iterdir()) | (set(personal.iterdir()) if personal.exists() else set())
for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
    try:
        with winreg.OpenKey(hive, r"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts") as key:
            for index in range(winreg.QueryInfoKey(key)[1]):
                value = winreg.EnumValue(key, index)[1]
                if isinstance(value, str):
                    path = Path(os.path.expandvars(value))
                    files.add(path if path.is_absolute() else system / path)
    except FileNotFoundError:
        pass
records, seen, skipped = [], set(), []
for path in sorted(files):
    if not path.is_file() or path.suffix.lower() not in (".ttf", ".ttc", ".otf", ".fon"):
        if path.is_file() and path.suffix.lower() not in (".ini", ".compositefont"):
            skipped.append(str(path))
        continue
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest in seen:
        continue
    seen.add(digest)
    target = destination / (digest[:12] + "-" + path.name)
    shutil.copy2(path, target)
    if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        raise ValueError(f"Font copy verification failed: {path}")
    records.append({"source": str(path), "file": target.name, "bytes": target.stat().st_size,
                    "sha256": digest, "kind": path.suffix.lower()})
manifest = {"personal_bundle": True, "files": records, "unsupported_cad_files": skipped,
            "bytes": sum(row["bytes"] for row in records)}
(destination / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({"fonts": len(records), "bytes": manifest["bytes"], "unsupported_cad_files": len(skipped)}))
