#!/usr/bin/env bash
set -euo pipefail
baron_repo="$(cd "$(dirname "$0")/.." && pwd)"
baron_root="${BARON_BUILD_ROOT:-/opt/baron-android}"
mkdir -p "$baron_root"
if [[ ! -d "$baron_root/krita/.git" ]]; then
    git init "$baron_root/krita"
    git -C "$baron_root/krita" fetch --depth 1 https://github.com/KDE/krita.git 96434e2e71aae3a509506518bf83c312b634ac66
    git -C "$baron_root/krita" checkout --detach FETCH_HEAD
fi
if [[ ! -d "$baron_root/krita/krita-deps-management/.git" ]]; then
    git init "$baron_root/krita/krita-deps-management"
    git -C "$baron_root/krita/krita-deps-management" fetch --depth 1 https://invent.kde.org/packaging/krita-deps-management.git 7eab9569cb65fe8422224fc1de644a169fd8b0f4
    git -C "$baron_root/krita/krita-deps-management" checkout --detach FETCH_HEAD
    git init "$baron_root/krita/krita-deps-management/ci-utilities"
    git -C "$baron_root/krita/krita-deps-management/ci-utilities" fetch --depth 1 https://invent.kde.org/packaging/krita-ci-utilities.git ed9f250c4de8382b8f8352739c88be75342f0bf8
    git -C "$baron_root/krita/krita-deps-management/ci-utilities" checkout --detach FETCH_HEAD
fi
python3 -m venv "$baron_root/venv"
"$baron_root/venv/bin/pip" install -q -r "$baron_root/krita/krita-deps-management/requirements.txt"
cd "$baron_root/krita"
if [[ ! -d plugins/dockers/baron ]]; then
    python3 "$baron_repo/scripts/install-overlay.py" "$baron_root/krita"
fi
"$baron_root/venv/bin/python" krita-deps-management/tools/setup-env.py --root "$baron_root" --android-abi arm64-v8a --venv "$baron_root/venv" -d
cp "$baron_root/.kde-ci.yml" .kde-ci.yml
set +u
source "$baron_root/base-env"
unset KDECI_GLOBAL_CONFIG_OVERRIDE_PATH
python krita-deps-management/ci-utilities/run-ci-build.py --project krita --branch master --platform Android/arm64-v8a/Qt5/Shared --only-env
set -u
