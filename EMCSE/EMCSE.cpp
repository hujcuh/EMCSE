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
#include <QSet>
#include <QFont>

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

    // 强制使用支持中文的字体，避免日志中的中文显示为方框
    QFont logFont;
    logFont.setFamilies(QStringList()
        << "Microsoft YaHei UI"
        << "Microsoft YaHei"
        << "SimSun"
        << "Segoe UI");
    logFont.setPointSize(10);

    ui.textLog->setFont(logFont);
    m_resultTree->setFont(logFont);

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

    // UI connections
    connect(m_btnSearch, &QPushButton::clicked,
        this, &EMCSE::onSearchClicked);

    // API result connections
    connect(m_apiClient, &EarthMcApiClient::playerQueryFinished,
        this, &EMCSE::onPlayerQueryFinished);

    connect(m_apiClient, &EarthMcApiClient::townQueryFinished,
        this, &EMCSE::onTownQueryFinished);

    connect(m_apiClient, &EarthMcApiClient::nationQueryFinished,
        this, &EMCSE::onNationQueryFinished);

    connect(m_apiClient, &EarthMcApiClient::playersBatchQueryFinished,
        this, &EMCSE::onPlayersBatchQueryFinished);

    connect(m_apiClient, &EarthMcApiClient::townsBatchQueryFinished,
        this, &EMCSE::onTownsBatchQueryFinished);
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

    // Reset nation mayor pipeline state
    m_waitingNationTownBatch = false;
    m_waitingNationMayorPlayersBatch = false;
    m_pendingNationResult = QJsonArray();
    m_pendingNationQueryText.clear();
    m_pendingNationTownDetails = QJsonArray();

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
    if (!ok) {
        m_btnSearch->setEnabled(true);
        appendLog("Nation query failed: " + error);
        return;
    }

    appendLog(QString("Nation query success, result count = %1").arg(result.size()));
    showJsonArrayInTree(result);

    if (result.isEmpty() || !result.at(0).isObject()) {
        m_btnSearch->setEnabled(true);
        appendLog("Nation result is empty or invalid.");
        return;
    }

    QJsonObject nation = result.at(0).toObject();
    if (!nation.contains("towns") || !nation.value("towns").isArray()) {
        m_btnSearch->setEnabled(true);
        appendLog("Nation has no towns array.");
        return;
    }

    QJsonArray towns = nation.value("towns").toArray();
    QStringList townIds;

    for (const QJsonValue& v : towns) {
        if (!v.isObject()) {
            continue;
        }

        QJsonObject townObj = v.toObject();

        // 优先用 uuid，拿不到再退回 name
        QString id = townObj.value("uuid").toString();
        if (id.isEmpty()) {
            id = townObj.value("name").toString();
        }

        if (!id.isEmpty()) {
            townIds.append(id);
        }
    }

    appendLog(QString("Nation towns to batch query: %1").arg(townIds.size()));

    if (townIds.isEmpty()) {
        m_btnSearch->setEnabled(true);
        appendLog("No town IDs found in nation.");
        return;
    }

    m_pendingNationResult = result;
    m_pendingNationQueryText = m_editQuery->text().trimmed();
    m_waitingNationTownBatch = true;

    QJsonObject templ;
    templ["name"] = true;
    templ["mayor"] = true;
    templ["nation"] = true;

    m_apiClient->queryTownsBatch(townIds, templ);
}

void EMCSE::onTownsBatchQueryFinished(bool ok, const QJsonArray& result, const QString& error)
{
    // 这里只处理 Nation -> Towns 的 mayor 流程
    if (!m_waitingNationTownBatch) {
        appendLog("Received towns batch result, but no nation town batch was pending.");
        return;
    }

    m_waitingNationTownBatch = false;

    if (!ok) {
        m_btnSearch->setEnabled(true);
        appendLog("Nation town batch query failed: " + error);
        return;
    }

    appendLog(QString("Nation town batch query success, result count = %1").arg(result.size()));

    m_pendingNationTownDetails = result;

    QSet<QString> mayorSet;
    QStringList mayorUuids;

    for (const QJsonValue& v : result) {
        if (!v.isObject()) {
            continue;
        }

        QJsonObject townObj = v.toObject();
        if (!townObj.contains("mayor") || !townObj.value("mayor").isObject()) {
            continue;
        }

        QJsonObject mayorObj = townObj.value("mayor").toObject();
        QString mayorUuid = mayorObj.value("uuid").toString();

        if (!mayorUuid.isEmpty() && !mayorSet.contains(mayorUuid)) {
            mayorSet.insert(mayorUuid);
            mayorUuids.append(mayorUuid);
        }
    }

    appendLog(QString("Nation mayor UUIDs to batch query: %1").arg(mayorUuids.size()));

    if (mayorUuids.isEmpty()) {
        m_btnSearch->setEnabled(true);
        appendLog("No mayor UUIDs found in nation towns.");
        return;
    }

    m_waitingNationMayorPlayersBatch = true;
    m_apiClient->queryPlayersBatch(mayorUuids);
}

void EMCSE::onPlayersBatchQueryFinished(bool ok, const QJsonArray& result, const QString& error)
{
    if (!m_waitingNationMayorPlayersBatch) {
        appendLog("Received players batch result, but no nation mayor batch was pending.");
        return;
    }

    m_waitingNationMayorPlayersBatch = false;
    m_btnSearch->setEnabled(true);

    if (!ok) {
        appendLog("Nation mayor players batch query failed: " + error);
        return;
    }

    appendLog(QString("Nation mayor players batch query success, result count = %1").arg(result.size()));

    if (m_pluginManager) {
        QJsonObject extras;
        extras["nationTowns"] = m_pendingNationTownDetails;
        extras["nationMayorPlayers"] = result;

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
    m_pendingNationTownDetails = QJsonArray();
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

        if (obj.contains("ids") && obj.value("ids").isArray()) {
            QJsonArray ids = obj.value("ids").toArray();
            appendLog(QString("Expired member UUIDs (%1):").arg(ids.size()));
            for (const QJsonValue& idVal : ids) {
                appendLog(" - " + idVal.toString());
            }
        }

        if (obj.contains("names") && obj.value("names").isArray()) {
            QJsonArray names = obj.value("names").toArray();
            appendLog(QString("Expired member names (%1):").arg(names.size()));
            for (const QJsonValue& nameVal : names) {
                appendLog(" - " + nameVal.toString());
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
