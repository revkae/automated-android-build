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
#include <QSignalBlocker>

static QString stripAnsi(const QByteArray &raw) {
    QString text = QString::fromUtf8(raw);
    static QRegularExpression ansi("\x1b\\[[0-9;]*[A-Za-z]");
    text.remove(ansi);
    return text;
}

BuildTab::BuildTab(ProfileStore<AppProfileData> *store, QWidget *parent)
    : QWidget(parent)
    , store_(store)
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
    QPushButton *deleteProfileBtn = new QPushButton("Delete",       this);
    profileBar->addWidget(newProfileBtn);
    profileBar->addWidget(renameProfileBtn);
    profileBar->addWidget(saveProfileBtn);
    profileBar->addWidget(deleteProfileBtn);

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
        for (const QString &name : store_->profileNames())
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
    connect(deleteProfileBtn, &QPushButton::clicked,          this, &BuildTab::onDeleteProfile);
    connect(debugBtn,         &QPushButton::clicked,          this, &BuildTab::onBuildDebug);
    connect(releaseBtn,       &QPushButton::clicked,          this, &BuildTab::onBuildRelease);
    connect(process, &QProcess::readyReadStandardOutput,      this, &BuildTab::onOutputReady);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BuildTab::onFinished);
}

void BuildTab::syncProfile() {
    if (profileCombo->currentText().isEmpty()) return;
    SaveData d;
    d.projectDir   = projectDir->text();
    d.outputDir    = outputDir->text();
    d.package_     = package_->text();
    d.mainActivity = mainActivity->text();
    d.keyLocation  = keyLocation->text();
    d.keyAlias     = keyAlias->text();
    d.keyStorePass = keyStorePass->text();
    d.keyPass      = keyPass->text();
    emit profileChanged(d);
}

void BuildTab::refreshProfileList() {
    QSignalBlocker blocker(profileCombo);
    QString current = profileCombo->currentText();
    profileCombo->clear();
    for (const QString &name : store_->profileNames())
        profileCombo->addItem(name);
    if (profileCombo->findText(current) >= 0)
        profileCombo->setCurrentText(current);
}

QString BuildTab::nextProfileName() const {
    int n = 1;
    while (store_->exists(QString("Profile%1").arg(n)))
        ++n;
    return QString("Profile%1").arg(n);
}

void BuildTab::loadProfileIntoForm(const QString &name) {
    const SaveData &d = store_->load(name).build;
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
    QString name = QInputDialog::getText(this, "New Profile", "Profile name:");
    if (name.isEmpty()) return;
    if (store_->exists(name)) {
        QMessageBox::warning(this, "Build", "A profile with that name already exists.");
        return;
    }

    store_->save(name, AppProfileData{});
    profileCombo->addItem(name);
    {
        QSignalBlocker b(profileCombo);
        profileCombo->setCurrentText(name);
    }
    emit profileListChanged();
}

void BuildTab::onSaveProfile() {
    QString name = profileCombo->currentText();
    if (name.isEmpty()) return;
    AppProfileData data = store_->load(name);
    data.build.projectDir   = projectDir->text();
    data.build.outputDir    = outputDir->text();
    data.build.package_     = package_->text();
    data.build.mainActivity = mainActivity->text();
    data.build.keyLocation  = keyLocation->text();
    data.build.keyAlias     = keyAlias->text();
    data.build.keyStorePass = keyStorePass->text();
    data.build.keyPass      = keyPass->text();
    store_->save(name, data);
    emit profileChanged(data.build);
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
    if (store_->exists(newName)) {
        QMessageBox::warning(this, "Rename", "A profile with that name already exists.");
        return;
    }
    store_->rename(current, newName);
    profileCombo->setItemText(profileCombo->currentIndex(), newName);
    emit profileListChanged();
}

void BuildTab::onDeleteProfile() {
    QString name = profileCombo->currentText();
    if (name.isEmpty()) return;
    auto btn = QMessageBox::question(this, "Delete Profile",
        QString("Delete profile \"%1\"?").arg(name));
    if (btn != QMessageBox::Yes) return;
    store_->remove(name);
    int idx = profileCombo->currentIndex();
    {
        QSignalBlocker b(profileCombo);
        profileCombo->removeItem(idx);
    }
    if (profileCombo->count() > 0)
        loadProfileIntoForm(profileCombo->currentText());
    emit profileListChanged();
}

void BuildTab::onProfileChanged(const QString &name) {
    if (name.isEmpty()) return;
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
