#pragma once
#include "app_profile.h"
#include "profilestore.h"
#include <QWidget>
#include <QProcess>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>

class BuildTab : public QWidget {
    Q_OBJECT
public:
    explicit BuildTab(ProfileStore<AppProfileData> *store, QWidget *parent = nullptr);
    void syncProfile();

signals:
    void profileChanged(const SaveData &data);
    void profileListChanged();

public slots:
    void refreshProfileList();

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

    ProfileStore<AppProfileData> *store_;
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
