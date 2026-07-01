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
    void syncProfile();

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
