#include "vb_savesystem.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

static QString configPath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/vb_profiles.json";
}

static QJsonObject toJson(const VBProfileData &d) {
    QJsonArray files;
    for (const auto &f : d.files) {
        QJsonObject o;
        o["path"] = f.path;
        o["occurrences"] = f.occurrences;
        files.append(o);
    }
    QJsonObject root;
    root["files"] = files;
    root["segment"] = d.segment;
    root["newVersionCode"] = d.newVersionCode;
    return root;
}

static VBProfileData fromJson(const QJsonObject &o) {
    VBProfileData d;
    d.segment = o["segment"].toInt(1);
    d.newVersionCode = o["newVersionCode"].toInt(0);
    for (const auto &v : o["files"].toArray()) {
        QJsonObject f = v.toObject();
        d.files.append({f["path"].toString(), f["occurrences"].toInt(1)});
    }
    return d;
}

VBSaveSystem::VBSaveSystem() {
    filePath_ = configPath();
    readFromDisk();
}

void VBSaveSystem::save(const QString &name, const VBProfileData &data) {
    profiles_[name] = data;
    writeToDisk();
}

VBProfileData VBSaveSystem::load(const QString &name) const {
    return profiles_.value(name);
}

void VBSaveSystem::rename(const QString &oldName, const QString &newName) {
    if (!profiles_.contains(oldName) || oldName == newName) return;
    profiles_[newName] = profiles_.take(oldName);
    writeToDisk();
}

bool VBSaveSystem::exists(const QString &name) const {
    return profiles_.contains(name);
}

QStringList VBSaveSystem::profileNames() const {
    return profiles_.keys();
}

void VBSaveSystem::readFromDisk() {
    QFile f(filePath_);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it)
        if (it.value().isObject())
            profiles_[it.key()] = fromJson(it.value().toObject());
}

void VBSaveSystem::writeToDisk() {
    QJsonObject root;
    for (auto it = profiles_.cbegin(); it != profiles_.cend(); ++it)
        root[it.key()] = toJson(it.value());
    QFile f(filePath_);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}
