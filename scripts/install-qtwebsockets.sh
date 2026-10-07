#!/usr/bin/env bash
set -eo pipefail
baron_root="${BARON_BUILD_ROOT:-/opt/baron-stable-5.3.4}"
baron_prefix="$baron_root/krita/_install"
if [[ -f "$baron_prefix/lib/cmake/Qt5WebSockets/Qt5WebSocketsConfig.cmake" ]]; then
    exit 0
fi
baron_dependency="$baron_root/extra/qtwebsockets"
if [[ ! -d "$baron_dependency/.git" ]]; then
    git clone --depth 1 --branch v5.15.7-lts-lgpl https://github.com/qt/qtwebsockets.git "$baron_dependency"
fi
[[ "$(git -C "$baron_dependency" rev-parse HEAD)" == 4fe33a26f24770069b264771e89361eeecc646f3 ]] || {
    echo "Unexpected QtWebSockets revision" >&2
    exit 1
}
export ANDROID_NDK_ROOT="$baron_root/android/android-ndk-r27d"
export ANDROID_ABIS=arm64-v8a
cd "$baron_dependency"
"$baron_prefix/bin/qmake" qtwebsockets.pro
make -j"${BARON_BUILD_JOBS:-4}"
make install
