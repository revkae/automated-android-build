#include "mainwindow.h"
#include "versionbumperwidget.h"
#include <QWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDir>
#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QXmlStreamReader>
#include <QFile>

static QString stripAnsi(const QByteArray &raw) {
    QString text = QString::fromUtf8(raw);
    static QRegularExpression ansi("\x1b\\[[0-9;]*[A-Za-z]");
    text.remove(ansi);
    return text;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    tabs = new QTabWidget(this);
    mainLayout->addWidget(tabs);

    // --- Tab 1: Version Bump ---
    tabs->addTab(new VersionBumperWidget(this), "Version Bump");

    // --- Tab 2: Build ---
    QWidget *buildTab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(buildTab);

    QHBoxLayout *profileBar = new QHBoxLayout();
    profileBar->addWidget(new QLabel("Profile:"));
    profileCombo = new QComboBox(this);
    profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    profileBar->addWidget(profileCombo);
    QPushButton *newProfileBtn    = new QPushButton("New Profile",    this);
    QPushButton *renameProfileBtn = new QPushButton("Rename",         this);
    QPushButton *saveProfileBtn   = new QPushButton("Save Profile",   this);
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

    // Project Dir row: browse + auto-fill package/main activity from AndroidManifest.xml
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

    tabs->addTab(buildTab, "Build");
    setCentralWidget(central);

    // Populate combo from saved profiles (block signals so we only load once)
    {
        QSignalBlocker blocker(profileCombo);
        for (const QString &name : saveSystem.profileNames())
            profileCombo->addItem(name);
    }
    if (profileCombo->count() > 0)
        loadProfileIntoForm(profileCombo->currentText());

    // --- Process ---
    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    QString scriptDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/..");
    process->setWorkingDirectory(scriptDir);

    connect(profileCombo,    &QComboBox::currentTextChanged, this, &MainWindow::onProfileChanged);
    connect(newProfileBtn,   &QPushButton::clicked,          this, &MainWindow::onNewProfile);
    connect(renameProfileBtn,&QPushButton::clicked,          this, &MainWindow::onRenameProfile);
    connect(saveProfileBtn,  &QPushButton::clicked,          this, &MainWindow::onSaveProfile);
    connect(debugBtn,        &QPushButton::clicked,          this, &MainWindow::onBuildDebug);
    connect(releaseBtn,      &QPushButton::clicked,          this, &MainWindow::onBuildRelease);
    connect(process, &QProcess::readyReadStandardOutput,     this, &MainWindow::onOutputReady);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onFinished);
}

QString MainWindow::nextProfileName() const {
    int n = 1;
    while (saveSystem.exists(QString("Profile%1").arg(n)))
        ++n;
    return QString("Profile%1").arg(n);
}

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
}

void MainWindow::onNewProfile() {
    QString name = nextProfileName();
    saveSystem.save(name, SaveData{});
    profileCombo->addItem(name);
    profileCombo->setCurrentText(name);
}

void MainWindow::onSaveProfile() {
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
    saveSystem.save(name, d);
    QMessageBox::information(this, "Build", QString("Profile \"%1\" saved.").arg(name));
}

void MainWindow::onRenameProfile() {
    QString current = profileCombo->currentText();
    if (current.isEmpty()) return;
    bool ok;
    QString newName = QInputDialog::getText(
        this, "Rename Profile", "New name:", QLineEdit::Normal, current, &ok);
    newName = newName.trimmed();
    if (!ok || newName.isEmpty() || newName == current) return;
    if (saveSystem.exists(newName)) {
        QMessageBox::warning(this, "Rename", "A profile with that name already exists.");
        return;
    }
    saveSystem.rename(current, newName);
    profileCombo->setItemText(profileCombo->currentIndex(), newName);
}

void MainWindow::onProfileChanged(const QString &name) {
    if (!name.isEmpty())
        loadProfileIntoForm(name);
}

void MainWindow::startBuild(const QString &type) {
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

void MainWindow::onBuildDebug()   { startBuild("debug");   }
void MainWindow::onBuildRelease() { startBuild("release"); }

void MainWindow::onOutputReady() {
    logOutput->append(stripAnsi(process->readAllStandardOutput()));
}

void MainWindow::onFinished(int exitCode, QProcess::ExitStatus) {
    if (exitCode == 0)
        logOutput->append("\n--- Build succeeded ---");
    else
        logOutput->append(QString("\n--- Build failed (exit %1) ---").arg(exitCode));
}

void MainWindow::autoFillFromManifest(const QString &dir) {
    // --- Parse AndroidManifest.xml for activity name ---
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
    QString pkg;           // may be empty in modern AGP projects
    QString rawActivity;   // raw android:name, possibly relative (starts with '.')
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

    // --- If package not in manifest, check build.gradle / build.gradle.kts ---
    // AGP 7.3+ uses `namespace` in the module build file instead of manifest `package`
    if (pkg.isEmpty()) {
        QStringList gradleCandidates = {
            dir + "/app/build.gradle.kts",
            dir + "/app/build.gradle",
            dir + "/build.gradle.kts",
            dir + "/build.gradle"
        };
        // matches: namespace "com.foo" / namespace = "com.foo" / applicationId "com.foo" etc.
        static QRegularExpression pkgRe(R"((?:namespace|applicationId)\s*=?\s*["']([A-Za-z][A-Za-z0-9_.]+)["'])");
        for (const QString &g : gradleCandidates) {
            QFile gf(g);
            if (!gf.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            auto m = pkgRe.match(QString::fromUtf8(gf.readAll()));
            if (m.hasMatch()) { pkg = m.captured(1); break; }
        }
    }

    // Resolve relative activity name using the package we found
    QString activity = rawActivity;
    if (!activity.isEmpty() && activity.startsWith('.') && !pkg.isEmpty())
        activity = pkg + activity;

    if (!pkg.isEmpty())      package_->setText(pkg);
    if (!activity.isEmpty()) mainActivity->setText(activity);
}
