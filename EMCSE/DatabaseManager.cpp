#include "stdafx.h"
#include "DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QUuid>

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
{
    m_connectionName = "main_connection_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool DatabaseManager::init(const QString& dbPath)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qDebug() << "Open database failed:" << m_lastError;
        return false;
    }

    return createTables();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_db);

    QString sql1 = R"(
        CREATE TABLE IF NOT EXISTS tracked_targets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            type TEXT NOT NULL,
            uuid TEXT NOT NULL UNIQUE,
            name TEXT,
            enabled INTEGER NOT NULL DEFAULT 1,
            last_json TEXT,
            last_checked_at TEXT,
            created_at TEXT,
            updated_at TEXT
        )
    )";

    if (!query.exec(sql1)) {
        m_lastError = query.lastError().text();
        qDebug() << "Create tracked_targets failed:" << m_lastError;
        return false;
    }

    QString sql2 = R"(
        CREATE TABLE IF NOT EXISTS change_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            target_id INTEGER NOT NULL,
            changed_at TEXT NOT NULL,
            summary TEXT,
            old_json TEXT,
            new_json TEXT
        )
    )";

    if (!query.exec(sql2)) {
        m_lastError = query.lastError().text();
        qDebug() << "Create change_logs failed:" << m_lastError;
        return false;
    }

    return true;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

QSqlDatabase DatabaseManager::database() const
{
    return m_db;
}
