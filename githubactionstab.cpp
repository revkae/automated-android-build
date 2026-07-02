#include "githubactionstab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFont>
#include <QProcess>

GitHubActionsTab::GitHubActionsTab(QWidget *parent) : QWidget(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Trigger selector
    QFormLayout *optionsForm = new QFormLayout;

    triggerCombo_ = new QComboBox(this);
    triggerCombo_->addItems({"Push tag (v*)", "Push to branch", "Manual (workflow_dispatch)"});
    optionsForm->addRow("Trigger:", triggerCombo_);

    branchEdit_ = new QLineEdit("main", this);
    branchEdit_->setVisible(false);
    optionsForm->addRow("Branch:", branchEdit_);

    javaVersionCombo_ = new QComboBox(this);
    javaVersionCombo_->addItems({"17", "21", "25"});
    optionsForm->addRow("Java version:", javaVersionCombo_);

    filenameEdit_ = new QLineEdit("android.yml", this);
    optionsForm->addRow("Filename:", filenameEdit_);

    layout->addLayout(optionsForm);

    signingCheck_ = new QCheckBox("Include signing step", this);

    secretsNotice_ = new QLabel(this);
    secretsNotice_->setWordWrap(true);
    secretsNotice_->setVisible(false);

    playDeployCheck_ = new QCheckBox("Deploy to Google Play", this);
    playDeployCheck_->setEnabled(false);

    trackCombo_ = new QComboBox(this);
    trackCombo_->addItems({"internal", "alpha", "beta", "production"});
    trackCombo_->setVisible(false);

    QHBoxLayout *deployRow = new QHBoxLayout;
    deployRow->addWidget(playDeployCheck_);
    deployRow->addWidget(trackCombo_);
    deployRow->addStretch();

    preview_ = new QTextEdit(this);
    preview_->setReadOnly(true);
    QFont mono("Courier");
    mono.setStyleHint(QFont::Monospace);
    preview_->setFont(mono);

    QPushButton *exportBtn = new QPushButton("Export Workflow", this);

    layout->addWidget(signingCheck_);
    layout->addWidget(secretsNotice_);
    layout->addLayout(deployRow);
    layout->addWidget(preview_);
    layout->addWidget(exportBtn);

    connect(triggerCombo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        branchEdit_->setVisible(idx == 1);
        updatePreview();
    });
    connect(branchEdit_, &QLineEdit::textChanged, this, [this]() { updatePreview(); });
    connect(javaVersionCombo_, &QComboBox::currentIndexChanged, this, [this]() { updatePreview(); });
    connect(signingCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        playDeployCheck_->setEnabled(checked);
        if (!checked) {
            playDeployCheck_->setChecked(false);
            trackCombo_->setVisible(false);
        }
        secretsNotice_->setVisible(checked);
        updatePreview();
    });
    connect(playDeployCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        trackCombo_->setVisible(checked);
        updatePreview();
    });
    connect(trackCombo_, &QComboBox::currentIndexChanged, this, [this]() {
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
    QString notice =
        "<b>Add these secrets to your GitHub repo Settings → Secrets and variables → Actions:</b><br>"
        "• <tt>SIGNING_KEY</tt> — keystore base64-encoded "
        "(<tt>base64 -i your.jks</tt> on macOS, <tt>base64 your.jks</tt> on Linux)<br>"
        "• <tt>KEY_STORE_PASS</tt> — your keystore password<br>"
        "• <tt>KEY_PASS</tt> — your key password";
    if (playDeployCheck_->isChecked()) {
        notice += "<br>• <tt>SERVICE_ACCOUNT_JSON</tt> — Google Play service account key "
                  "(paste the entire contents of your .json file)";
    }
    secretsNotice_->setText(notice);
    preview_->setPlainText(buildYaml());
}

QString GitHubActionsTab::buildYaml() const {
    QString javaVersion = javaVersionCombo_->currentText();

    QString onBlock;
    int triggerIdx = triggerCombo_->currentIndex();
    if (triggerIdx == 0) {
        onBlock = R"(on:
  push:
    tags:
      - 'v*'
)";
    } else if (triggerIdx == 1) {
        QString branch = branchEdit_->text().trimmed();
        if (branch.isEmpty()) branch = "main";
        onBlock = "on:\n  push:\n    branches:\n      - '" + branch + "'\n";
    } else {
        onBlock = R"(on:
  workflow_dispatch:
)";
    }

    QString yaml = "name: Android Release\n\n" + onBlock + R"(
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Set up JDK )" + javaVersion + R"(
        uses: actions/setup-java@v4
        with:
          java-version: ')" + javaVersion + R"('
          distribution: 'temurin'

      - name: Grant execute permission for gradlew
        run: chmod +x gradlew

      - name: Build release APK
        run: ./gradlew assembleRelease

      - name: Build release AAB
        run: ./gradlew bundleRelease
)";

    if (signingCheck_->isChecked()) {
        QString alias = currentProfile_.keyAlias.isEmpty()
                        ? "your-key-alias"
                        : currentProfile_.keyAlias;
        alias.remove('\n').remove('\r');
        alias.replace("\"", "\\\"");
        yaml += R"(
      - name: Decode keystore
        run: |
          echo "${{ secrets.SIGNING_KEY }}" | base64 --decode > keystore.jks

      - name: Sign APK
        run: |
          BUILD_TOOLS=$(ls -d $ANDROID_HOME/build-tools/*/ | sort -V | tail -1)
          ${BUILD_TOOLS}apksigner sign \
            --ks keystore.jks \
            --ks-key-alias ")" + alias + R"(" \
            --ks-pass pass:${{ secrets.KEY_STORE_PASS }} \
            --key-pass pass:${{ secrets.KEY_PASS }} \
            --out app/build/outputs/apk/release/app-release-signed.apk \
            app/build/outputs/apk/release/app-release-unsigned.apk

      - name: Sign AAB
        run: |
          jarsigner -verbose \
            -sigalg SHA256withRSA -digestalg SHA-256 \
            -keystore keystore.jks \
            -storepass ${{ secrets.KEY_STORE_PASS }} \
            -keypass ${{ secrets.KEY_PASS }} \
            -signedjar app/build/outputs/bundle/release/app-release-signed.aab \
            app/build/outputs/bundle/release/app-release.aab \
            )" + alias + R"(
)";

        if (playDeployCheck_->isChecked()) {
            QString pkg = currentProfile_.package_.isEmpty()
                          ? "com.example.app"
                          : currentProfile_.package_;
            pkg.remove('\n').remove('\r');
            QString track = trackCombo_->currentText();
            yaml += R"(
      - name: Deploy to Google Play
        uses: r0adkll/upload-google-play@v1
        with:
          serviceAccountJsonPlainText: ${{ secrets.SERVICE_ACCOUNT_JSON }}
          packageName: )" + pkg + R"(
          releaseFiles: app/build/outputs/bundle/release/app-release-signed.aab
          track: )" + track + R"(
)";
        }
    }

    yaml += R"(
      - name: Upload APK
        uses: actions/upload-artifact@v4
        with:
          name: app-release
          path: app/build/outputs/apk/release/*.apk

      - name: Upload AAB
        uses: actions/upload-artifact@v4
        with:
          name: app-release-aab
          path: app/build/outputs/bundle/release/app-release.aab
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

    QString filename = filenameEdit_->text().trimmed();
    if (filename.isEmpty()) filename = "android.yml";

    QString filePath = workflowDir + "/" + filename;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export",
            "Failed to write file:\n" + filePath + "\n" + file.errorString());
        return;
    }

    QTextStream out(&file);
    out << buildYaml();
    file.close();

    QString relPath = ".github/workflows/" + filename;

    QProcess gitAdd;
    gitAdd.setProcessChannelMode(QProcess::MergedChannels);
    gitAdd.start("git", {"-C", currentProfile_.projectDir, "add", relPath});
    if (!gitAdd.waitForFinished(10000) || gitAdd.exitCode() != 0) {
        QMessageBox::warning(this, "Export",
            "Workflow written but git add failed:\n" + QString::fromUtf8(gitAdd.readAll()));
        return;
    }

    QProcess gitCommit;
    gitCommit.setProcessChannelMode(QProcess::MergedChannels);
    gitCommit.start("git", {"-C", currentProfile_.projectDir, "commit", "-m", "update GitHub Actions workflow"});
    if (!gitCommit.waitForFinished(10000) || gitCommit.exitCode() != 0) {
        QMessageBox::warning(this, "Export",
            "Workflow written but git commit failed:\n" + QString::fromUtf8(gitCommit.readAll()));
        return;
    }

    QProcess gitPush;
    gitPush.setProcessChannelMode(QProcess::MergedChannels);
    gitPush.start("git", {"-C", currentProfile_.projectDir, "push"});
    if (!gitPush.waitForFinished(30000) || gitPush.exitCode() != 0) {
        QMessageBox::warning(this, "Export",
            "Workflow committed but git push failed:\n" + QString::fromUtf8(gitPush.readAll()));
        return;
    }

    QMessageBox::information(this, "Export",
        "Workflow exported, committed, and pushed:\n" + filePath);
}
