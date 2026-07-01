#include "mainwindow.h"
#include "versionbumperwidget.h"
#include "buildtab.h"
#include "githubactionstab.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout  = new QVBoxLayout(central);
    auto *tabs    = new QTabWidget(this);

    auto *versionBumper    = new VersionBumperWidget(this);
    auto *buildTab         = new BuildTab(this);
    auto *githubActionsTab = new GitHubActionsTab(this);

    tabs->addTab(versionBumper,    "Version Bump");
    tabs->addTab(buildTab,         "Build");
    tabs->addTab(githubActionsTab, "GitHub Actions");

    connect(buildTab, &BuildTab::profileChanged,
            githubActionsTab, &GitHubActionsTab::setProfile);

    layout->addWidget(tabs);
    setCentralWidget(central);
}
