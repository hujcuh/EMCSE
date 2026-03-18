#include "pch.h"
#include "MayorCheckPlugin.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>

int MayorCheckPlugin::loadOfflineThresholdDays() const
{
    const int defaultDays = 40;

    QString configPath =
        QCoreApplication::applicationDirPath()
        + "/plugins/MayorCheckPlugin.config.json";

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return defaultDays;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return defaultDays;
    }

    QJsonObject obj = doc.object();
    if (!obj.contains("offlineThresholdDays") || !obj.value("offlineThresholdDays").isDouble()) {
        return defaultDays;
    }

    int days = obj.value("offlineThresholdDays").toInt(defaultDays);
    if (days < 0) {
        return defaultDays;
    }

    return days;
}

QJsonArray MayorCheckPlugin::analyze(const QJsonObject& context)
{
    QJsonArray warnings;

    QString targetType = context.value("targetType").toString();
    if (targetType != "nation") {
        return warnings;
    }

    QJsonObject extras = context.value("extras").toObject();
    QJsonArray nationTowns = extras.value("nationTowns").toArray();
    QJsonArray nationMayorPlayers = extras.value("nationMayorPlayers").toArray();

    if (nationTowns.isEmpty() || nationMayorPlayers.isEmpty()) {
        return warnings;
    }

    const int thresholdDays = loadOfflineThresholdDays();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    // Build player uuid -> player object map
    QHash<QString, QJsonObject> playerMap;
    for (const QJsonValue& v : nationMayorPlayers) {
        if (!v.isObject()) {
            continue;
        }

        QJsonObject player = v.toObject();
        QString uuid = player.value("uuid").toString();
        if (!uuid.isEmpty()) {
            playerMap.insert(uuid, player);
        }
    }

    QJsonArray expiredIds;
    QJsonArray expiredNames;

    for (const QJsonValue& v : nationTowns) {
        if (!v.isObject()) {
            continue;
        }

        QJsonObject town = v.toObject();
        QString townName = town.value("name").toString();

        if (!town.contains("mayor") || !town.value("mayor").isObject()) {
            continue;
        }

        QJsonObject mayorObj = town.value("mayor").toObject();
        QString mayorUuid = mayorObj.value("uuid").toString();
        QString mayorName = mayorObj.value("name").toString();

        if (mayorUuid.isEmpty() || !playerMap.contains(mayorUuid)) {
            continue;
        }

        QJsonObject player = playerMap.value(mayorUuid);
        if (!player.contains("timestamps") || !player.value("timestamps").isObject()) {
            continue;
        }

        QJsonObject timestamps = player.value("timestamps").toObject();
        if (!timestamps.contains("lastOnline") || !timestamps.value("lastOnline").isDouble()) {
            continue;
        }

        qint64 lastOnlineMs = static_cast<qint64>(timestamps.value("lastOnline").toDouble());
        qint64 days = (nowMs - lastOnlineMs) / (1000LL * 60 * 60 * 24);

        if (days >= thresholdDays) {
            expiredIds.append(mayorUuid);
            expiredNames.append(QStringLiteral("%1 | %2 | offline %3 days")
                .arg(townName)
                .arg(mayorName)
                .arg(days));
        }
    }

    if (!expiredNames.isEmpty()) {
        QJsonObject w;
        w["severity"] = "critical";
        w["title"] = QStringLiteral("Nation inactive mayors");
        w["message"] = QStringLiteral("%1 mayors exceed offline threshold (%2 days).")
            .arg(expiredNames.size())
            .arg(thresholdDays);
        w["ids"] = expiredIds;
        w["names"] = expiredNames;
        warnings.append(w);
    }

    return warnings;
}
