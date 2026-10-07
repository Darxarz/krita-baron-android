"""Keep installed Qt dependencies outside androiddeployqt's application QML root."""

import argparse
import shutil
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
args = parser.parse_args()
source = args.source.resolve()
stage = source.parent / "baron-qml-scan-root"
count = 0
for folder in ("plugins", "qmlmodules", "libs"):
    for file in (source / folder).rglob("*"):
        if file.is_file() and (file.suffix in (".qml", ".js") or file.name == "qmldir"):
            destination = stage / file.relative_to(source)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, destination)
            count += 1
if count < 20:
    raise SystemExit("Krita QML sources were not found; refusing incomplete packaging")
template = source / "_build/krita-deployment.json.in2"
text = template.read_text()
old = f'"qml-root-path": "{source.as_posix()}"'
new = f'"qml-root-path": "{stage.as_posix()}"'
if old not in text and new not in text:
    raise SystemExit("Unexpected deployment template; QML root was not changed")
text = text.replace(old, new)
text = text.replace(
    f'"qml-import-paths": "{source.as_posix()}/_build/lib"',
    f'"qml-import-paths": "{source.as_posix()}/_build/lib,{source.as_posix()}/_install/qml"',
)
template.write_text(text)
fonts = source / "_install/etc/fonts/fonts.conf"
config = fonts.read_text()
font_dir = '<dir prefix="relative">../../baron-fonts</dir>'
if font_dir not in config:
    config = config.replace("<fontconfig>", f"<fontconfig>\n    {font_dir}", 1)
    fonts.write_text(config)
gradle = source / "packaging/android/apk/build.gradle"
text = gradle.read_text()
if "copyPersonalFonts" not in text:
    text = text.replace(
        "tasks.register('copyLocaleFiles', Copy) {",
        """tasks.register('copyPersonalFonts', Copy) {
    dependsOn('configure')
    def bundle = System.getenv('BARON_FONT_BUNDLE_DIR') ?: System.getenv('BARON_PERSONAL_FONT_DIR')
    if (bundle) {
        from bundle
        into 'assets/baron-fonts/'
        include '**'
    }
}

tasks.register('copyLocaleFiles', Copy) {""",
        1,
    )
    text = text.replace(
        "'copyFonts', 'copyAssets', 'copyLocaleFiles'",
        "'copyFonts', 'copyAssets', 'copyPersonalFonts', 'copyLocaleFiles'",
        1,
    )
    gradle.write_text(text)
elif "System.getenv('BARON_FONT_BUNDLE_DIR')" not in text:
    text = text.replace(
        "System.getenv('BARON_PERSONAL_FONT_DIR')",
        "System.getenv('BARON_FONT_BUNDLE_DIR') ?: System.getenv('BARON_PERSONAL_FONT_DIR')",
    )
    gradle.write_text(text)
print(f"QML scanner root staged: {count} files; installed Qt modules will be packaged.")
