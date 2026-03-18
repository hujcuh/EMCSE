#pragma once

#include <QtWidgets/QMainWindow>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include "ui_EMCSE.h"

class DatabaseManager;
class EarthMcApiClient;
class PluginManager;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class EMCSE : public QMainWindow
{
    Q_OBJECT

public:
    EMCSE(QWidget* parent = nullptr);
    ~EMCSE();

private slots:
    void onSearchClicked();
    void onPlayerQueryFinished(bool ok, const QJsonArray& result, const QString& error);
    void onTownQueryFinished(bool ok, const QJsonArray& result, const QString& error);
    void onNationQueryFinished(bool ok, const QJsonArray& result, const QString& error);

    // Shared batch callback for both Town-mayor and Nation-mayors
    void onPlayersBatchQueryFinished(bool ok, const QJsonArray& result, const QString& error);

    // Nation -> Towns -> MayorPlayers pipeline
    void onTownsBatchQueryFinished(bool ok, const QJsonArray& result, const QString& error);

private:
    void appendLog(const QString& text);
    void showPluginWarnings(const QJsonArray& warnings);

    // Formatting helpers
    QString formatJsonValue(const QString& key, const QJsonValue& value) const;
    bool isTimestampKey(const QString& key) const;
    QString formatTimestamp(const QJsonValue& value) const;
    bool isPermissionArrayKey(const QString& key) const;
    QString permissionLabelForIndex(int index) const;
    bool isBoolArray(const QJsonArray& arr) const;

    // Tree view helpers
    void showJsonArrayInTree(const QJsonArray& arr);
    void addJsonValueToTree(QTreeWidgetItem* parentItem, const QString& key, const QJsonValue& value);
    void addJsonObjectToTree(QTreeWidgetItem* parentItem, const QJsonObject& obj);
    void addJsonArrayToTree(QTreeWidgetItem* parentItem, const QString& key, const QJsonArray& arr);

private:
    Ui::EMCSEClass ui;

    DatabaseManager* m_dbManager = nullptr;
    EarthMcApiClient* m_apiClient = nullptr;
    PluginManager* m_pluginManager = nullptr;

    QComboBox* m_comboType = nullptr;
    QLineEdit* m_editQuery = nullptr;
    QPushButton* m_btnSearch = nullptr;
    QTreeWidget* m_resultTree = nullptr;

    // -------------------------
    // Town mayor analysis state
    // -------------------------
    QJsonArray m_pendingTownResult;
    QString m_pendingTownQueryText;
    bool m_waitingTownMayorBatch = false;

    // -------------------------
    // Nation mayor analysis state
    // -------------------------
    QJsonArray m_pendingNationResult;
    QString m_pendingNationQueryText;
    QJsonArray m_pendingNationTownDetails;

    bool m_waitingNationTownBatch = false;
    bool m_waitingNationMayorPlayersBatch = false;
};
