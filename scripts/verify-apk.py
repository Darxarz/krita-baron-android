import hashlib
import json
import sys
import tempfile
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
    release = json.loads(
        (Path(__file__).resolve().parents[1] / "android/version.json").read_text(encoding="utf-8")
    )
    if release["version"].encode("utf-16le") not in binary:
        raise SystemExit("Native UI version does not match the release metadata")
    if b"/upscale/factor" not in binary and "/upscale/factor".encode("utf-16le") not in binary:
        raise SystemExit("Missing Python-compatible decimal history serialization")
    version_module = next(name for name in files if name.endswith("libkritaversion_arm64-v8a.so"))
    if release["krita_version"].encode("utf-16le") not in archive.read(version_module):
        raise SystemExit("APK Krita base does not match the stable release metadata")
    for text in (
        b"Open full-screen model gallery",
        b"Retrieve the existing result",
        b"/api/krita/native/prepare",
        b"/baron-updates/stable.json",
        b"prompt_banks",
        b"style_banks",
        b"linked_edit_style",
        b"styleSamplerPreset",
        b"/baron/model-thumbnails",
        b"/api/krita/model-metadata?name=",
        b"modelInformationDialog",
        b"modelTags",
        b"promptSyntax",
        b"a1111_gpu_noise",
        b"Click to toggle preview, double-click to apply.",
        b"history_click_behavior",
        b"controlRange",
        b"Add Control Layer",
        b"Jobs",
        b"Original author: Acly and contributors",
        b"https://www.interstice.cloud",
        b"/plugin/resources",
        b"/baron/native/prepare",
        b"connectInterstice",
        b"connectComfyUI",
        b"connectOrchestrion",
        b"Copy Prompt (Evaluated)",
        b"styleSelect",
        b"promptCompletionList",
        b"stylePresets",
        b"rootRegionSummary",
        b"baron-native-state",
        b"settingsCategories",
        b"Could not save generation history: %1",
        b"modelFamilyFilter",
        b"Tag Auto-Completion",
        b"interfaceSettingsScroll",
        b"generation_finished_action",
        b"apply_region_behavior",
        b"<lora:%1:%2>",
        b"Queued images exceed the memory limit.",
        b"Cancelled locally; server cancellation could not be confirmed:",
        b"The job is no longer in the server queue or history.",
        b"promptActionBar",
        b"promptDisabledFragments",
        b"/api/translate",
        b"/api/prompt/organize/labels",
        b"orientationLayouts",
        b"orientation_layouts_enabled",
        b"forgetOrientationLayouts",
        b"inpaintSeamless",
        b"inpaintFocus",
        b"inpaintContext",
        b"customInpaint",
        b"inpaint_options",
        b"selection_bounds",
    ):
        if text not in binary:
            raise SystemExit("APK does not contain the current native client: " + text.decode())
    if not any(
        b"Lorg/baron/krita/Credentials;" in archive.read(name)
        for name in files
        if name.endswith(".dex")
    ):
        raise SystemExit("Android Keystore helper is missing")
    for helper in (
        b"Lorg/baron/krita/Updates;",
        b"Lorg/baron/krita/UpdateProvider;",
        b"Lorg/baron/krita/PromptClipboard;",
        b"Lorg/baron/krita/CrashDiagnostics;",
        b"Lorg/baron/krita/TombstoneReport;",
        b"Lorg/baron/krita/AnrReport;",
    ):
        if not any(helper in archive.read(name) for name in files if name.endswith(".dex")):
            raise SystemExit("Android update helper is missing")
    for dependency in (
        "libQt5WebSockets_arm64-v8a.so",
        "libQt5Concurrent_arm64-v8a.so",
        "libQt5Qml_arm64-v8a.so",
        "libplugins_imageformats_qwebp_arm64-v8a.so",
        "libqml_QtQuick_Controls.2_qtquickcontrols2plugin_arm64-v8a.so",
        "libqml_QtQuick_Layouts_qquicklayoutsplugin_arm64-v8a.so",
        "libkritaqmlcomponents_arm64-v8a.so",
    ):
        if not any(name.endswith("/" + dependency) for name in files):
            raise SystemExit("Required Android dependency is missing: " + dependency)
    from PyQt5.QtCore import QFile, QResource
    from PyQt5.QtGui import QImage

    launcher = next(
        name
        for name in files
        if name.startswith("res/mipmap-xxxhdpi") and name.endswith("/ic_launcher.png")
    )
    expected_icon = QImage(
        str(Path(__file__).resolve().parents[1] / "android/branding/mipmap-xxxhdpi.png")
    )
    packaged_icon = QImage.fromData(archive.read(launcher))
    if expected_icon.isNull() or expected_icon != packaged_icon:
        raise SystemExit("APK launcher icon does not match the Baron goat artwork")
    if not any("ic_launcher_foreground" in name and name.endswith(".png") for name in files):
        raise SystemExit("Adaptive goat launcher foreground is missing")

    with tempfile.TemporaryDirectory() as temporary:
        resource = Path(temporary) / "qml.rcc"
        resource.write_bytes(archive.read("assets/android_rcc_bundle.rcc"))
        if not QResource.registerResource(str(resource)):
            raise SystemExit("Invalid QML resource bundle")
        required = ["QtQuick/Controls.2", "QtQuick/Layouts", "org/krita/components"]
        for folder in required:
            if not QFile.exists(":/android_rcc_bundle/qml/" + folder + "/qmldir"):
                raise SystemExit("Text Properties QML module is missing: " + folder)
        QResource.unregisterResource(str(resource))
    personal_fonts = None
    public_fonts = None
    if "assets/baron-fonts/manifest.json" in files:
        font_manifest = json.loads(archive.read("assets/baron-fonts/manifest.json"))
        for font in font_manifest["files"]:
            if (
                hashlib.sha256(archive.read("assets/baron-fonts/" + font["file"])).hexdigest()
                != font["sha256"]
            ):
                raise SystemExit("Font integrity check failed: " + font["file"])
            if font_manifest.get("public_bundle"):
                if (
                    hashlib.sha256(
                        archive.read("assets/baron-fonts/" + font["license_file"])
                    ).hexdigest()
                    != font["license_sha256"]
                ):
                    raise SystemExit("Font license integrity check failed: " + font["file"])
        if font_manifest.get("public_bundle") and not font_manifest.get("personal_bundle"):
            public_fonts = len(font_manifest["files"])
        else:
            personal_fonts = len(font_manifest["files"])
report = {
    "file": path.name,
    "bytes": path.stat().st_size,
    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    "native_module": module,
    "python_interpreter": False,
    "keystore_class": True,
    "current_gallery_and_native_api": True,
    "android_runtime_tested": False,
    "text_properties_qml_packaged": True,
    "websocket_progress_packaged": True,
    "embedded_tag_completion_packaged": True,
    "prompt_fragment_actions_packaged": True,
    "background_result_processing_packaged": True,
    "orientation_layouts_packaged": True,
    "android_plain_text_clipboard_helper_packaged": True,
    "native_ui_version": release["version"],
    "krita_base_version": release["krita_version"],
    "krita_base_revision": release["krita_revision"],
    "baron_goat_launcher_verified": True,
    "android_exit_history_report_packaged": True,
    "android_anr_thread_report_packaged": True,
    "personal_fonts": personal_fonts,
    "public_fonts": public_fonts,
}
print(json.dumps(report, indent=2))
