#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>

class QPluginLoader;
class IAnalysisPlugin;

class PluginManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager();

    bool loadPlugins(const QString& pluginDirPath);

    QJsonArray runPlugins(
        const QString& targetType,
        const QString& queryText,
        const QJsonArray& rawResult,
        const QJsonObject& extras = QJsonObject()
    );

    int pluginCount() const;

signals:
    void logMessage(const QString& text);

private:
    QList<QPluginLoader*> m_loaders;
    QList<IAnalysisPlugin*> m_plugins;
};
