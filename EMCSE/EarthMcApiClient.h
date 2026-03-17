#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonArray>

class QNetworkReply;

class EarthMcApiClient : public QObject
{
    Q_OBJECT

public:
    explicit EarthMcApiClient(QObject* parent = nullptr);

    void queryPlayer(const QString& nameOrUuid);
    void queryTown(const QString& nameOrUuid);
    void queryNation(const QString& nameOrUuid);

    // New: batch query players by uuid
    void queryPlayersBatch(const QStringList& uuids);

signals:
    void playerQueryFinished(bool ok, const QJsonArray& result, const QString& error);
    void townQueryFinished(bool ok, const QJsonArray& result, const QString& error);
    void nationQueryFinished(bool ok, const QJsonArray& result, const QString& error);

    // New
    void playersBatchQueryFinished(bool ok, const QJsonArray& result, const QString& error);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    // keep old single-item helper
    void postQuery(const QString& endpoint, const QString& queryItem, const QString& queryType);

    // new array helper for batch query
    void postQueryArray(const QString& endpoint, const QJsonArray& queryItems, const QString& queryType);

private:
    QNetworkAccessManager m_manager;
};
