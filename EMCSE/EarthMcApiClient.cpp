#include "stdafx.h"
#include "EarthMcApiClient.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

static const QString BASE_URL = "https://api.earthmc.net/v3/aurora";

EarthMcApiClient::EarthMcApiClient(QObject* parent)
    : QObject(parent)
{
    connect(&m_manager, &QNetworkAccessManager::finished,
        this, &EarthMcApiClient::onReplyFinished);
}

void EarthMcApiClient::queryPlayer(const QString& nameOrUuid)
{
    postQuery("players", nameOrUuid, "player");
}

void EarthMcApiClient::queryTown(const QString& nameOrUuid)
{
    postQuery("towns", nameOrUuid, "town");
}

void EarthMcApiClient::queryNation(const QString& nameOrUuid)
{
    postQuery("nations", nameOrUuid, "nation");
}

void EarthMcApiClient::queryPlayersBatch(const QStringList& uuids)
{
    QJsonArray arr;
    for (const QString& id : uuids) {
        arr.append(id);
    }

    postQueryArray("players", arr, "playersBatch");
}

void EarthMcApiClient::postQuery(const QString& endpoint, const QString& queryItem, const QString& queryType)
{
    QJsonArray arr;
    arr.append(queryItem);
    postQueryArray(endpoint, arr, queryType);
}

void EarthMcApiClient::postQueryArray(const QString& endpoint, const QJsonArray& queryItems, const QString& queryType)
{
    QUrl url(BASE_URL + "/" + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject bodyObj;
    bodyObj["query"] = queryItems;

    QByteArray body = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_manager.post(request, body);
    reply->setProperty("queryType", queryType);
}

void EarthMcApiClient::onReplyFinished(QNetworkReply* reply)
{
    QString queryType = reply->property("queryType").toString();
    QByteArray data = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        if (queryType == "player") {
            emit playerQueryFinished(false, QJsonArray(), reply->errorString());
        }
        else if (queryType == "town") {
            emit townQueryFinished(false, QJsonArray(), reply->errorString());
        }
        else if (queryType == "nation") {
            emit nationQueryFinished(false, QJsonArray(), reply->errorString());
        }
        else if (queryType == "playersBatch") {
            emit playersBatchQueryFinished(false, QJsonArray(), reply->errorString());
        }

        reply->deleteLater();
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        QString err = "JSON parse failed";

        if (queryType == "player") {
            emit playerQueryFinished(false, QJsonArray(), err);
        }
        else if (queryType == "town") {
            emit townQueryFinished(false, QJsonArray(), err);
        }
        else if (queryType == "nation") {
            emit nationQueryFinished(false, QJsonArray(), err);
        }
        else if (queryType == "playersBatch") {
            emit playersBatchQueryFinished(false, QJsonArray(), err);
        }

        reply->deleteLater();
        return;
    }

    if (queryType == "player") {
        emit playerQueryFinished(true, doc.array(), QString());
    }
    else if (queryType == "town") {
        emit townQueryFinished(true, doc.array(), QString());
    }
    else if (queryType == "nation") {
        emit nationQueryFinished(true, doc.array(), QString());
    }
    else if (queryType == "playersBatch") {
        emit playersBatchQueryFinished(true, doc.array(), QString());
    }

    reply->deleteLater();
}
