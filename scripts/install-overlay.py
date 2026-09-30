"""Install the native docker into an isolated, clean Krita source checkout."""

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).resolve()
if not (source / "plugins/dockers/CMakeLists.txt").is_file():
    raise SystemExit("Not a full Krita source checkout")
if subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True).strip():
    raise SystemExit(
        "Use a clean, isolated Krita checkout; existing changes will not be overwritten"
    )
target = source / "plugins/dockers/baron"
shutil.copytree(ROOT / "krita", target)
shutil.copytree(ROOT / "native", target / "native")
shutil.copytree(ROOT / "android/org/baron", source / "packaging/android/apk/src/org/baron")
gradle = source / "packaging/android/apk/build.gradle"
gradle.write_text(
    gradle.read_text(encoding="utf-8").replace(
        "    defaultConfig {",
        "    defaultConfig {\n        applicationId 'org.krita.baron'",
    ),
    encoding="utf-8",
)
for relative in (
    "AndroidManifest.xml",
    "flavors/debug/AndroidManifest.xml",
    "flavors/next/AndroidManifest.xml",
):
    manifest = source / "packaging/android/apk" / relative
    if manifest.exists():
        text = manifest.read_text(encoding="utf-8")
        for label in (
            'android:label="Krita Debug"',
            'android:label="Krita Next"',
            'android:label="Krita"',
        ):
            text = text.replace(label, 'android:label="Krita Baron Edition"')
        manifest.write_text(text, encoding="utf-8")
proguard = source / "packaging/android/apk/proguard-rules.pro"
with proguard.open("a", encoding="utf-8") as file:
    file.write("\n-keep class org.baron.krita.Credentials { public static *; }\n")
cmake = source / "plugins/dockers/CMakeLists.txt"
with cmake.open("a", encoding="utf-8") as file:
    file.write("\nadd_subdirectory(baron)\n")
print("Native Baron docker installed in isolated Krita source tree")
