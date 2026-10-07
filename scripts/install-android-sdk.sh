#!/usr/bin/env bash
set -euo pipefail
baron_root="${BARON_BUILD_ROOT:-/opt/baron-stable-5.3.4}"
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export ANDROID_HOME="$baron_root/android/sdk"
baron_sdkmanager="$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager"
if [[ ! -f "$baron_sdkmanager" ]]; then
    mkdir -p "$ANDROID_HOME/cmdline-tools"
    curl -fL --retry 3 https://dl.google.com/android/repository/commandlinetools-linux-13114758_latest.zip -o "$baron_root/android/cmdline-tools.zip"
    unzip -q "$baron_root/android/cmdline-tools.zip" -d "$ANDROID_HOME/cmdline-tools"
    mv "$ANDROID_HOME/cmdline-tools/cmdline-tools" "$ANDROID_HOME/cmdline-tools/latest"
fi
yes | "$baron_sdkmanager" --sdk_root="$ANDROID_HOME" --licenses > "$baron_root/android/sdk-licenses.log" 2>&1 || true
"$baron_sdkmanager" --sdk_root="$ANDROID_HOME" 'platform-tools' 'platforms;android-36' 'build-tools;35.0.0' > "$baron_root/android/sdk-install.log" 2>&1
echo "Linux Android SDK is ready"
