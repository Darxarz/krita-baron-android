"""Archive the modified source and curated public fonts, excluding build/private files."""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("krita", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]


def source_files(repo):
    git = "git"
    git_path = str(repo)
    marker = repo / ".git"
    if sys.platform != "win32" and marker.is_file():
        if re.match(r"gitdir: [A-Za-z]:[\\/]", marker.read_text()):
            git = shutil.which("git.exe") or "/mnt/c/Program Files/Git/cmd/git.exe"
            git_path = subprocess.check_output(["wslpath", "-w", str(repo)], text=True).strip()
    names = (
        subprocess.check_output(
            [git, "-C", git_path, "ls-files", "--cached", "--others", "--exclude-standard", "-z"]
        )
        .decode()
        .split("\0")
    )
    for name in sorted(set(names)):
        path = repo / name
        if name.startswith("artifacts/") or path.resolve() == args.output.resolve():
            continue
        if not name or not path.is_file() or path.is_symlink():
            continue
        if path.suffix in (".apk", ".keystore", ".pyc") or path.name in (".env", "env", "base-env"):
            raise ValueError("Unexpected private/build file: " + name)
        yield path, name


args.output.parent.mkdir(parents=True, exist_ok=True)
with ZipFile(args.output, "w", ZIP_DEFLATED, compresslevel=3) as archive:
    for repo, prefix in [(args.krita, "krita"), (root, "baron")]:
        for path, name in source_files(repo):
            archive.write(path, prefix + "/" + name)
    fonts = root / "build/curated-fonts"
    for path in sorted(fonts.rglob("*")):
        if path.is_file():
            archive.write(path, "baron/build/curated-fonts/" + path.relative_to(fonts).as_posix())
print(f"Release source archive: {args.output.stat().st_size} bytes")
