#include "mainwindow.h"
#include "versionbumperwidget.h"
#include "buildtab.h"
#include "githubactionstab.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , profileStore_("app", appProfileToJson, appProfileFromJson)
{
    auto *central = new QWidget(this);
    auto *layout  = new QVBoxLayout(central);
    auto *tabs    = new QTabWidget(this);

    auto *versionBumper    = new VersionBumperWidget(&profileStore_, this);
    auto *buildTab         = new BuildTab(&profileStore_, this);
    auto *githubActionsTab = new GitHubActionsTab(this);

    tabs->addTab(versionBumper,    "Version Bump");
    tabs->addTab(buildTab,         "Build");
    tabs->addTab(githubActionsTab, "GitHub Actions");

    connect(buildTab,      &BuildTab::profileChanged,
            githubActionsTab, &GitHubActionsTab::setProfile);
    connect(buildTab,      &BuildTab::profileListChanged,
            versionBumper, &VersionBumperWidget::refreshProfileList);
    connect(versionBumper, &VersionBumperWidget::profileListChanged,
            buildTab,      &BuildTab::refreshProfileList);
    buildTab->syncProfile();

    layout->addWidget(tabs);
    setCentralWidget(central);
}
