#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

class IAnalysisPlugin
{
public:
    virtual ~IAnalysisPlugin() = default;

    virtual QString pluginId() const = 0;
    virtual QString displayName() const = 0;
    virtual QString version() const = 0;

    // targetType: "player" / "town" / "nation"
    virtual bool supports(const QString& targetType) const = 0;

    // context:
    // {
    //   "targetType": "player",
    //   "queryText": "leaf_h",
    //   "rawResult": [...],
    //   "extras": {...}
    // }
    //
    // return warnings:
    // [
    //   {
    //     "severity": "info|warning|critical",
    //     "title": "...",
    //     "message": "...",
    //     "targetId": "...",
    //     "pluginId": "..."
    //   }
    // ]
    virtual QJsonArray analyze(const QJsonObject& context) = 0;
};

#define IAnalysisPlugin_iid "com.emcse.IAnalysisPlugin/1.0"
Q_DECLARE_INTERFACE(IAnalysisPlugin, IAnalysisPlugin_iid)
