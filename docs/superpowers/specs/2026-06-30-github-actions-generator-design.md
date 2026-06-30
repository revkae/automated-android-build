# GitHub Actions Workflow Generator — Design Spec

**Date:** 2026-06-30  
**Branch:** versionandbuild  

---

## Overview

Add a "GitHub Actions" tab to the existing Qt build tool that generates a ready-to-use `.github/workflows/android.yml` file pre-filled from the currently loaded build profile. The workflow triggers on tag push (`v*`), builds a release APK, optionally signs it, and uploads it as a GitHub artifact.

---

## UI Layout

A third tab — "GitHub Actions" — added to `MainWindow` alongside "Version Bump" and "Build".

The tab contains (top to bottom):

1. **Checkbox** — "Include signing step" (unchecked by default)
2. **Preview pane** — read-only `QTextEdit` in monospace font showing the generated `.yml`. Updates live when the checkbox is toggled or the profile changes.
3. **Secrets notice** — a `QLabel` visible only when signing is checked:
   > Add these secrets to your GitHub repo Settings → Secrets:
   > - `SIGNING_KEY` — your keystore file base64-encoded (`base64 -i your.jks`)
   > - `KEY_STORE_PASS` — your keystore password
   > - `KEY_PASS` — your key password
4. **Export button** — writes the workflow to `<projectDir>/.github/workflows/android.yml`, shows a `QMessageBox` with the full path on success.

---

## New Files

| File | Purpose |
|------|---------|
| `githubactionstab.h` | `GitHubActionsTab` widget declaration |
| `githubactionstab.cpp` | Widget implementation, template generation, export logic |

---

## Class: `GitHubActionsTab`

```cpp
class GitHubActionsTab : public QWidget {
    Q_OBJECT
public:
    explicit GitHubActionsTab(QWidget *parent = nullptr);
    void setProfile(const SaveData &data);

private slots:
    void onExport();
    void updatePreview();

private:
    QString buildYaml() const;

    SaveData     currentProfile_;
    QTextEdit   *preview_;
    QCheckBox   *signingCheck_;
    QLabel      *secretsNotice_;
};
```

**`setProfile()`** — stores the profile and calls `updatePreview()`.  
**`updatePreview()`** — calls `buildYaml()` and sets the result on `preview_`.  
**`buildYaml()`** — fills the template string; inserts signing steps when `signingCheck_` is checked, substitutes `keyAlias` from `currentProfile_` into the signing step.  
**`onExport()`** — creates `<projectDir>/.github/workflows/` if needed, writes `android.yml`, shows result dialog.

---

## Changes to Existing Files

### `mainwindow.h`
- Add `#include "githubactionstab.h"`
- Add member: `GitHubActionsTab *githubActionsTab;`

### `mainwindow.cpp`
- Instantiate `GitHubActionsTab` and add as third tab: `tabs->addTab(githubActionsTab, "GitHub Actions")`
- In `loadProfileIntoForm()`: call `githubActionsTab->setProfile(d)` after loading into form fields

---

## Generated Workflow Template

### Without signing

```yaml
name: Android Release

on:
  push:
    tags:
      - 'v*'

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Set up JDK 17
        uses: actions/setup-java@v4
        with:
          java-version: '17'
          distribution: 'temurin'

      - name: Grant execute permission for gradlew
        run: chmod +x gradlew

      - name: Build release APK
        run: ./gradlew assembleRelease

      - name: Upload APK
        uses: actions/upload-artifact@v4
        with:
          name: app-release
          path: app/build/outputs/apk/release/*.apk
```

### Additional steps when signing is enabled

Inserted between "Build release APK" and "Upload APK":

```yaml
      - name: Decode keystore
        run: |
          echo "${{ secrets.SIGNING_KEY }}" | base64 --decode > keystore.jks

      - name: Sign APK
        run: |
          BUILD_TOOLS=$(ls -d $ANDROID_HOME/build-tools/*/ | sort -V | tail -1)
          ${BUILD_TOOLS}apksigner sign \
            --ks keystore.jks \
            --ks-key-alias <KEY_ALIAS_FROM_PROFILE> \
            --ks-pass pass:${{ secrets.KEY_STORE_PASS }} \
            --key-pass pass:${{ secrets.KEY_PASS }} \
            --out app/build/outputs/apk/release/app-release-signed.apk \
            app/build/outputs/apk/release/app-release-unsigned.apk
```

`<KEY_ALIAS_FROM_PROFILE>` is substituted from `SaveData::keyAlias` at generation time. Passwords and keystore use GitHub Secrets so they are never written to the file.

---

## Error Handling

- If `projectDir` is empty when Export is clicked: show a warning dialog — "No project directory set. Load or save a profile first."
- If the directory creation fails: show a warning dialog with the system error.
- If the file write fails: show a warning dialog with the system error.

---

## Out of Scope

- AAB (Android App Bundle) generation
- Debug build workflows
- Automatic GitHub Secrets upload
- Play Store deployment step
