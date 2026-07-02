#pragma once
#include "app_profile.h"
#include "profilestore.h"

#include <QWidget>

class QComboBox;
class QTableWidget;
class QSpinBox;
class QLabel;

class VersionBumperWidget : public QWidget {
    Q_OBJECT

public:
    explicit VersionBumperWidget(ProfileStore<AppProfileData> *store, QWidget *parent = nullptr);

signals:
    void profileListChanged();

public slots:
    void refreshProfileList();

private slots:
    void onNewProfile();
    void onRenameProfile();
    void onSaveProfile();
    void onDeleteProfile();
    void onProfileChanged(const QString &name);
    void onAddFile();
    void onRemove();
    void onApply();
    void onApplyWithAutomation();
    void refreshVersionPreview();

private:
    QComboBox    *profileCombo;
    QTableWidget *fileTable;
    QSpinBox     *segmentSpin;
    QSpinBox     *newCodeSpin;
    QLabel       *currentVersionLabel;
    QLabel       *newVersionLabel;
    QLabel       *currentCodeLabel;

    QList<FileEntryData>         entries_;
    ProfileStore<AppProfileData> *store_;

    QString doApply();
    void    loadProfile(const VBProfileData &data);
    void    addFileRow(const QString &path, int occurrences);
    QString detectVersion(const QString &path) const;
    int     detectVersionCode(const QString &path) const;
};
