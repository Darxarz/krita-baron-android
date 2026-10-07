"""Publish verified Baron packages; update the manifest after every file is in place."""

import argparse
import hashlib
import json
import os
import re
import shutil
import tempfile
from pathlib import Path
from zipfile import ZipFile


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_verified(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        if sha(source) != sha(target):
            raise ValueError(f"Published releases are immutable: {target}")
        return
    with tempfile.NamedTemporaryFile(dir=target.parent, suffix=".pending", delete=False) as file:
        pending = Path(file.name)
    try:
        shutil.copyfile(source, pending)
        if sha(pending) != sha(source):
            raise ValueError("Package copy failed verification")
        os.replace(pending, target)
    finally:
        pending.unlink(missing_ok=True)


def publish(windows, android, version, destinations):
    with ZipFile(windows) as archive:
        source = archive.read("ai_diffusion/__init__.py").decode("utf-8")
        match = re.search(r'__version__\s*=\s*"([^"]+)"', source)
        if match is None:
            raise ValueError("Plugin version is missing")
        plugin_version = match[1]
        if not re.fullmatch(r"\d+\.\d+\.\d+-baron\.\d+", plugin_version):
            raise ValueError("Not a Baron plugin")
    if not re.fullmatch(r"\d+\.\d+\.\d+", version["version"]):
        raise ValueError("Invalid Android version")
    if version["package"] != "org.krita.baron.debug":
        raise ValueError("Wrong Android package")
    names = {
        "windows": f"krita-ai-diffusion-{plugin_version}.zip",
        "android": f"krita-baron-android-arm64-v{version['version']}.apk",
    }
    manifest = {"schema": 1, "edition": "baron", "channel": "stable"}
    for platform, package in (("windows", windows), ("android", android)):
        size = package.stat().st_size
        if not 0 < size <= (32 if platform == "windows" else 512) * 1024 * 1024:
            raise ValueError("Package size exceeds update limit")
        manifest[platform] = {
            "version": plugin_version if platform == "windows" else version["version"],
            "url": "https://orchestrion.su/baron-updates/releases/" + names[platform],
            "bytes": size,
            "sha256": sha(package),
        }
    manifest["android"].update({key: version[key] for key in ("package", "version_code")})
    for destination in destinations:
        destination = destination.resolve()
        if destination.name != "baron-updates":
            raise ValueError("Destination must be a dedicated baron-updates directory")
        current = destination / "stable.json"
        if current.exists():
            previous = json.loads(current.read_text(encoding="utf-8"))
            if previous["android"]["version_code"] > version["version_code"]:
                raise ValueError("Refusing to downgrade Android")

            def key(v):
                return tuple(map(int, re.findall(r"\d+", v)))

            if key(previous["windows"]["version"]) > key(plugin_version):
                raise ValueError("Refusing to downgrade the plugin")
        copy_verified(windows, destination / "releases" / names["windows"])
        copy_verified(android, destination / "releases" / names["android"])
    for destination in destinations:
        destination = destination.resolve()
        pending = destination / "stable.json.pending"
        pending.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        os.replace(pending, destination / "stable.json")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windows", type=Path, required=True)
    parser.add_argument("--android", type=Path, required=True)
    parser.add_argument(
        "--version", type=Path, default=Path(__file__).resolve().parents[1] / "android/version.json"
    )
    parser.add_argument("--destination", type=Path, action="append", required=True)
    args = parser.parse_args()
    publish(
        args.windows,
        args.android,
        json.loads(args.version.read_text(encoding="utf-8")),
        args.destination,
    )
