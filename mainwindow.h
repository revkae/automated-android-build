#pragma once
#include <QMainWindow>
#include <QProcess>
#include <QTextEdit>
#include <QLineEdit>

class MainWindow : public QMainWindow {
    Q_OBJECT

    public:
        MainWindow(QWidget *parent = nullptr);

    private slots:
        void onBuildDebug();
        void onBuildRelease();
        void onOutputReady();
        void onFinished(int exitCode, QProcess::ExitStatus status);

    private:
        void startBuild(const QString &type);

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
