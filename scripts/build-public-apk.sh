#!/usr/bin/env bash
set -eo pipefail
baron_root="${BARON_BUILD_ROOT:-/opt/baron-stable-5.3.4}"
baron_repo="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$baron_repo/scripts/stage-public-fonts.py" "$baron_repo/build/curated-fonts" "$baron_root/public-fonts"
export BARON_FONT_BUNDLE_DIR="$baron_root/public-fonts"
bash "$baron_repo/scripts/build-android.sh"
