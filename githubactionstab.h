#pragma once
#include "savedata.h"
#include <QWidget>
#include <QTextEdit>
#include <QCheckBox>
#include <QLabel>

class GitHubActionsTab : public QWidget {
    Q_OBJECT
public:
    explicit GitHubActionsTab(QWidget *parent = nullptr);
    void setProfile(const SaveData &data);

private slots:
    void onExport();
    void updatePreview();

private:
    QString buildYaml() const;

    SaveData   currentProfile_;
    QTextEdit *preview_;
    QCheckBox *signingCheck_;
    QLabel    *secretsNotice_;
};
