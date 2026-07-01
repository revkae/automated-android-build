#pragma once
#include <QList>
#include <QString>

struct FileEntryData {
    QString path;
    int occurrences;
};

struct VBProfileData {
    QList<FileEntryData> files;
    int segment;
    int newVersionCode;
};
