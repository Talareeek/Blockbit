#!/bin/bash
# Android build wrapper. Sets up JAVA_HOME/ANDROID_HOME env and forwards args
# to Gradle, so you don't have to export them every shell.
#
# Examples:
#   ./android-build.sh assembleDebug
#   ./android-build.sh assembleDebug -PARCH_ABI=x86_64   # emulator ABI
#   ./android-build.sh installDebug                       # install on adb device
#   ./android-build.sh clean

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# JDK 21 is required by AGP 8.7. Our system has JDK 26 which AGP rejects.
: "${JAVA_HOME:=$HOME/jdks/jdk-21.0.13+9}"
: "${ANDROID_HOME:=$HOME/Android/Sdk}"
export JAVA_HOME
export ANDROID_HOME
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export PATH="$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/emulator:$PATH"

if [ ! -x "$JAVA_HOME/bin/java" ]; then
    echo "ERROR: JDK 21 not found at $JAVA_HOME" >&2
    echo "Install with:" >&2
    echo "  mkdir -p ~/jdks && cd ~/jdks" >&2
    echo "  curl -fsSL -o jdk21.tar.gz 'https://api.adoptium.net/v3/binary/latest/21/ga/linux/x64/jdk/hotspot/normal/eclipse'" >&2
    echo "  bsdtar -xf jdk21.tar.gz && rm jdk21.tar.gz" >&2
    exit 1
fi

if [ ! -d "$ANDROID_HOME" ]; then
    echo "ERROR: Android SDK not found at $ANDROID_HOME" >&2
    exit 1
fi

cd "$SCRIPT_DIR/android"
exec ./gradlew "$@"
