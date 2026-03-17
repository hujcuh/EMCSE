#include "stdafx.h"
#include "EMCSE.h"

#include "DatabaseManager.h"
#include "EarthMcApiClient.h"
#include "PluginManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QSqlDatabase>
#include <QDebug>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

EMCSE::EMCSE(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    // Central layout
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    if (!layout) {
        layout = new QVBoxLayout();
        layout->setContentsMargins(9, 9, 9, 9);
        centralWidget()->setLayout(layout);
    }

    // Search bar
    QWidget* searchBar = new QWidget(this);
    QHBoxLayout* searchLayout = new QHBoxLayout(searchBar);
    searchLayout->setContentsMargins(0, 0, 0, 0);

    m_comboType = new QComboBox(this);
    m_comboType->addItem("Player");
    m_comboType->addItem("Town");
    m_comboType->addItem("Nation");

    m_editQuery = new QLineEdit(this);
    m_editQuery->setPlaceholderText("Enter player, town, or nation name");

    m_btnSearch = new QPushButton("Search", this);

    searchLayout->addWidget(m_comboType);
    searchLayout->addWidget(m_editQuery);
    searchLayout->addWidget(m_btnSearch);

    layout->insertWidget(0, searchBar);

    // Result tree
    m_resultTree = new QTreeWidget(this);
    m_resultTree->setColumnCount(2);
    m_resultTree->setHeaderLabels(QStringList() << "Key" << "Value");
    m_resultTree->header()->setStretchLastSection(true);
    m_resultTree->setRootIsDecorated(true);
    m_resultTree->setAlternatingRowColors(true);

    layout->addWidget(m_resultTree, 1);

    // Log box
    if (layout->indexOf(ui.textLog) == -1) {
        layout->addWidget(ui.textLog);
    }

    m_resultTree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui.textLog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui.textLog->setMaximumHeight(180);

    // Services
    m_dbManager = new DatabaseManager(this);
    m_apiClient = new EarthMcApiClient(this);
    m_pluginManager = new PluginManager(this);

    connect(m_pluginManager, &PluginManager::logMessage,
        this, &EMCSE::appendLog);

    // Database init
    QString dataDir = QCoreApplication::applicationDirPath() + "/data";
    QDir().mkpath(dataDir);

    QString dbPath = dataDir + "/earthmc_tracker.db";

    appendLog("App started");
    appendLog("Database path: " + dbPath);
    appendLog("Available SQL drivers: " + QSqlDatabase::drivers().join(", "));

    if (m_dbManager->init(dbPath)) {
        appendLog("Database initialized successfully");
        statusBar()->showMessage("Database initialized successfully");
    }
    else {
        appendLog("Database initialization failed: " + m_dbManager->lastError());
        statusBar()->showMessage("Database initialization failed");
    }

    // Plugin loading
    QString pluginDir = QCoreApplication::applicationDirPath() + "/plugins";
    QDir().mkpath(pluginDir);
    appendLog("Plugin directory: " + pluginDir);
    m_pluginManager->loadPlugins(pluginDir);
    appendLog(QString("Plugin count: %1").arg(m_pluginManager->pluginCount()));

    connect(m_btnSearch, &QPushButton::clicked,
        this, &EMCSE::onSearchClicked);

    connect(m_apiClient, &EarthMcApiClient::playerQueryFinished,
        this, &EMCSE::onPlayerQueryFinished);

    connect(m_apiClient, &EarthMcApiClient::townQueryFinished,
        this, &EMCSE::onTownQueryFinished);

    connect(m_apiClient, &EarthMcApiClient::nationQueryFinished,
        this, &EMCSE::onNationQueryFinished);

    // Nation resident batch callback
    connect(m_apiClient, &EarthMcApiClient::playersBatchQueryFinished,
        this, &EMCSE::onPlayersBatchQueryFinished);
}

EMCSE::~EMCSE()
{
}

void EMCSE::onSearchClicked()
{
    QString input = m_editQuery->text().trimmed();

    if (input.isEmpty()) {
        appendLog("Query is empty");
        return;
    }

    m_btnSearch->setEnabled(false);
    m_resultTree->clear();

    // Reset nation batch state
    m_waitingNationBatch = false;
    m_pendingNationResult = QJsonArray();
    m_pendingNationQueryText.clear();

    QString currentType = m_comboType->currentText();

    if (currentType == "Player") {
        appendLog("Querying player: " + input);
        m_apiClient->queryPlayer(input);
    }
    else if (currentType == "Town") {
        appendLog("Querying town: " + input);
        m_apiClient->queryTown(input);
    }
    else if (currentType == "Nation") {
        appendLog("Querying nation: " + input);
        m_apiClient->queryNation(input);
    }
}

void EMCSE::onPlayerQueryFinished(bool ok, const QJsonArray& result, const QString& error)
{
    m_btnSearch->setEnabled(true);

    if (!ok) {
        appendLog("Player query failed: " + error);
        return;
    }

    appendLog(QString("Player query success, result count = %1").arg(result.size()));
    showJsonArrayInTree(result);

    if (m_pluginManager) {
        QJsonArray warnings = m_pluginManager->runPlugins(
            "player",
            m_editQuery->text().trimmed(),
            result
        );
        showPluginWarnings(warnings);
    }
}

void EMCSE::onTownQueryFinished(bool ok, const QJsonArray& result, const QString& error)
{
    m_btnSearch->setEnabled(true);

    if (!ok) {
        appendLog("Town query failed: " + error);
        return;
    }

    appendLog(QString("Town query success, result count = %1").arg(result.size()));
    showJsonArrayInTree(result);

    if (m_pluginManager) {
        QJsonArray warnings = m_pluginManager->runPlugins(
            "town",
            m_editQuery->text().trimmed(),
            result
        );
        showPluginWarnings(warnings);
    }
}

void EMCSE::onNationQueryFinished(bool ok, const QJsonArray& result, const QString& error)
{
    m_btnSearch->setEnabled(true);

    if (!ok) {
        appendLog("Nation query failed: " + error);
        return;
    }

    appendLog(QString("Nation query success, result count = %1").arg(result.size()));
    showJsonArrayInTree(result);

    if (result.isEmpty() || !result.at(0).isObject()) {
        appendLog("Nation result is empty or invalid.");
        return;
    }

    QJsonObject nation = result.at(0).toObject();
    if (!nation.contains("residents") || !nation.value("residents").isArray()) {
        appendLog("Nation has no residents array.");
        return;
    }

    QJsonArray residents = nation.value("residents").toArray();
    QStringList residentUuids;

    for (const QJsonValue& v : residents) {
        if (!v.isObject()) {
            continue;
        }

        QJsonObject residentObj = v.toObject();
        QString uuid = residentObj.value("uuid").toString();
        if (!uuid.isEmpty()) {
            residentUuids.append(uuid);
        }
    }

    appendLog(QString("Nation residents to batch query: %1").arg(residentUuids.size()));

    if (residentUuids.isEmpty()) {
        appendLog("No resident UUIDs found in nation.");
        return;
    }

    m_pendingNationResult = result;
    m_pendingNationQueryText = m_editQuery->text().trimmed();
    m_waitingNationBatch = true;

    m_apiClient->queryPlayersBatch(residentUuids);
}

void EMCSE::onPlayersBatchQueryFinished(bool ok, const QJsonArray& result, const QString& error)
{
    if (!m_waitingNationBatch) {
        appendLog("Received players batch result, but no nation batch was pending.");
        return;
    }

    m_waitingNationBatch = false;

    if (!ok) {
        appendLog("Nation resident batch query failed: " + error);
        return;
    }

    appendLog(QString("Nation resident batch query success, result count = %1").arg(result.size()));

    if (m_pluginManager) {
        QJsonObject extras;
        extras["nationPlayers"] = result;

        QJsonArray warnings = m_pluginManager->runPlugins(
            "nation",
            m_pendingNationQueryText,
            m_pendingNationResult,
            extras
        );

        showPluginWarnings(warnings);
    }

    m_pendingNationResult = QJsonArray();
    m_pendingNationQueryText.clear();
}

void EMCSE::showPluginWarnings(const QJsonArray& warnings)
{
    if (warnings.isEmpty()) {
        appendLog("No plugin warnings.");
        return;
    }

    appendLog(QString("Plugin warnings: %1").arg(warnings.size()));

    for (const QJsonValue& v : warnings) {
        if (!v.isObject()) {
            continue;
        }

        QJsonObject obj = v.toObject();
        QString severity = obj.value("severity").toString("info");
        QString title = obj.value("title").toString();
        QString message = obj.value("message").toString();
        QString pluginId = obj.value("pluginId").toString();

        appendLog(
            QString("[PLUGIN][%1][%2] %3 - %4")
            .arg(severity.toUpper())
            .arg(pluginId)
            .arg(title)
            .arg(message)
        );

        // If plugin returned expired member ids, list them in log
        if (obj.contains("ids") && obj.value("ids").isArray()) {
            QJsonArray ids = obj.value("ids").toArray();
            appendLog(QString("Expired member UUIDs (%1):").arg(ids.size()));
            for (const QJsonValue& idVal : ids) {
                appendLog("  - " + idVal.toString());
            }
        }

        // Optional: also list names if plugin returned them
        if (obj.contains("names") && obj.value("names").isArray()) {
            QJsonArray names = obj.value("names").toArray();
            appendLog(QString("Expired member names (%1):").arg(names.size()));
            for (const QJsonValue& nameVal : names) {
                appendLog("  - " + nameVal.toString());
            }
        }
    }
}

void EMCSE::appendLog(const QString& text)
{
    QString line = QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
        .arg(text);

    ui.textLog->appendPlainText(line);
    qDebug() << line;
}

// =========================
// Formatting helpers
// =========================

bool EMCSE::isTimestampKey(const QString& key) const
{
    return key == "registered"
        || key == "joinedTownAt"
        || key == "joinedNationAt"
        || key == "ruinedAt"
        || key == "lastOnline";
}

QString EMCSE::formatTimestamp(const QJsonValue& value) const
{
    if (!value.isDouble()) {
        return "null";
    }

    qint64 ms = static_cast<qint64>(value.toDouble());
    if (ms <= 0) {
        return "null";
    }

    QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
    return dt.toString("yyyy-MM-dd HH:mm:ss");
}

bool EMCSE::isPermissionArrayKey(const QString& key) const
{
    return key == "build"
        || key == "destroy"
        || key == "switch"
        || key == "itemUse";
}

QString EMCSE::permissionLabelForIndex(int index) const
{
    switch (index) {
    case 0: return "resident";
    case 1: return "nation";
    case 2: return "ally";
    case 3: return "outsider";
    default: return QString("[%1]").arg(index);
    }
}

bool EMCSE::isBoolArray(const QJsonArray& arr) const
{
    for (int i = 0; i < arr.size(); ++i) {
        if (!arr.at(i).isBool()) {
            return false;
        }
    }
    return true;
}

QString EMCSE::formatJsonValue(const QString& key, const QJsonValue& value) const
{
    if (isTimestampKey(key) && value.isDouble()) {
        return formatTimestamp(value);
    }

    if (value.isString()) {
        return value.toString();
    }

    if (value.isDouble()) {
        qint64 intValue = static_cast<qint64>(value.toDouble());
        if (value.toDouble() == static_cast<double>(intValue)) {
            return QString::number(intValue);
        }
        return QString::number(value.toDouble(), 'f', 6);
    }

    if (value.isBool()) {
        return value.toBool() ? "true" : "false";
    }

    if (value.isNull()) {
        return "null";
    }

    if (value.isArray()) {
        QJsonArray arr = value.toArray();
        return arr.isEmpty() ? "[]" : QString("[size=%1]").arg(arr.size());
    }

    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        return obj.isEmpty() ? "{}" : "";
    }

    return "undefined";
}

// =========================
// Tree rendering
// =========================

void EMCSE::showJsonArrayInTree(const QJsonArray& arr)
{
    if (!m_resultTree) {
        return;
    }

    m_resultTree->clear();

    if (arr.size() == 1 && arr.at(0).isObject()) {
        addJsonObjectToTree(m_resultTree->invisibleRootItem(), arr.at(0).toObject());
        m_resultTree->expandAll();
        return;
    }

    for (int i = 0; i < arr.size(); ++i) {
        QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_resultTree);
        rootItem->setText(0, QString("[%1]").arg(i));

        QJsonValue value = arr.at(i);

        if (value.isObject()) {
            QJsonObject obj = value.toObject();
            rootItem->setText(1, obj.isEmpty() ? "{}" : "");
            addJsonObjectToTree(rootItem, obj);
        }
        else if (value.isArray()) {
            QJsonArray subArr = value.toArray();
            rootItem->setText(1, subArr.isEmpty() ? "[]" : QString("[size=%1]").arg(subArr.size()));
            addJsonArrayToTree(rootItem, QString("[%1]").arg(i), subArr);
        }
        else {
            rootItem->setText(1, formatJsonValue(QString(), value));
        }
    }

    m_resultTree->expandAll();
}

void EMCSE::addJsonObjectToTree(QTreeWidgetItem* parentItem, const QJsonObject& obj)
{
    const QStringList keys = obj.keys();

    for (const QString& key : keys) {
        addJsonValueToTree(parentItem, key, obj.value(key));
    }
}

void EMCSE::addJsonArrayToTree(QTreeWidgetItem* parentItem, const QString& key, const QJsonArray& arr)
{
    if (arr.isEmpty()) {
        return;
    }

    if (isPermissionArrayKey(key) && arr.size() == 4 && isBoolArray(arr)) {
        for (int i = 0; i < arr.size(); ++i) {
            QTreeWidgetItem* item = new QTreeWidgetItem(parentItem);
            item->setText(0, permissionLabelForIndex(i));
            item->setText(1, arr.at(i).toBool() ? "true" : "false");
        }
        return;
    }

    for (int i = 0; i < arr.size(); ++i) {
        QString childKey = QString("[%1]").arg(i);
        addJsonValueToTree(parentItem, childKey, arr.at(i));
    }
}

void EMCSE::addJsonValueToTree(QTreeWidgetItem* parentItem, const QString& key, const QJsonValue& value)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(parentItem);
    item->setText(0, key);

    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        item->setText(1, obj.isEmpty() ? "{}" : "");
        addJsonObjectToTree(item, obj);
        return;
    }

    if (value.isArray()) {
        QJsonArray arr = value.toArray();
        item->setText(1, arr.isEmpty() ? "[]" : QString("[size=%1]").arg(arr.size()));
        addJsonArrayToTree(item, key, arr);
        return;
    }

    item->setText(1, formatJsonValue(key, value));
}
