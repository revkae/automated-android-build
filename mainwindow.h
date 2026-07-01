#pragma once
#include "app_profile.h"
#include "profilestore.h"
#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
private:
    ProfileStore<AppProfileData> profileStore_;
};
