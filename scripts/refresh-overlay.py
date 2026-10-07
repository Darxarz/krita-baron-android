"""Refresh only the Baron overlay in a dedicated Krita build checkout."""

import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).resolve()
subprocess.run(
    [sys.executable, str(root / "scripts/apply-krita-patches.py"), str(source)], check=True
)
subprocess.run([sys.executable, str(root / "scripts/install-branding.py"), str(source)], check=True)
target = source / "plugins/dockers/baron"
if not (target / "krita_barondocker.json").is_file():
    raise SystemExit("Install the overlay first; no other source files will be modified")
for folder in ("krita", "native"):
    for file in (root / folder).rglob("*"):
        if not file.is_file():
            continue
        destination = target / (
            file.relative_to(root / folder)
            if folder == "krita"
            else Path("native") / file.relative_to(root / folder)
        )
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.is_file() or destination.read_bytes() != file.read_bytes():
            shutil.copyfile(file, destination)
shutil.copytree(
    root / "android/org/baron",
    source / "packaging/android/apk/src/org/baron",
    dirs_exist_ok=True,
)
gradle = source / "packaging/android/apk/build.gradle"
text = gradle.read_text(encoding="utf-8")
if "applicationId 'org.krita.baron'" not in text:
    gradle.write_text(
        text.replace(
            "    defaultConfig {",
            "    defaultConfig {\n        applicationId 'org.krita.baron'",
        ),
        encoding="utf-8",
    )
proguard = source / "packaging/android/apk/proguard-rules.pro"
text = proguard.read_text(encoding="utf-8")
if "org.baron.krita.Credentials" not in text:
    proguard.write_text(
        text + "\n-keep class org.baron.krita.Credentials { public static *; }\n",
        encoding="utf-8",
    )
for manifest in (source / "packaging/android/apk").rglob("AndroidManifest.xml"):
    text = manifest.read_text(encoding="utf-8")
    for label in (
        'android:label="Krita Debug"',
        'android:label="Krita Next"',
        'android:label="Krita"',
    ):
        text = text.replace(label, 'android:label="Krita Baron Edition"')
    manifest.write_text(text, encoding="utf-8")
version = json.loads((root / "android/version.json").read_text(encoding="utf-8"))
shutil.copyfile(root / "android/version.json", target / "version.json")
text = gradle.read_text(encoding="utf-8")
text = re.sub(r"(?m)^\s*versionCode .*", f"        versionCode {version['version_code']}", text)
text = re.sub(
    r"(?m)^\s*versionName .*",
    f'        versionName "{version["krita_version"]}-baron.{version["version"]}"',
    text,
)
gradle.write_text(text, encoding="utf-8")
manifest = source / "packaging/android/apk/AndroidManifest.xml"
text = manifest.read_text(encoding="utf-8")
if "android.permission.REQUEST_INSTALL_PACKAGES" not in text:
    text = text.replace(
        "    <application",
        '    <uses-permission android:name="android.permission.REQUEST_INSTALL_PACKAGES" />\n    <application',
        1,
    )
if "org.baron.krita.UpdateProvider" not in text:
    text = text.replace(
        "        <activity",
        '        <provider android:name="org.baron.krita.UpdateProvider" android:authorities="${applicationId}.baronupdates" android:exported="false" android:grantUriPermissions="true" />\n        <activity',
        1,
    )
manifest.write_text(text, encoding="utf-8")
text = proguard.read_text(encoding="utf-8")
if "org.baron.krita.Updates" not in text:
    proguard.write_text(
        text
        + "\n-keep class org.baron.krita.Updates { public static *; }\n-keep class org.baron.krita.UpdateProvider { *; }\n",
        encoding="utf-8",
    )
print("Refreshed native Baron files and its separate Android package identity")
for helper in ("CrashDiagnostics", "TombstoneReport", "AnrReport"):
    text = proguard.read_text(encoding="utf-8")
    if "org.baron.krita." + helper not in text:
        proguard.write_text(
            text + f"\n-keep class org.baron.krita.{helper} {{ public static *; }}\n",
            encoding="utf-8",
        )
text = proguard.read_text(encoding="utf-8")
if "org.baron.krita.PromptClipboard" not in text:
    proguard.write_text(
        text + "\n-keep class org.baron.krita.PromptClipboard { public static *; }\n",
        encoding="utf-8",
    )
