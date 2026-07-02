# Android Build Tool

A Qt desktop app that automates building, versioning, and CI setup for Android projects.

## Requirements

- Qt 6
- CMake 3.16+
- Android SDK with build-tools (for local builds)
- `adb` in PATH (for debug installs)
- `jarsigner` in PATH (for AAB signing)

## Build

```bash
cmake -B build
cmake --build build
./build/AndroidBuildTool˛
```

## Tabs

### Build

Configure your Android project and run builds locally.

**Fields:**

| Field | Description |
|---|---|
| Project Dir | Root of your Android project (contains `gradlew`) |
| Output Dir | Where signed APK/AAB files are saved |
| Package | App package name, e.g. `com.example.app` |
| Main Activity | Fully qualified launcher activity |
| Key Location | Path to your `.jks` keystore file |
| Key Alias | Key alias inside the keystore |
| Key Store Pass / Key Pass | Keystore and key passwords |

When you select a **Project Dir**, the tool reads `AndroidManifest.xml` and auto-fills Package and Main Activity.

**Buttons:**
- **Build Debug** — runs `assembleDebug`, installs via `adb`, and launches the app
- **Build Release** — runs `assembleRelease` and signs the APK with `apksigner`

**Profiles** let you save and switch between multiple project configurations. Use **New Profile → fill fields → Save Profile**.

---

### Version Bump

Bump version names and version codes across multiple files at once (e.g. `build.gradle`, `package.json`, `pubspec.yaml`).

1. **Add File** — pick a file; the tool detects the current version and how many times it appears
2. Set **Segment**: `1` = patch, `2` = minor, `3` = major
3. Set **New Version Code** (the integer build number)
4. Preview shows current → new version before you commit

**Apply Version Bump** — updates the files in place.

**Apply Version Bump with Automation** — updates the files, then runs:
```
git add <files>
git commit -m "Bump version to vX.Y.Z"
git tag vX.Y.Z
git push origin HEAD vX.Y.Z
```
This triggers any CI workflow set up to run on tag pushes (see GitHub Actions tab).

---

### GitHub Actions

Generates a ready-to-use GitHub Actions workflow for Android builds.

**Options:**

| Option | Description |
|---|---|
| Trigger | Push tag (`v*`), push to branch, or manual dispatch |
| Java version | 17, 21, or 25 |
| Include signing step | Decodes keystore from a secret and signs APK + AAB |
| Deploy to Google Play | Uploads the signed AAB via `r0adkll/upload-google-play` |

**If signing is enabled**, add these secrets to your repo under *Settings → Secrets and variables → Actions*:

- `SIGNING_KEY` — base64-encoded keystore (`base64 -i your.jks` on macOS)
- `KEY_STORE_PASS` — keystore password
- `KEY_PASS` — key password

**If Google Play deploy is enabled**, also add:

- `SERVICE_ACCOUNT_JSON` — full contents of your Google Play service account JSON

**Export Workflow** saves the YAML to `.github/workflows/<filename>` inside the project directory and pushes it to the remote.

## Typical workflow

1. Open the **Build** tab, create a profile for your project, save it.
2. Use **Build Debug** during development to build and sideload instantly.
3. When ready to release, go to **Version Bump**, add your version files, bump the version with automation — this commits, tags, and pushes.
4. The tag push triggers the GitHub Actions workflow (generated in the **GitHub Actions** tab) which builds, signs, and optionally publishes to Google Play.
