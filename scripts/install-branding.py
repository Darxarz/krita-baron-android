"""Install prepared Baron goat launcher artwork without extra build dependencies."""

import shutil
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).resolve()
resources = source / "packaging/android/apk/res"
names = ("ic_launcher", "ic_launcher_round", "ic_launcher_next", "ic_launcher_next_round")
for density in ("mdpi", "hdpi", "xhdpi", "xxhdpi", "xxxhdpi"):
    folder = resources / f"mipmap-{density}"
    folder.mkdir(parents=True, exist_ok=True)
    for name in names:
        (folder / f"{name}.webp").unlink(missing_ok=True)
        shutil.copyfile(root / f"android/branding/mipmap-{density}.png", folder / f"{name}.png")

drawable = resources / "drawable"
drawable.mkdir(parents=True, exist_ok=True)
for name in ("ic_launcher_foreground", "ic_launcher_next_foreground"):
    (drawable / f"{name}.xml").unlink(missing_ok=True)
    shutil.copyfile(root / "android/branding/adaptive-foreground.png", drawable / f"{name}.png")
(drawable / "ic_launcher_background.xml").write_text(
    '<?xml version="1.0" encoding="utf-8"?>\n'
    '<shape xmlns:android="http://schemas.android.com/apk/res/android" '
    'android:shape="rectangle"><solid android:color="#211529"/></shape>\n',
    encoding="utf-8",
)
for name in ("ic_launcher-playstore.png", "ic_launcher_next-playstore.png"):
    shutil.copyfile(
        root / "android/branding/playstore.png", source / "packaging/android/apk" / name
    )
print("Installed Baron goat icon at five launcher densities and adaptive safe bounds")
