#!/usr/bin/env bash
set -eo pipefail
baron_root="${BARON_BUILD_ROOT:-/opt/baron-stable-5.3.4}"
baron_repo="$(cd "$(dirname "$0")/.." && pwd)"
export BARON_PERSONAL_FONT_DIR="${BARON_PERSONAL_FONT_DIR:-$baron_root/personal-fonts}"
[[ -f "$BARON_PERSONAL_FONT_DIR/manifest.json" ]] || {
    echo "First create and verify the private font bundle" >&2
    exit 1
}
cd "$baron_root/krita"
source ./env
unset KDECI_GLOBAL_CONFIG_OVERRIDE_PATH
export ANDROID_HOME="$baron_root/android/sdk"
export KDECI_ANDROID_SDK_ROOT="$ANDROID_HOME"
export KDECI_ANDROID_NDK_ROOT="$baron_root/android/android-ndk-r27d"
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH="$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$PATH"
export KDECI_WORKDIR_PATH="$baron_root"
export KDECI_SHARED_INSTALL_PATH="$baron_root/krita/_install"
export KRITA_INSTALL_PREFIX="$baron_root/krita/_install"
export ANDROID_ABI=arm64-v8a
python3 "$baron_repo/scripts/prepare-android-qml.py" "$baron_root/krita"
python3 build-tools/ci-scripts/build-android-package.py
