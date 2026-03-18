#pragma once

#include <QObject>
#include "IAnalysisPlugin.h"

class MayorCheckPlugin : public QObject, public IAnalysisPlugin
{
    Q_OBJECT
        Q_PLUGIN_METADATA(IID IAnalysisPlugin_iid FILE "MayorCheckPlugin.json")
        Q_INTERFACES(IAnalysisPlugin)

public:
    QString pluginId() const override { return "mayor-check"; }
    QString displayName() const override { return "Mayor Check Plugin"; }
    QString version() const override { return "1.0.0"; }

    bool supports(const QString& targetType) const override
    {
        return targetType == "nation";
    }

    QJsonArray analyze(const QJsonObject& context) override;

private:
    int loadOfflineThresholdDays() const;
};
