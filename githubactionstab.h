#pragma once
#include "savedata.h"
#include <QWidget>
#include <QTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>

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
    QCheckBox *playDeployCheck_;
    QComboBox *trackCombo_;
    QLabel    *secretsNotice_;
    QComboBox *triggerCombo_;
    QLineEdit *branchEdit_;
    QComboBox *javaVersionCombo_;
    QLineEdit *filenameEdit_;
};
