#include "savesystem.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

static QString configPath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/profiles.json";
}

static QJsonObject toJson(const SaveData &d) {
    QJsonObject o;
    o["projectDir"]   = d.projectDir;
    o["outputDir"]    = d.outputDir;
    o["package_"]     = d.package_;
    o["mainActivity"] = d.mainActivity;
    o["keyLocation"]  = d.keyLocation;
    o["keyAlias"]     = d.keyAlias;
    o["keyStorePass"] = d.keyStorePass;
    o["keyPass"]      = d.keyPass;
    return o;
}

static SaveData fromJson(const QJsonObject &o) {
    SaveData d;
    d.projectDir   = o["projectDir"].toString();
    d.outputDir    = o["outputDir"].toString();
    d.package_     = o["package_"].toString();
    d.mainActivity = o["mainActivity"].toString();
    d.keyLocation  = o["keyLocation"].toString();
    d.keyAlias     = o["keyAlias"].toString();
    d.keyStorePass = o["keyStorePass"].toString();
    d.keyPass      = o["keyPass"].toString();
    return d;
}

SaveSystem::SaveSystem() {
    filePath_ = configPath();
    readFromDisk();
}

void SaveSystem::save(const QString &name, const SaveData &data) {
    profiles_[name] = data;
    writeToDisk();
}

SaveData SaveSystem::load(const QString &name) const {
    return profiles_.value(name);
}

void SaveSystem::rename(const QString &oldName, const QString &newName) {
    if (!profiles_.contains(oldName) || oldName == newName) return;
    profiles_[newName] = profiles_.take(oldName);
    writeToDisk();
}

bool SaveSystem::exists(const QString &name) const {
    return profiles_.contains(name);
}

QStringList SaveSystem::profileNames() const {
    return profiles_.keys();
}

void SaveSystem::readFromDisk() {
    QFile f(filePath_);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it)
        if (it.value().isObject())
            profiles_[it.key()] = fromJson(it.value().toObject());
}

void SaveSystem::writeToDisk() {
    QJsonObject root;
    for (auto it = profiles_.cbegin(); it != profiles_.cend(); ++it)
        root[it.key()] = toJson(it.value());
    QFile f(filePath_);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}
