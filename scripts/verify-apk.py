import hashlib
import json
import sys
from pathlib import Path
from zipfile import ZipFile

path = Path(sys.argv[1]).resolve()
with ZipFile(path) as archive:
    files = archive.namelist()
    forbidden = [
        name
        for name in files
        if any(
            part in name.lower()
            for part in ("libpython", "pykrita", "cpython", "libpyqt", "libpyside")
        )
    ]
    if forbidden:
        raise SystemExit("Unexpected Python components: " + ", ".join(forbidden))
    module = next(name for name in files if name.endswith("lib_kritabarondocker_arm64-v8a.so"))
    binary = archive.read(module)
    for text in (
        b"Open full-screen model gallery",
        b"Retrieve the existing result",
        b"/api/krita/native/prepare",
    ):
        if text not in binary:
            raise SystemExit("APK does not contain the current native client: " + text.decode())
    if not any(
        b"Lorg/baron/krita/Credentials;" in archive.read(name)
        for name in files
        if name.endswith(".dex")
    ):
        raise SystemExit("Android Keystore helper is missing")
report = {
    "file": path.name,
    "bytes": path.stat().st_size,
    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    "native_module": module,
    "python_interpreter": False,
    "keystore_class": True,
    "current_gallery_and_native_api": True,
    "android_runtime_tested": False,
}
print(json.dumps(report, indent=2))
