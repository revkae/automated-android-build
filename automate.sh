#!/bin/bash

PROJECT_DIR="/Users/ravan/Documents/android"
OUTPUT_DIR="/Users/ravan/Documents/android/output"
PACKAGE="com.example.test"
MAIN_ACTIVITY="com.example.test.MainActivity"

set -e

print_blue()  { printf "\e[1;34m%s\e[0m\n" "$1"; }
print_green() { printf "\e[1;32m%s\e[0m\n" "$1"; }
print_red()   { printf "\e[1;31m%s\e[0m\n" "$1"; }

cd "$PROJECT_DIR"

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
