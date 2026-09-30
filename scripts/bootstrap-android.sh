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
    git clone --depth 1 https://invent.kde.org/packaging/krita-deps-management.git "$baron_root/krita/krita-deps-management"
    git clone --depth 1 https://invent.kde.org/packaging/krita-ci-utilities.git "$baron_root/krita/krita-deps-management/ci-utilities"
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
