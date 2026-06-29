#pragma once
#include "savedata.h"
#include <QMap>
#include <QStringList>

class SaveSystem {
public:
    SaveSystem();

    void        save(const QString &name, const SaveData &data);
    SaveData    load(const QString &name) const;
    void        rename(const QString &oldName, const QString &newName);
    bool        exists(const QString &name) const;
    QStringList profileNames() const;

private:
    QMap<QString, SaveData> profiles_;
    QString                 filePath_;

    void readFromDisk();
    void writeToDisk();
};
