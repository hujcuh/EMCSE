#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonArray>
#include <QJsonObject>

class QNetworkReply;

class EarthMcApiClient : public QObject
{
    Q_OBJECT

public:
    explicit EarthMcApiClient(QObject* parent = nullptr);

    void queryPlayer(const QString& nameOrUuid);
    void queryTown(const QString& nameOrUuid);
    void queryNation(const QString& nameOrUuid);

    // Existing: batch query players by uuid
    void queryPlayersBatch(const QStringList& uuids);

    // New: batch query towns by name or uuid, optional POST template
    void queryTownsBatch(
        const QStringList& townIdsOrNames,
        const QJsonObject& templ = QJsonObject()
    );

signals:
    void playerQueryFinished(bool ok, const QJsonArray& result, const QString& error);
    void townQueryFinished(bool ok, const QJsonArray& result, const QString& error);
    void nationQueryFinished(bool ok, const QJsonArray& result, const QString& error);

    // Existing
    void playersBatchQueryFinished(bool ok, const QJsonArray& result, const QString& error);

    // New
    void townsBatchQueryFinished(bool ok, const QJsonArray& result, const QString& error);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    // keep old single-item helper
    void postQuery(const QString& endpoint, const QString& queryItem, const QString& queryType);

    // new array helper for batch query + optional template
    void postQueryArray(
        const QString& endpoint,
        const QJsonArray& queryItems,
        const QString& queryType,
        const QJsonObject& templ = QJsonObject()
    );

private:
    QNetworkAccessManager m_manager;
};
