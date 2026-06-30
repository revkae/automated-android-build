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
