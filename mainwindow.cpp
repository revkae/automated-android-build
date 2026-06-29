#include "mainwindow.h"
#include <QWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDir>

static QString stripAnsi(const QByteArray &raw) {
    QString text = QString::fromUtf8(raw);
    static QRegularExpression ansi("\x1b\\[[0-9;]*[A-Za-z]");
    text.remove(ansi);
    return text;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    QPushButton *debugBtn = new QPushButton("Build Debug", this);
    QPushButton *releaseBtn = new QPushButton("Build Release", this);
    logOutput = new QTextEdit(this);
    logOutput->setReadOnly(true);

    layout->addWidget(debugBtn);
    layout->addWidget(releaseBtn);
    layout->addWidget(logOutput);
    setCentralWidget(central);

    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    // automate.sh lives next to the source, one level above the build/ binary
    QString scriptDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/..");
    process->setWorkingDirectory(scriptDir);

    connect(debugBtn, &QPushButton::clicked, this, &MainWindow::onBuildDebug);
    connect(releaseBtn, &QPushButton::clicked, this, &MainWindow::onBuildRelease);
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::onOutputReady);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onFinished);
}

void MainWindow::onBuildDebug() {
    logOutput->clear();
    process->start("bash", QStringList() << "./automate.sh" << "debug");
}

void MainWindow::onBuildRelease() {
    logOutput->clear();
    process->start("bash", QStringList() << "./automate.sh" << "release");
}

void MainWindow::onOutputReady() {
    logOutput->append(stripAnsi(process->readAllStandardOutput()));
}

void MainWindow::onFinished(int exitCode, QProcess::ExitStatus) {
    if (exitCode == 0)
        logOutput->append("\n--- Build succeeded ---");
    else
        logOutput->append(QString("\n--- Build failed (exit %1) ---").arg(exitCode));
}
