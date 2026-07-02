#include "versionbumperwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QProcess>
#include <QFileInfo>

static const QRegularExpression kVerRe(R"((\d+)\.(\d+)(?:\.(\d+))?)");

static const QRegularExpression kCodeRe(
    R"(((?:versionCode|lockfileVersion|version_code|VERSION_CODE)["'\s]*[:=]\s*["']?)(\d+))");

static QString detectVersionInContent(const QString &content) {
    auto m = kVerRe.match(content);
    return m.hasMatch() ? m.captured(0) : QString{};
}

static int detectVersionCodeInContent(const QString &content) {
    auto m = kCodeRe.match(content);
    return m.hasMatch() ? m.captured(2).toInt() : -1;
}

static QString applyBump(const QString &version, int segment) {
    auto m = kVerRe.match(version);
    if (!m.hasMatch()) return version;
    int major = m.captured(1).toInt();
    int minor = m.captured(2).toInt();
    bool hasPatch = !m.captured(3).isEmpty();
    int patch = hasPatch ? m.captured(3).toInt() : 0;
    if (segment == 3) { major++; minor = 0; patch = 0; }
    else if (segment == 2) { minor++; patch = 0; }
    else { patch++; hasPatch = true; }
    if (hasPatch)
        return QString("%1.%2.%3").arg(major).arg(minor).arg(patch);
    return QString("%1.%2").arg(major).arg(minor);
}

VersionBumperWidget::VersionBumperWidget(ProfileStore<AppProfileData> *store, QWidget *parent)
    : QWidget(parent)
    , store_(store)
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(12, 12, 12, 12);

    // Profile bar
    auto *profileBar = new QHBoxLayout;
    profileBar->addWidget(new QLabel("Profile:"));
    profileCombo = new QComboBox;
    profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    profileBar->addWidget(profileCombo);
    auto *newProfileBtn    = new QPushButton("New Profile");
    auto *renameProfileBtn = new QPushButton("Rename");
    auto *saveProfileBtn   = new QPushButton("Save Profile");
    auto *deleteProfileBtn = new QPushButton("Delete");
    profileBar->addWidget(newProfileBtn);
    profileBar->addWidget(renameProfileBtn);
    profileBar->addWidget(saveProfileBtn);
    profileBar->addWidget(deleteProfileBtn);
    layout->addLayout(profileBar);

    // File table
    fileTable = new QTableWidget(0, 2, this);
    fileTable->setHorizontalHeaderLabels({"File", "Occurrences"});
    fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(fileTable);

    auto *btnRow = new QHBoxLayout;
    auto *addBtn    = new QPushButton("Add File");
    auto *removeBtn = new QPushButton("Remove");
    btnRow->addWidget(addBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    auto *form = new QFormLayout;

    segmentSpin = new QSpinBox;
    segmentSpin->setRange(1, 3);
    segmentSpin->setValue(1);
    segmentSpin->setToolTip("1 = patch, 2 = minor, 3 = major");
    form->addRow("Segment (1=patch 2=minor 3=major):", segmentSpin);

    newCodeSpin = new QSpinBox;
    newCodeSpin->setRange(0, 999999);
    newCodeSpin->setValue(0);
    form->addRow("New Version Code:", newCodeSpin);

    currentVersionLabel = new QLabel("—");
    newVersionLabel     = new QLabel("—");
    currentCodeLabel    = new QLabel("—");
    form->addRow("Current Version:", currentVersionLabel);
    form->addRow("New Version:", newVersionLabel);
    form->addRow("Current Version Code:", currentCodeLabel);
    layout->addLayout(form);

    auto *applyRow    = new QHBoxLayout;
    auto *applyBtn    = new QPushButton("Apply Version Bump");
    auto *applyAutoBtn = new QPushButton("Apply Version Bump with Automation");
    applyBtn->setFixedHeight(36);
    applyAutoBtn->setFixedHeight(36);
    applyRow->addWidget(applyBtn);
    applyRow->addWidget(applyAutoBtn);
    layout->addLayout(applyRow);

    // Populate profile combo
    {
        QSignalBlocker b(profileCombo);
        for (const QString &name : store_->profileNames())
            profileCombo->addItem(name);
    }
    if (profileCombo->count() > 0)
        loadProfile(store_->load(profileCombo->currentText()).vb);

    connect(newProfileBtn,    &QPushButton::clicked, this, &VersionBumperWidget::onNewProfile);
    connect(renameProfileBtn, &QPushButton::clicked, this, &VersionBumperWidget::onRenameProfile);
    connect(saveProfileBtn,   &QPushButton::clicked, this, &VersionBumperWidget::onSaveProfile);
    connect(deleteProfileBtn, &QPushButton::clicked, this, &VersionBumperWidget::onDeleteProfile);
    connect(profileCombo, &QComboBox::currentTextChanged, this, &VersionBumperWidget::onProfileChanged);
    connect(addBtn,    &QPushButton::clicked, this, &VersionBumperWidget::onAddFile);
    connect(removeBtn, &QPushButton::clicked, this, &VersionBumperWidget::onRemove);
    connect(applyBtn,     &QPushButton::clicked, this, &VersionBumperWidget::onApply);
    connect(applyAutoBtn, &QPushButton::clicked, this, &VersionBumperWidget::onApplyWithAutomation);
    connect(segmentSpin, &QSpinBox::valueChanged, this, &VersionBumperWidget::refreshVersionPreview);
}

void VersionBumperWidget::onNewProfile() {
    QString name = QInputDialog::getText(this, "New Profile", "Profile name:");
    if (name.isEmpty()) return;
    if (store_->exists(name)) {
        QMessageBox::warning(this, "Version Bumper", "A profile with that name already exists.");
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

void VersionBumperWidget::onRenameProfile() {
    QString current = profileCombo->currentText();
    if (current.isEmpty()) return;
    QString newName = QInputDialog::getText(this, "Rename Profile", "New name:", QLineEdit::Normal, current);
    if (newName.isEmpty() || newName == current) return;
    if (store_->exists(newName)) {
        QMessageBox::warning(this, "Version Bumper", "A profile with that name already exists.");
        return;
    }
    store_->rename(current, newName);
    int idx = profileCombo->currentIndex();
    {
        QSignalBlocker b(profileCombo);
        profileCombo->setItemText(idx, newName);
    }
    emit profileListChanged();
}

void VersionBumperWidget::onDeleteProfile() {
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
        loadProfile(store_->load(profileCombo->currentText()).vb);
    else
        loadProfile({});
    emit profileListChanged();
}

void VersionBumperWidget::onSaveProfile() {
    QString name = profileCombo->currentText();
    if (name.isEmpty()) {
        onNewProfile();
        return;
    }
    AppProfileData data = store_->load(name);
    data.vb.segment        = segmentSpin->value();
    data.vb.newVersionCode = newCodeSpin->value();
    data.vb.files          = entries_;
    store_->save(name, data);
    QMessageBox::information(this, "Version Bumper", QString("Profile \"%1\" saved.").arg(name));
}

void VersionBumperWidget::onProfileChanged(const QString &name) {
    if (name.isEmpty()) return;
    loadProfile(store_->load(name).vb);
}

void VersionBumperWidget::refreshProfileList() {
    QSignalBlocker blocker(profileCombo);
    QString current = profileCombo->currentText();
    profileCombo->clear();
    for (const QString &name : store_->profileNames())
        profileCombo->addItem(name);
    if (profileCombo->findText(current) >= 0)
        profileCombo->setCurrentText(current);
}

void VersionBumperWidget::onAddFile() {
    QString path = QFileDialog::getOpenFileName(this, "Select File");
    if (path.isEmpty()) return;

    bool wasEmpty = entries_.isEmpty();

    int occurrences = 1;
    {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(f.readAll());
            QString ver = detectVersionInContent(content);
            if (!ver.isEmpty()) {
                int count = content.count(ver);
                if (count > 0) occurrences = count;
            }
        }
    }

    addFileRow(path, occurrences);

    if (wasEmpty) {
        int code = detectVersionCode(path);
        if (code >= 0)
            newCodeSpin->setValue(code + 1);
    }

    refreshVersionPreview();
}

void VersionBumperWidget::onRemove() {
    int row = fileTable->currentRow();
    if (row < 0) return;
    fileTable->removeRow(row);
    entries_.removeAt(row);
    refreshVersionPreview();
}

QString VersionBumperWidget::doApply() {
    int segment        = segmentSpin->value();
    QString newCodeStr = QString::number(newCodeSpin->value());
    QString newVer;
    int errorCount = 0;

    for (const auto &entry : entries_) {
        QFile file(entry.path);
        if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) { errorCount++; continue; }
        QString content = file.readAll();

        QString currentVer = detectVersionInContent(content);
        if (currentVer.isEmpty()) { errorCount++; file.close(); continue; }

        QString bumped = applyBump(currentVer, segment);
        if (newVer.isEmpty()) newVer = bumped;

        int pos = 0;
        for (int i = 0; i < entry.occurrences; ++i) {
            int idx = content.indexOf(currentVer, pos);
            if (idx < 0) break;
            content.replace(idx, currentVer.length(), bumped);
            pos = idx + bumped.length();
        }

        QList<QRegularExpressionMatch> codeMatches;
        auto it = kCodeRe.globalMatch(content);
        while (it.hasNext()) codeMatches.prepend(it.next());
        for (const auto &cm : codeMatches)
            content.replace(cm.capturedStart(2), cm.capturedLength(2), newCodeStr);

        file.seek(0);
        file.resize(0);
        QTextStream out(&file);
        out << content;
        file.close();
    }

    refreshVersionPreview();

    if (errorCount > 0) {
        QMessageBox::warning(this, "Version Bumper",
            QString("%1 file(s) could not be updated.").arg(errorCount));
        return {};
    }
    return newVer;
}

void VersionBumperWidget::onApply() {
    if (entries_.isEmpty()) {
        QMessageBox::information(this, "Version Bumper", "No files in the list.");
        return;
    }
    if (!doApply().isEmpty())
        QMessageBox::information(this, "Version Bumper", "All files updated successfully.");
}

void VersionBumperWidget::onApplyWithAutomation() {
    if (entries_.isEmpty()) {
        QMessageBox::information(this, "Version Bumper", "No files in the list.");
        return;
    }

    QString newVer = doApply();
    if (newVer.isEmpty()) return;

    QString tag     = "v" + newVer;
    QString workDir = QFileInfo(entries_.first().path).absolutePath();

    // Stage all modified version files
    QStringList addArgs = {"-C", workDir, "add"};
    for (const auto &entry : entries_)
        addArgs << entry.path;

    QProcess gitAdd;
    gitAdd.setProcessChannelMode(QProcess::MergedChannels);
    gitAdd.start("git", addArgs);
    if (!gitAdd.waitForFinished(10000) || gitAdd.exitCode() != 0) {
        QMessageBox::warning(this, "Version Bumper",
            QString("git add failed:\n%1").arg(QString::fromUtf8(gitAdd.readAll())));
        return;
    }

    // Commit the version bump before tagging
    QProcess gitCommit;
    gitCommit.setProcessChannelMode(QProcess::MergedChannels);
    gitCommit.start("git", {"-C", workDir, "commit", "-m", "Bump version to " + tag});
    if (!gitCommit.waitForFinished(10000) || gitCommit.exitCode() != 0) {
        QMessageBox::warning(this, "Version Bumper",
            QString("git commit failed:\n%1").arg(QString::fromUtf8(gitCommit.readAll())));
        return;
    }

    // Tag the commit that contains the version bump
    QProcess gitTag;
    gitTag.setProcessChannelMode(QProcess::MergedChannels);
    gitTag.start("git", {"-C", workDir, "tag", tag});
    if (!gitTag.waitForFinished(10000) || gitTag.exitCode() != 0) {
        QMessageBox::warning(this, "Version Bumper",
            QString("git tag %1 failed:\n%2").arg(tag, QString::fromUtf8(gitTag.readAll())));
        return;
    }

    // Push the commit and the tag together
    QProcess gitPush;
    gitPush.setProcessChannelMode(QProcess::MergedChannels);
    gitPush.start("git", {"-C", workDir, "push", "origin", "HEAD", tag});
    if (!gitPush.waitForFinished(30000) || gitPush.exitCode() != 0) {
        QMessageBox::warning(this, "Version Bumper",
            QString("git push failed:\n%1").arg(QString::fromUtf8(gitPush.readAll())));
        return;
    }

    QMessageBox::information(this, "Version Bumper",
        QString("Version bumped to %1, tag %2 created and pushed.").arg(newVer, tag));
}

void VersionBumperWidget::refreshVersionPreview() {
    if (entries_.isEmpty()) {
        currentVersionLabel->setText("—");
        newVersionLabel->setText("—");
        currentCodeLabel->setText("—");
        return;
    }

    const QString &firstPath = entries_.first().path;
    QString current = detectVersion(firstPath);
    if (current.isEmpty()) {
        currentVersionLabel->setText("(not found)");
        newVersionLabel->setText("—");
    } else {
        currentVersionLabel->setText(current);
        newVersionLabel->setText(applyBump(current, segmentSpin->value()));
    }

    int code = detectVersionCode(firstPath);
    currentCodeLabel->setText(code >= 0 ? QString::number(code) : "(not found)");
}

void VersionBumperWidget::loadProfile(const VBProfileData &data) {
    fileTable->setRowCount(0);
    entries_.clear();

    for (const auto &f : data.files)
        addFileRow(f.path, f.occurrences);

    {
        QSignalBlocker b(segmentSpin);
        segmentSpin->setValue(data.segment > 0 ? data.segment : 1);
    }
    newCodeSpin->setValue(data.newVersionCode);

    refreshVersionPreview();
}

void VersionBumperWidget::addFileRow(const QString &path, int occurrences) {
    int row = fileTable->rowCount();
    fileTable->insertRow(row);
    fileTable->setItem(row, 0, new QTableWidgetItem(path));

    auto *occSpin = new QSpinBox;
    occSpin->setRange(1, 999);
    occSpin->setValue(occurrences);
    occSpin->setAlignment(Qt::AlignCenter);
    fileTable->setCellWidget(row, 1, occSpin);

    entries_.append({path, occurrences});

    connect(occSpin, &QSpinBox::valueChanged, this, [this, row](int val) {
        if (row < entries_.size())
            entries_[row].occurrences = val;
    });
}

QString VersionBumperWidget::detectVersion(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return detectVersionInContent(QString::fromUtf8(file.readAll()));
}

int VersionBumperWidget::detectVersionCode(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return -1;
    return detectVersionCodeInContent(QString::fromUtf8(file.readAll()));
}
