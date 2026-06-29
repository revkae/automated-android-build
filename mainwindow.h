#pragma once
#include "savesystem.h"
#include <QMainWindow>
#include <QProcess>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>

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
    QString nextProfileName() const;

    SaveSystem saveSystem;
    QComboBox *profileCombo;

    QProcess  *process;
    QTextEdit *logOutput;

    QLineEdit *projectDir;
    QLineEdit *outputDir;
    QLineEdit *package_;
    QLineEdit *mainActivity;
    QLineEdit *keyLocation;
    QLineEdit *keyAlias;
    QLineEdit *keyStorePass;
    QLineEdit *keyPass;
};
