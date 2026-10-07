"""Install the temporary Baron monogram in Android launcher resources."""

import shutil
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
resources = Path(sys.argv[1]) / "packaging/android/apk/res"
foreground = """<?xml version="1.0" encoding="utf-8"?>
<vector xmlns:android="http://schemas.android.com/apk/res/android" android:width="108dp" android:height="108dp" android:viewportWidth="108" android:viewportHeight="108">
<path android:fillColor="#ffffff" android:fillType="evenOdd" android:pathData="M36,28 L55,28 C77,28 80,48 66,53 C83,59 79,81 56,81 L36,81 Z M45,37 L45,49 L55,49 C67,49 67,37 55,37 Z M45,58 L45,72 L56,72 C70,72 70,58 56,58 Z"/>
</vector>
"""
background = """<?xml version="1.0" encoding="utf-8"?>
<shape xmlns:android="http://schemas.android.com/apk/res/android" android:shape="rectangle"><solid android:color="#4c487b"/></shape>
"""
(resources / "drawable").mkdir(exist_ok=True)
for variant in ("ic_launcher_foreground", "ic_launcher_next_foreground"):
    (resources / "drawable" / f"{variant}.xml").write_text(foreground)
(resources / "drawable/ic_launcher_background.xml").write_text(background)
for folder in resources.glob("mipmap-*"):
    for existing in folder.glob("*.webp"):
        if existing.stem not in (
            "ic_launcher",
            "ic_launcher_round",
            "ic_launcher_next",
            "ic_launcher_next_round",
        ):
            continue
        replacement = folder / (existing.stem + ".png")
        shutil.copyfile(root / "android/branding" / f"{folder.name}.png", replacement)
        existing.unlink()
    for existing in folder.glob("ic_launcher*.png"):
        source = root / "android/branding" / f"{folder.name}.png"
        if source.is_file():
            shutil.copyfile(source, existing)
print("Installed temporary Baron icon (replace android/branding for final artwork)")
