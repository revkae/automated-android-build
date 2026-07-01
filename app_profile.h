#pragma once
#include "savedata.h"
#include "vb_data.h"
#include <QJsonArray>
#include <QJsonObject>

struct AppProfileData {
    SaveData      build;
    VBProfileData vb;
};

inline QJsonObject appProfileToJson(const AppProfileData &d) {
    QJsonObject o;
    o["projectDir"]       = d.build.projectDir;
    o["outputDir"]        = d.build.outputDir;
    o["package_"]         = d.build.package_;
    o["mainActivity"]     = d.build.mainActivity;
    o["keyLocation"]      = d.build.keyLocation;
    o["keyAlias"]         = d.build.keyAlias;
    o["keyStorePass"]     = d.build.keyStorePass;
    o["keyPass"]          = d.build.keyPass;
    QJsonArray files;
    for (const auto &f : d.vb.files) {
        QJsonObject fo;
        fo["path"]        = f.path;
        fo["occurrences"] = f.occurrences;
        files.append(fo);
    }
    o["vbFiles"]          = files;
    o["vbSegment"]        = d.vb.segment;
    o["vbNewVersionCode"] = d.vb.newVersionCode;
    return o;
}

inline AppProfileData appProfileFromJson(const QJsonObject &o) {
    AppProfileData d;
    d.build.projectDir   = o["projectDir"].toString();
    d.build.outputDir    = o["outputDir"].toString();
    d.build.package_     = o["package_"].toString();
    d.build.mainActivity = o["mainActivity"].toString();
    d.build.keyLocation  = o["keyLocation"].toString();
    d.build.keyAlias     = o["keyAlias"].toString();
    d.build.keyStorePass = o["keyStorePass"].toString();
    d.build.keyPass      = o["keyPass"].toString();
    d.vb.segment         = o["vbSegment"].toInt(1);
    d.vb.newVersionCode  = o["vbNewVersionCode"].toInt(0);
    for (const auto &v : o["vbFiles"].toArray()) {
        QJsonObject f = v.toObject();
        d.vb.files.append({f["path"].toString(), f["occurrences"].toInt(1)});
    }
    return d;
}
