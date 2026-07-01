# Project Architecture Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor `AndroidBuildTool` so every tab is a self-contained widget and profile persistence is a single generic class.

**Architecture:** Create `ProfileStore<T>` (header-only template) to replace the two near-identical save-system classes. Extract the Build tab into `BuildTab : QWidget` — mirroring the existing pattern of `VersionBumperWidget` and `GitHubActionsTab`. Reduce `MainWindow` to a ~40-line thin host that creates three tabs and connects `BuildTab::profileChanged` to `GitHubActionsTab::setProfile`.

**Tech Stack:** C++17, Qt6 (Widgets, Core), CMake 3.16, no new dependencies

## Global Constraints

- Qt6 only — no Qt5 APIs
- C++17 standard (`std::function`, lambdas, range-for)
- No new third-party dependencies — only Qt6::Core and Qt6::Widgets (already linked)
- No `.ui` files — all UI built in code
- Raw pointers for Qt-parented widgets (Qt parent-child memory management)
- No unit test framework exists — verification is build success + manual smoke test
- Do NOT add "Co-Authored-By" lines to commit messages

---

## File Map

| Action | File | Responsibility after change |
|--------|------|-----------------------------|
| Create | `profilestore.h` | Header-only template `ProfileStore<T>` — generic profile CRUD + JSON persistence |
| Create | `buildtab.h` | `BuildTab` widget declaration |
| Create | `buildtab.cpp` | `BuildTab` implementation — profile bar, form fields, build process, log, manifest auto-fill |
| Delete | `savesystem.h` | Replaced by `ProfileStore<SaveData>` |
| Delete | `savesystem.cpp` | Replaced by `ProfileStore<SaveData>` |
| Delete | `vb_savesystem.h` | Replaced by `ProfileStore<VBProfileData>` |
| Delete | `vb_savesystem.cpp` | Replaced by `ProfileStore<VBProfileData>` |
| Modify | `mainwindow.h` | Thin host — only constructor declaration |
| Modify | `mainwindow.cpp` | Creates 3 tabs, connects `BuildTab::profileChanged` signal |
| Modify | `versionbumperwidget.h` | Replace `VBSaveSystem` member with `ProfileStore<VBProfileData>` |
| Modify | `versionbumperwidget.cpp` | Add JSON includes; add serializer lambdas in member-init list |
| Modify | `CMakeLists.txt` | Remove deleted `.cpp` files; add `buildtab.cpp` |
| Unchanged | `githubactionstab.h/cpp` | Not touched |
| Unchanged | `savedata.h`, `vb_data.h`, `main.cpp` | Not touched |

**Note:** `ProfileStore<SaveData>` stores to `profiles-build.json`; `ProfileStore<VBProfileData>` stores to `profiles-vb.json`. These differ from the old filenames (`profiles.json`, `vb_profiles.json`). Existing saved profiles will not be picked up automatically — re-enter them after the refactor.

---

### Task 1: Create `ProfileStore<T>` header

**Files:**
- Create: `profilestore.h`

**Interfaces:**
- Produces:
  - `ProfileStore<T>(const QString &storageKey, ToJsonFn, FromJsonFn)` — constructor
  - `void save(const QString &name, const T &data)`
  - `T load(const QString &name) const`
  - `void rename(const QString &oldName, const QString &newName)`
  - `bool exists(const QString &name) const`
  - `QStringList profileNames() const`

The file stores to `QStandardPaths::AppConfigLocation + "/profiles-" + storageKey + ".json"`.

- [ ] **Step 1: Create `profilestore.h`**

```cpp
#pragma once
#include <functional>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStandardPaths>
#include <QStringList>

template<typename T>
class ProfileStore {
public:
    using ToJsonFn   = std::function<QJsonObject(const T&)>;
    using FromJsonFn = std::function<T(const QJsonObject&)>;

    ProfileStore(const QString &storageKey, ToJsonFn toJson, FromJsonFn fromJson)
        : toJson_(toJson), fromJson_(fromJson)
    {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(dir);
        filePath_ = dir + "/profiles-" + storageKey + ".json";
        readFromDisk();
    }

    void save(const QString &name, const T &data) {
        profiles_[name] = data;
        writeToDisk();
    }

    T load(const QString &name) const {
        return profiles_.value(name);
    }

    void rename(const QString &oldName, const QString &newName) {
        if (!profiles_.contains(oldName) || oldName == newName) return;
        profiles_[newName] = profiles_.take(oldName);
        writeToDisk();
    }

    bool exists(const QString &name) const {
        return profiles_.contains(name);
    }

    QStringList profileNames() const {
        return profiles_.keys();
    }

private:
    QMap<QString, T> profiles_;
    QString          filePath_;
    ToJsonFn         toJson_;
    FromJsonFn       fromJson_;

    void readFromDisk() {
        QFile f(filePath_);
        if (!f.open(QIODevice::ReadOnly)) return;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) return;
        QJsonObject root = doc.object();
        for (auto it = root.begin(); it != root.end(); ++it)
            if (it.value().isObject())
                profiles_[it.key()] = fromJson_(it.value().toObject());
    }

    void writeToDisk() {
        QJsonObject root;
        for (auto it = profiles_.cbegin(); it != profiles_.cend(); ++it)
            root[it.key()] = toJson_(it.value());
        QFile f(filePath_);
        if (f.open(QIODevice::WriteOnly))
            f.write(QJsonDocument(root).toJson());
    }
};
```

- [ ] **Step 2: Verify it's syntactically valid by building (no consumers changed yet)**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build 2>&1 | tail -20
```

Expected: clean build. `profilestore.h` is not included by anything yet so no compile errors.

- [ ] **Step 3: Commit**

```bash
git add profilestore.h
git commit -m "feat: add ProfileStore<T> generic profile persistence template"
```

---

### Task 2: Migrate `VersionBumperWidget` to `ProfileStore<VBProfileData>`

**Files:**
- Modify: `versionbumperwidget.h`
- Modify: `versionbumperwidget.cpp`
- Delete: `vb_savesystem.h`, `vb_savesystem.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ProfileStore<T>` from Task 1
- No change to `VersionBumperWidget`'s public interface

- [ ] **Step 1: Rewrite `versionbumperwidget.h`**

Replace `#include "vb_savesystem.h"` with `#include "profilestore.h"` and change the member type. Full new file:

```cpp
#pragma once
#include "vb_data.h"
#include "profilestore.h"

#include <QWidget>

class QComboBox;
class QTableWidget;
class QSpinBox;
class QLabel;

class VersionBumperWidget : public QWidget {
    Q_OBJECT

public:
    explicit VersionBumperWidget(QWidget *parent = nullptr);

private slots:
    void onNewProfile();
    void onRenameProfile();
    void onSaveProfile();
    void onProfileChanged(const QString &name);
    void onAddFile();
    void onRemove();
    void onApply();
    void onApplyWithAutomation();
    void refreshVersionPreview();

private:
    QComboBox    *profileCombo;
    QTableWidget *fileTable;
    QSpinBox     *segmentSpin;
    QSpinBox     *newCodeSpin;
    QLabel       *currentVersionLabel;
    QLabel       *newVersionLabel;
    QLabel       *currentCodeLabel;

    QList<FileEntryData>        entries_;
    ProfileStore<VBProfileData> saveSystem_;

    QString doApply();
    void    loadProfile(const VBProfileData &data);
    void    addFileRow(const QString &path, int occurrences);
    QString detectVersion(const QString &path) const;
    int     detectVersionCode(const QString &path) const;
};
```

- [ ] **Step 2: Update `versionbumperwidget.cpp` — add JSON includes and member-init serializers**

Add three includes after the existing includes block (around line 20, before `static const QRegularExpression`):

```cpp
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
```

Change the constructor signature from:

```cpp
VersionBumperWidget::VersionBumperWidget(QWidget *parent) : QWidget(parent) {
```

to:

```cpp
VersionBumperWidget::VersionBumperWidget(QWidget *parent)
    : QWidget(parent)
    , saveSystem_("vb",
        [](const VBProfileData &d) -> QJsonObject {
            QJsonArray files;
            for (const auto &f : d.files) {
                QJsonObject o;
                o["path"] = f.path;
                o["occurrences"] = f.occurrences;
                files.append(o);
            }
            QJsonObject root;
            root["files"] = files;
            root["segment"] = d.segment;
            root["newVersionCode"] = d.newVersionCode;
            return root;
        },
        [](const QJsonObject &o) -> VBProfileData {
            VBProfileData d;
            d.segment = o["segment"].toInt(1);
            d.newVersionCode = o["newVersionCode"].toInt(0);
            for (const auto &v : o["files"].toArray()) {
                QJsonObject f = v.toObject();
                d.files.append({f["path"].toString(), f["occurrences"].toInt(1)});
            }
            return d;
        })
{
```

The rest of the file (everything after `{`) is unchanged.

- [ ] **Step 3: Update `CMakeLists.txt` — remove `vb_savesystem.cpp`**

Change the `add_executable` block from:

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

to:

```cmake
add_executable(AndroidBuildTool
    main.cpp
    mainwindow.cpp
    savesystem.cpp
    versionbumperwidget.cpp
    githubactionstab.cpp
)
```

(`vb_savesystem.cpp` removed; `savesystem.cpp` stays for now — it gets removed in Task 4.)

- [ ] **Step 4: Delete `vb_savesystem.h` and `vb_savesystem.cpp`**

```bash
rm vb_savesystem.h vb_savesystem.cpp
```

- [ ] **Step 5: Build and verify**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build 2>&1 | tail -30
```

Expected: clean build, no errors. `VersionBumperWidget` now uses `ProfileStore<VBProfileData>`.

- [ ] **Step 6: Smoke test — launch app and verify Version Bump tab**

```bash
./build/AndroidBuildTool
```

Verify:
1. App launches without crashing
2. "Version Bump" tab appears and is functional
3. Creating a new profile, saving, and reloading works (data goes to `profiles-vb.json` in the app config dir)

- [ ] **Step 7: Commit**

```bash
git add profilestore.h versionbumperwidget.h versionbumperwidget.cpp CMakeLists.txt
git rm vb_savesystem.h vb_savesystem.cpp
git commit -m "refactor: replace VBSaveSystem with ProfileStore<VBProfileData>"
```

---

### Task 3: Create `BuildTab` widget

**Files:**
- Create: `buildtab.h`
- Create: `buildtab.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ProfileStore<SaveData>` from Task 1, `SaveData` from `savedata.h`
- Produces:
  - `BuildTab(QWidget *parent = nullptr)` — constructor
  - `signal profileChanged(const SaveData &data)` — emitted on profile load, save, and manifest auto-fill

- [ ] **Step 1: Create `buildtab.h`**

```cpp
#pragma once
#include "savedata.h"
#include "profilestore.h"
#include <QWidget>
#include <QProcess>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>

class BuildTab : public QWidget {
    Q_OBJECT
public:
    explicit BuildTab(QWidget *parent = nullptr);

signals:
    void profileChanged(const SaveData &data);

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

    ProfileStore<SaveData> saveSystem_;
    QComboBox  *profileCombo;
    QProcess   *process;
    QTextEdit  *logOutput;
    QLineEdit  *projectDir;
    QLineEdit  *outputDir;
    QLineEdit  *package_;
    QLineEdit  *mainActivity;
    QLineEdit  *keyLocation;
    QLineEdit  *keyAlias;
    QLineEdit  *keyStorePass;
    QLineEdit  *keyPass;
};
```

- [ ] **Step 2: Create `buildtab.cpp`**

```cpp
#include "buildtab.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QXmlStreamReader>
#include <QFile>
#include <QJsonObject>
#include <QSignalBlocker>

static QString stripAnsi(const QByteArray &raw) {
    QString text = QString::fromUtf8(raw);
    static QRegularExpression ansi("\x1b\\[[0-9;]*[A-Za-z]");
    text.remove(ansi);
    return text;
}

BuildTab::BuildTab(QWidget *parent)
    : QWidget(parent)
    , saveSystem_("build",
        [](const SaveData &d) -> QJsonObject {
            QJsonObject o;
            o["projectDir"]   = d.projectDir;
            o["outputDir"]    = d.outputDir;
            o["package_"]     = d.package_;
            o["mainActivity"] = d.mainActivity;
            o["keyLocation"]  = d.keyLocation;
            o["keyAlias"]     = d.keyAlias;
            o["keyStorePass"] = d.keyStorePass;
            o["keyPass"]      = d.keyPass;
            return o;
        },
        [](const QJsonObject &o) -> SaveData {
            SaveData d;
            d.projectDir   = o["projectDir"].toString();
            d.outputDir    = o["outputDir"].toString();
            d.package_     = o["package_"].toString();
            d.mainActivity = o["mainActivity"].toString();
            d.keyLocation  = o["keyLocation"].toString();
            d.keyAlias     = o["keyAlias"].toString();
            d.keyStorePass = o["keyStorePass"].toString();
            d.keyPass      = o["keyPass"].toString();
            return d;
        })
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *profileBar = new QHBoxLayout();
    profileBar->addWidget(new QLabel("Profile:"));
    profileCombo = new QComboBox(this);
    profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    profileBar->addWidget(profileCombo);
    QPushButton *newProfileBtn    = new QPushButton("New Profile",  this);
    QPushButton *renameProfileBtn = new QPushButton("Rename",       this);
    QPushButton *saveProfileBtn   = new QPushButton("Save Profile", this);
    profileBar->addWidget(newProfileBtn);
    profileBar->addWidget(renameProfileBtn);
    profileBar->addWidget(saveProfileBtn);

    QFormLayout *form = new QFormLayout();
    projectDir   = new QLineEdit(this);
    outputDir    = new QLineEdit(this);
    package_     = new QLineEdit(this);
    mainActivity = new QLineEdit(this);
    keyLocation  = new QLineEdit(this);
    keyAlias     = new QLineEdit(this);
    keyStorePass = new QLineEdit(this);
    keyPass      = new QLineEdit(this);

    keyStorePass->setEchoMode(QLineEdit::Password);
    keyPass->setEchoMode(QLineEdit::Password);

    auto makeBrowseDirRow = [&](QLineEdit *edit) {
        QWidget *w = new QWidget(this);
        QHBoxLayout *h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(edit);
        QPushButton *btn = new QPushButton("Browse…", this);
        connect(btn, &QPushButton::clicked, this, [=]() {
            QString dir = QFileDialog::getExistingDirectory(this, "Select Directory", edit->text());
            if (!dir.isEmpty()) edit->setText(dir);
        });
        h->addWidget(btn);
        return w;
    };

    auto makeBrowseFileRow = [&](QLineEdit *edit) {
        QWidget *w = new QWidget(this);
        QHBoxLayout *h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(edit);
        QPushButton *btn = new QPushButton("Browse…", this);
        connect(btn, &QPushButton::clicked, this, [=]() {
            QString file = QFileDialog::getOpenFileName(this, "Select File", edit->text());
            if (!file.isEmpty()) edit->setText(file);
        });
        h->addWidget(btn);
        return w;
    };

    {
        QWidget *w = new QWidget(this);
        QHBoxLayout *h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(projectDir);
        QPushButton *btn = new QPushButton("Browse…", this);
        connect(btn, &QPushButton::clicked, this, [=]() {
            QString dir = QFileDialog::getExistingDirectory(this, "Select Directory", projectDir->text());
            if (!dir.isEmpty()) {
                projectDir->setText(dir);
                autoFillFromManifest(dir);
            }
        });
        h->addWidget(btn);
        form->addRow("Project Dir:", w);
    }
    form->addRow("Output Dir:",     makeBrowseDirRow(outputDir));
    form->addRow("Package:",        package_);
    form->addRow("Main Activity:",  mainActivity);
    form->addRow("Key Location:",   makeBrowseFileRow(keyLocation));
    form->addRow("Key Alias:",      keyAlias);
    form->addRow("Key Store Pass:", keyStorePass);
    form->addRow("Key Pass:",       keyPass);

    QPushButton *debugBtn   = new QPushButton("Build Debug",   this);
    QPushButton *releaseBtn = new QPushButton("Build Release", this);
    logOutput = new QTextEdit(this);
    logOutput->setReadOnly(true);

    layout->addLayout(profileBar);
    layout->addLayout(form);
    layout->addWidget(debugBtn);
    layout->addWidget(releaseBtn);
    layout->addWidget(logOutput);

    {
        QSignalBlocker blocker(profileCombo);
        for (const QString &name : saveSystem_.profileNames())
            profileCombo->addItem(name);
    }
    if (profileCombo->count() > 0)
        loadProfileIntoForm(profileCombo->currentText());

    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    QString scriptDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/..");
    process->setWorkingDirectory(scriptDir);

    connect(profileCombo,     &QComboBox::currentTextChanged, this, &BuildTab::onProfileChanged);
    connect(newProfileBtn,    &QPushButton::clicked,          this, &BuildTab::onNewProfile);
    connect(renameProfileBtn, &QPushButton::clicked,          this, &BuildTab::onRenameProfile);
    connect(saveProfileBtn,   &QPushButton::clicked,          this, &BuildTab::onSaveProfile);
    connect(debugBtn,         &QPushButton::clicked,          this, &BuildTab::onBuildDebug);
    connect(releaseBtn,       &QPushButton::clicked,          this, &BuildTab::onBuildRelease);
    connect(process, &QProcess::readyReadStandardOutput,      this, &BuildTab::onOutputReady);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BuildTab::onFinished);
}

QString BuildTab::nextProfileName() const {
    int n = 1;
    while (saveSystem_.exists(QString("Profile%1").arg(n)))
        ++n;
    return QString("Profile%1").arg(n);
}

void BuildTab::loadProfileIntoForm(const QString &name) {
    SaveData d = saveSystem_.load(name);
    projectDir->setText(d.projectDir);
    outputDir->setText(d.outputDir);
    package_->setText(d.package_);
    mainActivity->setText(d.mainActivity);
    keyLocation->setText(d.keyLocation);
    keyAlias->setText(d.keyAlias);
    keyStorePass->setText(d.keyStorePass);
    keyPass->setText(d.keyPass);
    emit profileChanged(d);
}

void BuildTab::onNewProfile() {
    QString name = nextProfileName();
    saveSystem_.save(name, SaveData{});
    profileCombo->addItem(name);
    profileCombo->setCurrentText(name);
}

void BuildTab::onSaveProfile() {
    QString name = profileCombo->currentText();
    if (name.isEmpty()) return;
    SaveData d;
    d.projectDir   = projectDir->text();
    d.outputDir    = outputDir->text();
    d.package_     = package_->text();
    d.mainActivity = mainActivity->text();
    d.keyLocation  = keyLocation->text();
    d.keyAlias     = keyAlias->text();
    d.keyStorePass = keyStorePass->text();
    d.keyPass      = keyPass->text();
    saveSystem_.save(name, d);
    emit profileChanged(d);
    QMessageBox::information(this, "Build", QString("Profile \"%1\" saved.").arg(name));
}

void BuildTab::onRenameProfile() {
    QString current = profileCombo->currentText();
    if (current.isEmpty()) return;
    bool ok;
    QString newName = QInputDialog::getText(
        this, "Rename Profile", "New name:", QLineEdit::Normal, current, &ok);
    newName = newName.trimmed();
    if (!ok || newName.isEmpty() || newName == current) return;
    if (saveSystem_.exists(newName)) {
        QMessageBox::warning(this, "Rename", "A profile with that name already exists.");
        return;
    }
    saveSystem_.rename(current, newName);
    profileCombo->setItemText(profileCombo->currentIndex(), newName);
}

void BuildTab::onProfileChanged(const QString &name) {
    if (!name.isEmpty())
        loadProfileIntoForm(name);
}

void BuildTab::startBuild(const QString &type) {
    logOutput->clear();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PROJECT_DIR",    projectDir->text());
    env.insert("OUTPUT_DIR",     outputDir->text());
    env.insert("PACKAGE",        package_->text());
    env.insert("MAIN_ACTIVITY",  mainActivity->text());
    env.insert("KEY_LOCATION",   keyLocation->text());
    env.insert("KEY_ALIAS",      keyAlias->text());
    env.insert("KEY_STORE_PASS", keyStorePass->text());
    env.insert("KEY_PASS",       keyPass->text());
    process->setProcessEnvironment(env);
    process->start("bash", QStringList() << "./automate.sh" << type);
}

void BuildTab::onBuildDebug()   { startBuild("debug");   }
void BuildTab::onBuildRelease() { startBuild("release"); }

void BuildTab::onOutputReady() {
    logOutput->append(stripAnsi(process->readAllStandardOutput()));
}

void BuildTab::onFinished(int exitCode, QProcess::ExitStatus) {
    if (exitCode == 0)
        logOutput->append("\n--- Build succeeded ---");
    else
        logOutput->append(QString("\n--- Build failed (exit %1) ---").arg(exitCode));
}

void BuildTab::autoFillFromManifest(const QString &dir) {
    QStringList manifestCandidates = {
        dir + "/app/src/main/AndroidManifest.xml",
        dir + "/src/main/AndroidManifest.xml",
        dir + "/AndroidManifest.xml"
    };

    QString manifestPath;
    for (const QString &c : manifestCandidates) {
        if (QFile::exists(c)) { manifestPath = c; break; }
    }
    if (manifestPath.isEmpty()) return;

    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QXmlStreamReader xml(&file);
    QString pkg;
    QString rawActivity;
    QString currentActivity;
    bool inIntentFilter = false;
    bool hasMainAction  = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const auto name = xml.name();
            if (name == QLatin1String("manifest")) {
                pkg = xml.attributes().value("package").toString();
            } else if (name == QLatin1String("activity") || name == QLatin1String("activity-alias")) {
                currentActivity = xml.attributes().value("http://schemas.android.com/apk/res/android", "name").toString();
                inIntentFilter = false;
                hasMainAction  = false;
            } else if (name == QLatin1String("intent-filter")) {
                inIntentFilter = true;
            } else if (inIntentFilter && name == QLatin1String("action")) {
                if (xml.attributes().value("http://schemas.android.com/apk/res/android", "name") == "android.intent.action.MAIN")
                    hasMainAction = true;
            }
        } else if (xml.isEndElement()) {
            if (xml.name() == QLatin1String("intent-filter")) {
                if (hasMainAction && !currentActivity.isEmpty() && rawActivity.isEmpty())
                    rawActivity = currentActivity;
                inIntentFilter = false;
                hasMainAction  = false;
            }
        }
    }

    if (pkg.isEmpty()) {
        QStringList gradleCandidates = {
            dir + "/app/build.gradle.kts",
            dir + "/app/build.gradle",
            dir + "/build.gradle.kts",
            dir + "/build.gradle"
        };
        static QRegularExpression pkgRe(R"((?:namespace|applicationId)\s*=?\s*["']([A-Za-z][A-Za-z0-9_.]+)["'])");
        for (const QString &g : gradleCandidates) {
            QFile gf(g);
            if (!gf.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            auto m = pkgRe.match(QString::fromUtf8(gf.readAll()));
            if (m.hasMatch()) { pkg = m.captured(1); break; }
        }
    }

    QString activity = rawActivity;
    if (!activity.isEmpty() && activity.startsWith('.') && !pkg.isEmpty())
        activity = pkg + activity;

    if (!pkg.isEmpty())      package_->setText(pkg);
    if (!activity.isEmpty()) mainActivity->setText(activity);

    SaveData current;
    current.projectDir   = projectDir->text();
    current.outputDir    = outputDir->text();
    current.package_     = package_->text();
    current.mainActivity = mainActivity->text();
    current.keyLocation  = keyLocation->text();
    current.keyAlias     = keyAlias->text();
    current.keyStorePass = keyStorePass->text();
    current.keyPass      = keyPass->text();
    emit profileChanged(current);
}
```

- [ ] **Step 3: Update `CMakeLists.txt` — add `buildtab.cpp`**

Change the `add_executable` block to:

```cmake
add_executable(AndroidBuildTool
    main.cpp
    mainwindow.cpp
    savesystem.cpp
    versionbumperwidget.cpp
    buildtab.cpp
    githubactionstab.cpp
)
```

(`savesystem.cpp` stays until Task 4.)

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build 2>&1 | tail -30
```

Expected: clean build. `BuildTab` compiles alongside the still-unchanged `MainWindow` (which still uses `SaveSystem` and the old form fields — no conflict yet since nothing in MainWindow references BuildTab).

- [ ] **Step 5: Commit**

```bash
git add buildtab.h buildtab.cpp CMakeLists.txt
git commit -m "feat: add BuildTab widget extracting Build tab from MainWindow"
```

---

### Task 4: Thin out `MainWindow` and delete old save system

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Delete: `savesystem.h`, `savesystem.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `BuildTab` from Task 3, `VersionBumperWidget` (unchanged), `GitHubActionsTab` (unchanged)
- `BuildTab::profileChanged(const SaveData &)` → `GitHubActionsTab::setProfile(const SaveData &)`

- [ ] **Step 1: Rewrite `mainwindow.h`**

Replace the entire file with:

```cpp
#pragma once
#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
};
```

- [ ] **Step 2: Rewrite `mainwindow.cpp`**

Replace the entire file with:

```cpp
#include "mainwindow.h"
#include "versionbumperwidget.h"
#include "buildtab.h"
#include "githubactionstab.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout  = new QVBoxLayout(central);
    auto *tabs    = new QTabWidget(this);

    auto *versionBumper    = new VersionBumperWidget(this);
    auto *buildTab         = new BuildTab(this);
    auto *githubActionsTab = new GitHubActionsTab(this);

    tabs->addTab(versionBumper,    "Version Bump");
    tabs->addTab(buildTab,         "Build");
    tabs->addTab(githubActionsTab, "GitHub Actions");

    connect(buildTab, &BuildTab::profileChanged,
            githubActionsTab, &GitHubActionsTab::setProfile);

    layout->addWidget(tabs);
    setCentralWidget(central);
}
```

- [ ] **Step 3: Update `CMakeLists.txt` — remove `savesystem.cpp`**

Change the `add_executable` block to the final form:

```cmake
add_executable(AndroidBuildTool
    main.cpp
    mainwindow.cpp
    versionbumperwidget.cpp
    buildtab.cpp
    githubactionstab.cpp
)
```

- [ ] **Step 4: Delete `savesystem.h` and `savesystem.cpp`**

```bash
rm savesystem.h savesystem.cpp
```

- [ ] **Step 5: Build and verify**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build 2>&1 | tail -30
```

Expected: clean build, no errors or warnings. `SaveSystem` is gone; `MainWindow` is 20 lines; all three tabs compile.

- [ ] **Step 6: Full smoke test**

```bash
./build/AndroidBuildTool
```

Verify:
1. App launches without crashing
2. Three tabs appear in order: "Version Bump", "Build", "GitHub Actions"
3. **Build tab** — profile combo, all 8 form fields, Browse buttons, and Build Debug / Build Release buttons appear
4. **Profile flow** — create a new profile, fill in Key Alias, save profile → "GitHub Actions" tab preview updates with the saved alias in the signing section
5. **Profile load** — switch profile combo → GitHub Actions preview updates
6. **Auto-fill** — click Browse in Project Dir, select an Android project dir with a manifest → Package and Main Activity fields auto-fill
7. **Version Bump tab** — profile management still works (profiles save/load independently)
8. **GitHub Actions tab** — checkbox, preview, export all work as before

- [ ] **Step 7: Commit**

```bash
git add mainwindow.h mainwindow.cpp CMakeLists.txt
git rm savesystem.h savesystem.cpp
git commit -m "refactor: thin MainWindow to tab host, delete SaveSystem"
```
