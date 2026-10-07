"""Validate a curated public font bundle and stage fonts with original licenses."""

import argparse
import hashlib
import json
import shutil
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("bundle", type=Path)
parser.add_argument("destination", type=Path)
args = parser.parse_args()
bundle = args.bundle.resolve()
destination = args.destination.resolve()
manifest = json.loads((bundle / "manifest.json").read_text(encoding="utf-8"))
if not manifest.get("public_bundle") or manifest.get("personal_bundle"):
    raise ValueError("This build accepts only the curated public font bundle")
for font in manifest["files"]:
    for key, digest in [("file", "sha256"), ("license_file", "license_sha256")]:
        path = (bundle / font[key]).resolve()
        if (
            not path.is_relative_to(bundle)
            or hashlib.sha256(path.read_bytes()).hexdigest() != font[digest]
        ):
            raise ValueError("Font/license verification failed: " + font[key])
    if font["license"] not in ("OFL-1.1", "Apache-2.0"):
        raise ValueError("Unapproved license: " + font["family"])
expected = {font["file"] for font in manifest["files"]}
if destination.exists():
    extras = {p.name for p in destination.glob("*.ttf")} - expected
    if extras:
        raise ValueError("Unexpected fonts in destination: " + str(extras))
destination.mkdir(parents=True, exist_ok=True)
for font in manifest["files"]:
    shutil.copyfile(bundle / font["file"], destination / font["file"])
shutil.copytree(bundle / "licenses", destination / "licenses", dirs_exist_ok=True)
shutil.copyfile(bundle / "manifest.json", destination / "manifest.json")
print(f"Public bundle staged: {len(expected)} families with licenses")
