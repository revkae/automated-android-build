#include "mainwindow.h"
#include <QWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QProcessEnvironment>
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

    QFormLayout *form = new QFormLayout();

    projectDir   = new QLineEdit("/Users/ravan/Documents/android");
    outputDir    = new QLineEdit("/Users/ravan/Documents/android/output");
    package_     = new QLineEdit("com.example.test");
    mainActivity = new QLineEdit("com.example.test.MainActivity");
    keyLocation  = new QLineEdit("/Users/ravan/Documents/android/signed_key/Untitled.jks");
    keyAlias     = new QLineEdit("key0");
    keyStorePass = new QLineEdit("test123");
    keyPass      = new QLineEdit("test123");

    keyStorePass->setEchoMode(QLineEdit::Password);
    keyPass->setEchoMode(QLineEdit::Password);

    form->addRow("Project Dir:",     projectDir);
    form->addRow("Output Dir:",      outputDir);
    form->addRow("Package:",         package_);
    form->addRow("Main Activity:",   mainActivity);
    form->addRow("Key Location:",    keyLocation);
    form->addRow("Key Alias:",       keyAlias);
    form->addRow("Key Store Pass:",  keyStorePass);
    form->addRow("Key Pass:",        keyPass);

    QPushButton *debugBtn   = new QPushButton("Build Debug",   this);
    QPushButton *releaseBtn = new QPushButton("Build Release", this);

    logOutput = new QTextEdit(this);
    logOutput->setReadOnly(true);

    layout->addLayout(form);
    layout->addWidget(debugBtn);
    layout->addWidget(releaseBtn);
    layout->addWidget(logOutput);
    setCentralWidget(central);

    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    QString scriptDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/..");
    process->setWorkingDirectory(scriptDir);

    connect(debugBtn,   &QPushButton::clicked, this, &MainWindow::onBuildDebug);
    connect(releaseBtn, &QPushButton::clicked, this, &MainWindow::onBuildRelease);
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::onOutputReady);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onFinished);
}

void MainWindow::startBuild(const QString &type) {
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

void MainWindow::onBuildDebug()   { startBuild("debug");   }
void MainWindow::onBuildRelease() { startBuild("release"); }

void MainWindow::onOutputReady() {
    logOutput->append(stripAnsi(process->readAllStandardOutput()));
}

void MainWindow::onFinished(int exitCode, QProcess::ExitStatus) {
    if (exitCode == 0)
        logOutput->append("\n--- Build succeeded ---");
    else
        logOutput->append(QString("\n--- Build failed (exit %1) ---").arg(exitCode));
}
