#!/usr/bin/env bash
set -euo pipefail
baron_root="${BARON_BUILD_ROOT:-/opt/baron-android}"
if [[ -f "$baron_root/android/android-ndk-r27d/source.properties" ]]; then exit 0; fi
mkdir -p "$baron_root/android"
aria2c --continue=true --max-connection-per-server=12 --split=12 --min-split-size=4M --auto-file-renaming=false --allow-overwrite=true --summary-interval=30 --console-log-level=warn --download-result=hide --dir="$baron_root/android" --out=ndk.zip https://dl.google.com/android/repository/android-ndk-r27d-linux.zip
unzip -q "$baron_root/android/ndk.zip" -d "$baron_root/android"
