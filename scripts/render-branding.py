"""Package the Baron goat artwork as legacy and adaptive Android launcher icons."""

import os
import sys
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QGuiApplication, QImage, QPainter

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).resolve()
resources = source / "packaging/android/apk/res"
artwork = root / "assets/branding/baron-goat-v1.png"
application = QGuiApplication(sys.argv[:1])
image = QImage(str(artwork))
if image.isNull() or not image.hasAlphaChannel():
    raise SystemExit("Missing transparent Baron goat artwork")

sizes = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
names = ("ic_launcher", "ic_launcher_round", "ic_launcher_next", "ic_launcher_next_round")
for density, size in sizes.items():
    folder = resources / f"mipmap-{density}"
    folder.mkdir(parents=True, exist_ok=True)
    scaled = image.scaled(
        size, size, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation
    )
    for name in names:
        (folder / f"{name}.webp").unlink(missing_ok=True)
        if not scaled.save(str(folder / f"{name}.png")):
            raise SystemExit("Could not write launcher icon")

drawable = resources / "drawable"
drawable.mkdir(parents=True, exist_ok=True)
foreground = QImage(432, 432, QImage.Format_ARGB32_Premultiplied)
foreground.fill(Qt.GlobalColor.transparent)
safe = image.scaled(
    264, 264, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation
)
painter = QPainter(foreground)
painter.drawImage((432 - safe.width()) // 2, (432 - safe.height()) // 2, safe)
painter.end()
for name in ("ic_launcher_foreground", "ic_launcher_next_foreground"):
    (drawable / f"{name}.xml").unlink(missing_ok=True)
    if not foreground.save(str(drawable / f"{name}.png")):
        raise SystemExit("Could not write adaptive launcher foreground")
(drawable / "ic_launcher_background.xml").write_text(
    '<?xml version="1.0" encoding="utf-8"?>\n'
    '<shape xmlns:android="http://schemas.android.com/apk/res/android" '
    'android:shape="rectangle"><solid android:color="#211529"/></shape>\n',
    encoding="utf-8",
)
for name in ("ic_launcher-playstore.png", "ic_launcher_next-playstore.png"):
    image.scaled(
        512, 512, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation
    ).save(str(source / "packaging/android/apk" / name))
print("Installed Baron goat icon at five launcher densities and adaptive safe bounds")
