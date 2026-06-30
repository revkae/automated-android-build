#pragma once
#include "vb_data.h"
#include <QMap>
#include <QStringList>

class VBSaveSystem {
public:
    VBSaveSystem();

    void          save(const QString &name, const VBProfileData &data);
    VBProfileData load(const QString &name) const;
    void          rename(const QString &oldName, const QString &newName);
    bool          exists(const QString &name) const;
    QStringList   profileNames() const;

private:
    QMap<QString, VBProfileData> profiles_;
    QString                       filePath_;

    void readFromDisk();
    void writeToDisk();
};
