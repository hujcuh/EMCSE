#pragma once

#include <QObject>
#include <QSqlDatabase>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject* parent = nullptr);

    bool init(const QString& dbPath);
    bool createTables();

    QString lastError() const;
    QSqlDatabase database() const;

private:
    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;
};
