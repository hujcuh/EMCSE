#pragma once

#include <QObject>
#include "IAnalysisPlugin.h"

class OfflineWarningPlugin : public QObject, public IAnalysisPlugin
{
    Q_OBJECT
        Q_PLUGIN_METADATA(IID IAnalysisPlugin_iid FILE "OfflineWarningPlugin.json")
        Q_INTERFACES(IAnalysisPlugin)

public:
    QString pluginId() const override { return "offline-warning"; }
    QString displayName() const override { return "Offline Warning Plugin"; }
    QString version() const override { return "1.2.0"; }

    bool supports(const QString& targetType) const override
    {
        return targetType == "player" || targetType == "nation";
    }

    QJsonArray analyze(const QJsonObject& context) override;

private:
    int loadOfflineThresholdDays() const;
};
