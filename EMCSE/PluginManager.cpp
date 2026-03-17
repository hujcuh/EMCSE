#include "stdafx.h"
#include "PluginManager.h"
#include "IAnalysisPlugin.h"

#include <QDir>
#include <QFileInfoList>
#include <QPluginLoader>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    for (QPluginLoader* loader : m_loaders) {
        if (loader) {
            loader->unload();
        }
    }
}

bool PluginManager::loadPlugins(const QString& pluginDirPath)
{
    QDir dir(pluginDirPath);
    if (!dir.exists()) {
        emit logMessage("Plugin directory does not exist: " + pluginDirPath);
        return false;
    }

    QStringList filters;
    filters << "*.dll";   // Only load DLL files on Windows

    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    if (files.isEmpty()) {
        emit logMessage("No plugin DLL files found in: " + pluginDirPath);
        return true;
    }

    for (const QFileInfo& fi : files) {
        QPluginLoader* loader = new QPluginLoader(fi.absoluteFilePath(), this);

        QObject* obj = loader->instance();
        if (!obj) {
            emit logMessage("Plugin load failed: " + fi.fileName() + " | " + loader->errorString());
            loader->deleteLater();
            continue;
        }

        IAnalysisPlugin* plugin = qobject_cast<IAnalysisPlugin*>(obj);
        if (!plugin) {
            emit logMessage("Invalid analysis plugin: " + fi.fileName());
            loader->unload();
            loader->deleteLater();
            continue;
        }

        emit logMessage(
            "Loaded plugin: "
            + plugin->displayName()
            + " | id=" + plugin->pluginId()
            + " | version=" + plugin->version()
        );

        m_loaders.append(loader);
        m_plugins.append(plugin);
    }

    return true;
}

QJsonArray PluginManager::runPlugins(
    const QString& targetType,
    const QString& queryText,
    const QJsonArray& rawResult,
    const QJsonObject& extras)
{
    QJsonArray allWarnings;

    QJsonObject context;
    context["targetType"] = targetType;
    context["queryText"] = queryText;
    context["rawResult"] = rawResult;
    context["extras"] = extras;

    for (IAnalysisPlugin* plugin : m_plugins) {
        if (!plugin) {
            continue;
        }

        if (!plugin->supports(targetType)) {
            continue;
        }

        QJsonArray warnings = plugin->analyze(context);

        for (const QJsonValue& v : warnings) {
            if (v.isObject()) {
                QJsonObject obj = v.toObject();
                if (!obj.contains("pluginId")) {
                    obj["pluginId"] = plugin->pluginId();
                }
                allWarnings.append(obj);
            }
        }
    }

    return allWarnings;
}

int PluginManager::pluginCount() const
{
    return m_plugins.size();
}
