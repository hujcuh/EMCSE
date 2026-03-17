#include "pch.h"
#include "OfflineWarningPlugin.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QDir>
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
    // Nation analysis
    // --------------------------
    if (targetType == "nation") {
        QJsonObject extras = context.value("extras").toObject();
        QJsonArray nationPlayers = extras.value("nationPlayers").toArray();

        if (nationPlayers.isEmpty()) {
            return warnings;
        }

        QJsonArray expiredIds;
        QJsonArray expiredNames;
        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

        for (const QJsonValue& v : nationPlayers) {
            if (!v.isObject()) {
                continue;
            }

            QJsonObject player = v.toObject();
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
                expiredIds.append(player.value("uuid").toString());
                expiredNames.append(player.value("name").toString());
            }
        }

        if (!expiredIds.isEmpty()) {
            QJsonObject w;
            w["severity"] = "critical";
            w["title"] = "Nation inactive members";
            w["message"] = QString("%1 players in this nation have been offline for >= %2 days.")
                .arg(expiredIds.size())
                .arg(thresholdDays);
            w["ids"] = expiredIds;
            w["names"] = expiredNames;
            warnings.append(w);
        }

        return warnings;
    }

    return warnings;
}
