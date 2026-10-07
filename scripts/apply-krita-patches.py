"""Apply reproducible Baron patches without resetting existing Krita source changes."""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).resolve()
for patch in sorted((root / "patches").glob("*.patch")):
    command = ["git", "-C", str(source), "apply"]
    reverse = subprocess.run(command + ["--reverse", "--check", str(patch)], capture_output=True)
    if reverse.returncode == 0:
        print(f"Already applied: {patch.name}")
        continue
    subprocess.run(command + ["--check", str(patch)], check=True)
    subprocess.run(command + [str(patch)], check=True)
    print(f"Applied: {patch.name}")
