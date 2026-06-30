# GitHub Actions Workflow Generator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "GitHub Actions" tab to the Qt build tool that generates and exports a ready-to-use `.github/workflows/android.yml` from the current build profile.

**Architecture:** A new `GitHubActionsTab` widget owns all generation and export logic. `MainWindow` instantiates it, adds it as a third tab, and calls `setProfile()` when the profile changes. No existing logic is restructured.

**Tech Stack:** C++17, Qt6 (Widgets, Core)

## Global Constraints

- Qt6 only — no Qt5 APIs
- C++17 standard
- New files must be added to `CMakeLists.txt` `add_executable` block
- No new dependencies beyond Qt6::Core and Qt6::Widgets (already linked)
- Follow existing code style: raw pointers for Qt-parented widgets, no `.ui` files

---

### Task 1: Create `GitHubActionsTab` widget

**Files:**
- Create: `githubactionstab.h`
- Create: `githubactionstab.cpp`
- Modify: `CMakeLists.txt` (add both new files to `add_executable`)

**Interfaces:**
- Produces:
  - `GitHubActionsTab(QWidget *parent = nullptr)` — constructor
  - `void setProfile(const SaveData &data)` — called by MainWindow on profile change

- [ ] **Step 1: Create `githubactionstab.h`**

```cpp
#pragma once
#include "savedata.h"
#include <QWidget>
#include <QTextEdit>
#include <QCheckBox>
#include <QLabel>

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

    SaveData   currentProfile_;
    QTextEdit *preview_;
    QCheckBox *signingCheck_;
    QLabel    *secretsNotice_;
};
```

- [ ] **Step 2: Create `githubactionstab.cpp`**

```cpp
#include "githubactionstab.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFont>

GitHubActionsTab::GitHubActionsTab(QWidget *parent) : QWidget(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);

    signingCheck_ = new QCheckBox("Include signing step", this);

    secretsNotice_ = new QLabel(
        "<b>Add these secrets to your GitHub repo Settings → Secrets and variables → Actions:</b><br>"
        "• <tt>SIGNING_KEY</tt> — keystore base64-encoded "
        "(<tt>base64 -i your.jks</tt> on macOS, <tt>base64 your.jks</tt> on Linux)<br>"
        "• <tt>KEY_STORE_PASS</tt> — your keystore password<br>"
        "• <tt>KEY_PASS</tt> — your key password",
        this);
    secretsNotice_->setWordWrap(true);
    secretsNotice_->setVisible(false);

    preview_ = new QTextEdit(this);
    preview_->setReadOnly(true);
    QFont mono("Courier");
    mono.setStyleHint(QFont::Monospace);
    preview_->setFont(mono);

    QPushButton *exportBtn = new QPushButton("Export Workflow", this);

    layout->addWidget(signingCheck_);
    layout->addWidget(secretsNotice_);
    layout->addWidget(preview_);
    layout->addWidget(exportBtn);

    connect(signingCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        secretsNotice_->setVisible(checked);
        updatePreview();
    });
    connect(exportBtn, &QPushButton::clicked, this, &GitHubActionsTab::onExport);

    updatePreview();
}

void GitHubActionsTab::setProfile(const SaveData &data) {
    currentProfile_ = data;
    updatePreview();
}

void GitHubActionsTab::updatePreview() {
    preview_->setPlainText(buildYaml());
}

QString GitHubActionsTab::buildYaml() const {
    QString yaml = R"(name: Android Release

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
)";

    if (signingCheck_->isChecked()) {
        QString alias = currentProfile_.keyAlias.isEmpty()
                        ? "your-key-alias"
                        : currentProfile_.keyAlias;
        yaml += R"(
      - name: Decode keystore
        run: |
          echo "${{ secrets.SIGNING_KEY }}" | base64 --decode > keystore.jks

      - name: Sign APK
        run: |
          BUILD_TOOLS=$(ls -d $ANDROID_HOME/build-tools/*/ | sort -V | tail -1)
          ${BUILD_TOOLS}apksigner sign \
            --ks keystore.jks \
            --ks-key-alias )" + alias + R"( \
            --ks-pass pass:${{ secrets.KEY_STORE_PASS }} \
            --key-pass pass:${{ secrets.KEY_PASS }} \
            --out app/build/outputs/apk/release/app-release-signed.apk \
            app/build/outputs/apk/release/app-release-unsigned.apk
)";
    }

    yaml += R"(
      - name: Upload APK
        uses: actions/upload-artifact@v4
        with:
          name: app-release
          path: app/build/outputs/apk/release/*.apk
)";

    return yaml;
}

void GitHubActionsTab::onExport() {
    if (currentProfile_.projectDir.isEmpty()) {
        QMessageBox::warning(this, "Export",
            "No project directory set. Load or save a profile first.");
        return;
    }

    QString workflowDir = currentProfile_.projectDir + "/.github/workflows";
    if (!QDir().mkpath(workflowDir)) {
        QMessageBox::warning(this, "Export",
            "Failed to create directory:\n" + workflowDir);
        return;
    }

    QString filePath = workflowDir + "/android.yml";
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export",
            "Failed to write file:\n" + filePath + "\n" + file.errorString());
        return;
    }

    QTextStream out(&file);
    out << buildYaml();
    file.close();

    QMessageBox::information(this, "Export",
        "Workflow exported to:\n" + filePath);
}
```

- [ ] **Step 3: Add new files to `CMakeLists.txt`**

In the `add_executable` block, add `githubactionstab.cpp` and (AUTOMOC picks up the header automatically via the `.cpp` include, but list it explicitly for clarity):

```cmake
add_executable(AndroidBuildTool
    main.cpp
    mainwindow.cpp
    savesystem.cpp
    vb_savesystem.cpp
    versionbumperwidget.cpp
    githubactionstab.cpp
)
```

- [ ] **Step 4: Build to verify it compiles**

```bash
cd /Users/ravan/Documents/bash/automated-android-build
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

Expected: build succeeds with no errors or warnings about `GitHubActionsTab`.

- [ ] **Step 5: Commit**

```bash
git add githubactionstab.h githubactionstab.cpp CMakeLists.txt
git commit -m "feat: add GitHubActionsTab widget with yaml generation and export"
```

---

### Task 2: Integrate `GitHubActionsTab` into `MainWindow`

**Files:**
- Modify: `mainwindow.h` — add include + member declaration
- Modify: `mainwindow.cpp` — instantiate tab, add to tabs, call setProfile

**Interfaces:**
- Consumes: `GitHubActionsTab(QWidget*)`, `void setProfile(const SaveData&)` from Task 1

- [ ] **Step 1: Update `mainwindow.h`**

Add the include and member. The diff is:

```cpp
// After existing includes at the top, add:
#include "githubactionstab.h"
```

And in the `private:` section, add after `QProcess *process;`:

```cpp
    GitHubActionsTab *githubActionsTab;
```

Full updated `mainwindow.h`:

```cpp
#pragma once
#include "savesystem.h"
#include "githubactionstab.h"
#include <QMainWindow>
#include <QProcess>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QTabWidget>

class VersionBumperWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void onNewProfile();
    void onSaveProfile();
    void onRenameProfile();
    void onProfileChanged(const QString &name);
    void onBuildDebug();
    void onBuildRelease();
    void onOutputReady();
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    void    startBuild(const QString &type);
    void    loadProfileIntoForm(const QString &name);
    void    autoFillFromManifest(const QString &dir);
    QString nextProfileName() const;

    SaveSystem saveSystem;
    QTabWidget *tabs;
    QComboBox  *profileCombo;

    QProcess         *process;
    QTextEdit        *logOutput;
    GitHubActionsTab *githubActionsTab;

    QLineEdit *projectDir;
    QLineEdit *outputDir;
    QLineEdit *package_;
    QLineEdit *mainActivity;
    QLineEdit *keyLocation;
    QLineEdit *keyAlias;
    QLineEdit *keyStorePass;
    QLineEdit *keyPass;
};
```

- [ ] **Step 2: Instantiate tab in `mainwindow.cpp` constructor**

After `tabs->addTab(buildTab, "Build");` (line 133), add:

```cpp
    githubActionsTab = new GitHubActionsTab(this);
    tabs->addTab(githubActionsTab, "GitHub Actions");
```

- [ ] **Step 3: Call `setProfile()` in `loadProfileIntoForm()`**

`loadProfileIntoForm()` currently ends at line 179 after setting `keyPass`. Add one line at the end:

```cpp
void MainWindow::loadProfileIntoForm(const QString &name) {
    SaveData d = saveSystem.load(name);
    projectDir->setText(d.projectDir);
    outputDir->setText(d.outputDir);
    package_->setText(d.package_);
    mainActivity->setText(d.mainActivity);
    keyLocation->setText(d.keyLocation);
    keyAlias->setText(d.keyAlias);
    keyStorePass->setText(d.keyStorePass);
    keyPass->setText(d.keyPass);
    githubActionsTab->setProfile(d);   // <- add this line
}
```

- [ ] **Step 4: Build to verify integration compiles**

```bash
cmake --build build
```

Expected: clean build, no errors.

- [ ] **Step 5: Manual smoke test**

Run the app:
```bash
./build/AndroidBuildTool
```

Verify:
1. Three tabs appear: "Version Bump", "Build", "GitHub Actions"
2. "GitHub Actions" tab shows the checkbox, preview pane (with YAML content), and "Export Workflow" button
3. Checking "Include signing step" shows the secrets notice and updates the YAML preview with decode + sign steps
4. Load a profile with a key alias set — the signing step in the preview shows the actual alias value
5. Click "Export Workflow" with no profile loaded — warning dialog appears
6. Load a profile with a project dir set, click "Export Workflow" — success dialog shows path, file exists at `<projectDir>/.github/workflows/android.yml`
7. Open the exported file and confirm it's valid YAML with correct content

- [ ] **Step 6: Commit**

```bash
git add mainwindow.h mainwindow.cpp
git commit -m "feat: integrate GitHubActionsTab into MainWindow"
```
