#include "pch.h"
#include "OfflineWarningPlugin.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

int OfflineWarningPlugin::loadOfflineThresholdDays() const
{
    const int defaultDays = 40;

    QString configPath =
        QCoreApplication::applicationDirPath()
        + "/plugins/OfflineWarningPlugin.config.json";

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

QJsonArray OfflineWarningPlugin::analyze(const QJsonObject& context)
{
    QJsonArray warnings;
    QString targetType = context.value("targetType").toString();
    int thresholdDays = loadOfflineThresholdDays();

    // --------------------------
    // Player analysis
    // --------------------------
    if (targetType == "player") {
        QJsonArray rawResult = context.value("rawResult").toArray();
        if (rawResult.isEmpty() || !rawResult.at(0).isObject()) {
            return warnings;
        }

        QJsonObject player = rawResult.at(0).toObject();
        QString playerName = player.value("name").toString();

        if (!player.contains("timestamps") || !player.value("timestamps").isObject()) {
            return warnings;
        }

        QJsonObject timestamps = player.value("timestamps").toObject();
        if (!timestamps.contains("lastOnline") || !timestamps.value("lastOnline").isDouble()) {
            return warnings;
        }

        qint64 lastOnlineMs = static_cast<qint64>(timestamps.value("lastOnline").toDouble());
        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        qint64 days = (nowMs - lastOnlineMs) / (1000LL * 60 * 60 * 24);

        if (days >= thresholdDays) {
            QJsonObject w;
            w["severity"] = "critical";
            w["title"] = "Player inactive";
            w["message"] = QString("%1 has been offline for %2 days (threshold: %3).")
                .arg(playerName)
                .arg(days)
                .arg(thresholdDays);
            w["targetId"] = player.value("uuid").toString();
            warnings.append(w);
        }

        return warnings;
    }

    // --------------------------
    // Town analysis
    // Requires host extras["townMayorPlayer"]
    // --------------------------
    if (targetType == "town") {
        QJsonArray rawResult = context.value("rawResult").toArray();
        if (rawResult.isEmpty() || !rawResult.at(0).isObject()) {
            return warnings;
        }

        QJsonObject town = rawResult.at(0).toObject();
        QString townName = town.value("name").toString();

        if (!town.contains("mayor") || !town.value("mayor").isObject()) {
            return warnings;
        }

        QJsonObject mayorObj = town.value("mayor").toObject();
        QString mayorName = mayorObj.value("name").toString();
        QString mayorUuid = mayorObj.value("uuid").toString();

        QJsonObject extras = context.value("extras").toObject();
        if (!extras.contains("townMayorPlayer") || !extras.value("townMayorPlayer").isObject()) {
            // 如果宿主程序没传 mayor 对应的 player 详情，则不做 town 警告
            return warnings;
        }

        QJsonObject mayorPlayer = extras.value("townMayorPlayer").toObject();
        if (!mayorPlayer.contains("timestamps") || !mayorPlayer.value("timestamps").isObject()) {
            return warnings;
        }

        QJsonObject timestamps = mayorPlayer.value("timestamps").toObject();
        if (!timestamps.contains("lastOnline") || !timestamps.value("lastOnline").isDouble()) {
            return warnings;
        }

        qint64 lastOnlineMs = static_cast<qint64>(timestamps.value("lastOnline").toDouble());
        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        qint64 days = (nowMs - lastOnlineMs) / (1000LL * 60 * 60 * 24);

        if (days >= thresholdDays) {
            QJsonObject w;
            w["severity"] = "critical";
            w["title"] = "Town mayor inactive";
            w["message"] = QString("%1 mayor %2 has been offline for %3 days (threshold: %4).")
                .arg(townName)
                .arg(mayorName)
                .arg(days)
                .arg(thresholdDays);
            w["targetId"] = mayorUuid;

            QJsonArray names;
            names.append(QString("%1 | %2 | offline %3 days")
                .arg(townName)
                .arg(mayorName)
                .arg(days));
            w["names"] = names;

            warnings.append(w);
        }

        return warnings;
    }

    return warnings;
}
