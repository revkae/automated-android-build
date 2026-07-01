#!/bin/bash

set -e

PROJECT_DIR="${PROJECT_DIR:-/Users/ravan/Documents/android}"
OUTPUT_DIR="${OUTPUT_DIR:-/Users/ravan/Documents/android/output}"
PACKAGE="${PACKAGE:-com.example.test}"
MAIN_ACTIVITY="${MAIN_ACTIVITY:-com.example.test.MainActivity}"
KEY_LOCATION="${KEY_LOCATION:-/Users/ravan/Documents/android/signed_key/Untitled.jks}"
KEY_ALIAS="${KEY_ALIAS:-key0}"
KEY_STORE_PASS="${KEY_STORE_PASS:-test123}"
KEY_PASS="${KEY_PASS:-test123}"

ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
BUILD_TOOLS_DIR=$(ls -d "$ANDROID_HOME/build-tools/"*/ 2>/dev/null | sort -V | tail -1)
export PATH="$BUILD_TOOLS_DIR:$ANDROID_HOME/platform-tools:$PATH"

print_blue()  { printf "\e[1;34m%s\e[0m\n" "$1"; }
print_green() { printf "\e[1;32m%s\e[0m\n" "$1"; }
print_red()   { printf "\e[1;31m%s\e[0m\n" "$1"; }

trap 'print_red "✗ Build failed!"' ERR

cd "$PROJECT_DIR"

BUILD_TYPE="${1:-debug}"

if [ "$BUILD_TYPE" = "bundle" ]; then
    print_blue "→ Building release AAB..."
    ./gradlew bundleRelease || { print_red "✗ Gradle bundle build failed"; exit 1; }

    mkdir -p "$OUTPUT_DIR"

    BUNDLE_PATH="app/build/outputs/bundle/release/app-release.aab"

    # Sign the AAB (jarsigner, not apksigner — apksigner doesn't sign .aab files)
    jarsigner -verbose \
        -sigalg SHA256withRSA -digestalg SHA-256 \
        -keystore "$KEY_LOCATION" \
        -storepass "$KEY_STORE_PASS" \
        -keypass "$KEY_PASS" \
        -signedjar "$OUTPUT_DIR/app-release-signed.aab" \
        "$BUNDLE_PATH" \
        "$KEY_ALIAS"

    print_green "✓ Signed AAB saved to $OUTPUT_DIR"
fi

if [ "$BUILD_TYPE" = "release" ]; then
    print_blue "→ Building release APK..."
    ./gradlew assembleRelease
    apksigner sign --ks "$KEY_LOCATION" \
            --ks-key-alias "$KEY_ALIAS" \
            --ks-pass pass:"$KEY_STORE_PASS" \
            --key-pass pass:"$KEY_PASS" \
            --out "$OUTPUT_DIR/app-release-signed.apk" \
            app/build/outputs/apk/release/app-release-unsigned.apk
    print_green "✓ Signed APK saved to $OUTPUT_DIR"
elif [ "$BUILD_TYPE" = "debug" ]; then
    print_blue "→ Building debug APK..."
    ./gradlew assembleDebug

    print_blue "→ Installing on device..."
    ./gradlew installDebug

    print_blue "→ Launching app..."
    adb shell am start -n "$PACKAGE/$MAIN_ACTIVITY" \
        -a android.intent.action.MAIN \
        -c android.intent.category.LAUNCHER

    mkdir -p "$OUTPUT_DIR"
    cp app/build/outputs/apk/debug/app-debug.apk "$OUTPUT_DIR/"
    print_green "✓ Done! APK saved to $OUTPUT_DIR"
fi
