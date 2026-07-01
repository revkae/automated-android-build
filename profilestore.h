#pragma once
#include <functional>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStandardPaths>
#include <QStringList>

template<typename T>
class ProfileStore {
public:
    using ToJsonFn   = std::function<QJsonObject(const T&)>;
    using FromJsonFn = std::function<T(const QJsonObject&)>;

    ProfileStore(const QString &storageKey, ToJsonFn toJson, FromJsonFn fromJson)
        : toJson_(toJson), fromJson_(fromJson)
    {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(dir);
        filePath_ = dir + "/profiles-" + storageKey + ".json";
        readFromDisk();
    }

    void save(const QString &name, const T &data) {
        profiles_[name] = data;
        writeToDisk();
    }

    T load(const QString &name) const {
        return profiles_.value(name);
    }

    void rename(const QString &oldName, const QString &newName) {
        if (!profiles_.contains(oldName) || oldName == newName) return;
        profiles_[newName] = profiles_.take(oldName);
        writeToDisk();
    }

    bool exists(const QString &name) const {
        return profiles_.contains(name);
    }

    QStringList profileNames() const {
        return profiles_.keys();
    }

private:
    QMap<QString, T> profiles_;
    QString          filePath_;
    ToJsonFn         toJson_;
    FromJsonFn       fromJson_;

    void readFromDisk() {
        QFile f(filePath_);
        if (!f.open(QIODevice::ReadOnly)) return;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) return;
        QJsonObject root = doc.object();
        for (auto it = root.begin(); it != root.end(); ++it)
            if (it.value().isObject())
                profiles_[it.key()] = fromJson_(it.value().toObject());
    }

    void writeToDisk() {
        QJsonObject root;
        for (auto it = profiles_.cbegin(); it != profiles_.cend(); ++it)
            root[it.key()] = toJson_(it.value());
        QFile f(filePath_);
        if (f.open(QIODevice::WriteOnly))
            f.write(QJsonDocument(root).toJson());
    }
};
