#pragma once
#include <QMainWindow>
#include <QProcess>
#include <QTextEdit>

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
        QProcess  *process;
        QTextEdit *logOutput;
};
