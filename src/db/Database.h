#pragma once
#include <QSqlDatabase>
#include <QString>

class Database {
public:
    static Database& instance();

    bool open(const QString& dbPath = QString());
    void close();
    QSqlDatabase& db();
    QString lastError() const { return m_lastError; }

    bool initialize();

private:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    QSqlDatabase m_db;
    QString m_lastError;
};
