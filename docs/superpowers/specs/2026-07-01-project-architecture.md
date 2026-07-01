# Project Architecture Refactor — Design Spec

**Date:** 2026-07-01
**Branch:** android-versionandbuild

---

## Overview

Refactor the Qt6 `AndroidBuildTool` to give it a consistent three-tab architecture where each tab is a fully self-contained widget. Currently the Build tab is embedded directly in `MainWindow` (335-line file), while the other two tabs (`VersionBumperWidget`, `GitHubActionsTab`) are proper self-contained widgets. Additionally, two nearly-identical save-system classes (`SaveSystem`, `VBSaveSystem`) duplicate ~150 lines of profile persistence logic.

This refactor:
1. Extracts the Build tab into a `BuildTab : QWidget` class
2. Unifies `SaveSystem` and `VBSaveSystem` into a single `ProfileStore<T>` template
3. Reduces `MainWindow` to a thin ~40-line host

---

## Section 1: `ProfileStore<T>` — unified save system

**File:** `profilestore.h` (header-only template)

Replaces both `SaveSystem` and `VBSaveSystem`. The only difference between those classes is the data type they serialize — that becomes a template parameter. Serialization (to/from `QJsonObject`) is passed as two lambdas at construction time, keeping the template itself free of any data-type knowledge.

```cpp
template<typename T>
class ProfileStore {
public:
    using ToJsonFn   = std::function<QJsonObject(const T&)>;
    using FromJsonFn = std::function<T(const QJsonObject&)>;

    ProfileStore(const QString &storageKey, ToJsonFn toJson, FromJsonFn fromJson);

    void        save(const QString &name, const T &data);
    T           load(const QString &name) const;
    void        rename(const QString &oldName, const QString &newName);
    bool        exists(const QString &name) const;
    QStringList profileNames() const;

private:
    QMap<QString, T> profiles_;
    QString filePath_;
    ToJsonFn toJson_;
    FromJsonFn fromJson_;
    void readFromDisk();
    void writeToDisk();
};
```

`storageKey` is a short string used to derive the JSON filename on disk: `QStandardPaths::AppConfigLocation + "/profiles-" + storageKey + ".json"` (e.g. `"build"` → `profiles-build.json`, `"vb"` → `profiles-vb.json`). This keeps the two stores' data separate on disk and avoids colliding with the old filenames (`profiles.json`, `vb_profiles.json`).

The serialization lambdas currently in `savesystem.cpp` and `vb_savesystem.cpp` move into the constructors of `BuildTab` and `VersionBumperWidget` respectively.

**`savesystem.h`, `savesystem.cpp`, `vb_savesystem.h`, `vb_savesystem.cpp` are deleted.**

---

## Section 2: `BuildTab` widget

**Files:** `buildtab.h`, `buildtab.cpp`

`BuildTab : QWidget` owns everything currently in `MainWindow`'s Build tab:

- `ProfileStore<SaveData> saveSystem_` — profile persistence
- `QComboBox *profileCombo` + New Profile / Rename / Save Profile buttons
- All 8 `QLineEdit` fields: `projectDir`, `outputDir`, `package_`, `mainActivity`, `keyLocation`, `keyAlias`, `keyStorePass`, `keyPass`
- `QProcess *process` + `QTextEdit *logOutput` — build execution and output
- Private `autoFillFromManifest(const QString &dir)` — moved verbatim from `mainwindow.cpp`

**Public interface:**

```cpp
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

`profileChanged(SaveData)` fires in:
- `loadProfileIntoForm()` — on profile combo change and app start
- `onSaveProfile()` — when the user saves, to keep GitHubActionsTab in sync
- `autoFillFromManifest()` — after auto-filling package/activity, so the GitHub Actions preview updates live

`GitHubActionsTab` is unchanged.

---

## Section 3: MainWindow — thin host

**Files:** `mainwindow.h`, `mainwindow.cpp`

`MainWindow` shrinks to ~40 lines. It creates the three tabs and wires the cross-tab signal:

```cpp
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

`mainwindow.h` retains only the class declaration with a single constructor — no member variables beyond what Qt parent-tracking needs.

---

## Section 4: VersionBumperWidget update

**Files:** `versionbumperwidget.h`, `versionbumperwidget.cpp`

The `VBSaveSystem saveSystem_` member is replaced with `ProfileStore<VBProfileData> saveSystem_`. The serialization lambdas (currently in `vb_savesystem.cpp`) move inline into the `VersionBumperWidget` constructor as a `ProfileStore<VBProfileData>` initializer. No behavior changes.

---

## File Changes Summary

| Action | Files |
|--------|-------|
| Create | `profilestore.h`, `buildtab.h`, `buildtab.cpp` |
| Delete | `savesystem.h`, `savesystem.cpp`, `vb_savesystem.h`, `vb_savesystem.cpp` |
| Modify | `mainwindow.h`, `mainwindow.cpp`, `versionbumperwidget.h`, `versionbumperwidget.cpp`, `CMakeLists.txt` |
| Unchanged | `githubactionstab.h`, `githubactionstab.cpp`, `savedata.h`, `vb_data.h`, `main.cpp` |

---

## Data Flow

```
MainWindow
├── VersionBumperWidget  [owns ProfileStore<VBProfileData>]
├── BuildTab             [owns ProfileStore<SaveData>, QProcess]
│     └── signal: profileChanged(SaveData)
└── GitHubActionsTab     [consumes SaveData via setProfile()]
      ↑
      connected by MainWindow
```

---

## Error Handling

No changes to existing error handling. All current `QMessageBox` dialogs for build errors, profile conflicts, and export failures remain in their respective widgets.

---

## Out of Scope

- Deduplicating `SaveData` / `VBProfileData` JSON serialization (separate concern)
- Extracting `autoFillFromManifest` into a standalone parser module
- Any new features
