#!/usr/bin/env bash
set -eo pipefail
baron_root="${BARON_BUILD_ROOT:-/opt/baron-stable-5.3.4}"
baron_repo="$(cd "$(dirname "$0")/.." && pwd)"
cd "$baron_root/krita"
if [[ "$(git rev-parse HEAD)" != e7e52a72ed37ecaf9ffaa2fab836b7c2f5539d1f ]]; then
    echo "Refusing to build a different Krita base: expected stable 5.3.4 release sources." >&2
    exit 1
fi
source ./env
unset KDECI_GLOBAL_CONFIG_OVERRIDE_PATH
export ANDROID_HOME="$baron_root/android/sdk"
export KDECI_ANDROID_SDK_ROOT="$ANDROID_HOME"
export KDECI_ANDROID_NDK_ROOT="$baron_root/android/android-ndk-r27d"
export KDECI_ANDROID_ABI=arm64-v8a
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH="$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$PATH"
bash "$baron_repo/scripts/install-qtwebsockets.sh"
python3 "$baron_repo/scripts/refresh-overlay.py" "$baron_root/krita"
cmake -S . -B _build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$baron_root/krita/_install" \
    -DCMAKE_TOOLCHAIN_FILE="$baron_root/krita/krita-deps-management/tools/android-toolchain-krita.cmake" \
    -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DBUILD_WITH_QT6=OFF -DBUILD_TESTING=OFF -DFOUNDATION_BUILD=ON \
    -DKRITA_ENABLE_PCH=OFF -DANDROID_ENABLE_STDIO_FORWARDING=ON -DCMAKE_DISABLE_FIND_PACKAGE_PythonLibrary=ON
cmake --build _build --target kritabarondocker --parallel "${BARON_BUILD_JOBS:-4}"
cmake --build _build --parallel "${BARON_BUILD_JOBS:-4}"
cmake --install _build
export KDECI_WORKDIR_PATH="$baron_root"
export KDECI_SHARED_INSTALL_PATH="$baron_root/krita/_install"
export KRITA_INSTALL_PREFIX="$baron_root/krita/_install"
export ANDROID_ABI=arm64-v8a
python3 "$baron_repo/scripts/prepare-android-qml.py" "$baron_root/krita"
python3 build-tools/ci-scripts/build-android-package.py
