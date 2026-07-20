#include "DailyDao.h"
#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool DailyDao::saveValue(int entityId, const QString& date, int columnId, double value) {
    QSqlDatabase& db = Database::instance().db();
    db.transaction();
    QSqlQuery q(db);
    // Delete old
    q.exec(QString("DELETE FROM daily_values WHERE entity_id=%1 AND report_date='%2' AND column_id=%3")
        .arg(entityId).arg(date).arg(columnId));
    // Insert new
    bool ok = q.exec(QString("INSERT INTO daily_values (entity_id, report_date, column_id, value) VALUES (%1, '%2', %3, %4)")
        .arg(entityId).arg(date).arg(columnId).arg(value, 0, 'f', 6));
    if (!ok) {
        qWarning() << "saveValue failed:" << q.lastError().text();
        db.rollback();
        return false;
    }
    return db.commit();
}

bool DailyDao::getValue(int entityId, const QString& date, int columnId, DailyValue& out) {
    QSqlQuery q(Database::instance().db());
    q.exec(QString("SELECT * FROM daily_values WHERE entity_id=%1 AND report_date='%2' AND column_id=%3")
        .arg(entityId).arg(date).arg(columnId));
    if (q.next()) {
        out.id = q.value("id").toInt();
        out.entityId = q.value("entity_id").toInt();
        out.reportDate = q.value("report_date").toString();
        out.columnId = q.value("column_id").toInt();
        out.value = q.value("value").toDouble();
        return true;
    }
    return false;
}

bool DailyDao::saveValues(const QVector<DailyValue>& values) {
    if (values.isEmpty()) return true;
    QSqlDatabase& db = Database::instance().db();
    db.transaction();
    QSqlQuery q(db);
    for (const auto& dv : values) {
        // Delete any existing row with same key
        q.exec(QString("DELETE FROM daily_values WHERE entity_id=%1 AND report_date='%2' AND column_id=%3")
            .arg(dv.entityId).arg(dv.reportDate).arg(dv.columnId));
        // Insert new row
        bool ok = q.exec(QString("INSERT INTO daily_values (entity_id, report_date, column_id, value) VALUES (%1, '%2', %3, %4)")
            .arg(dv.entityId).arg(dv.reportDate).arg(dv.columnId).arg(dv.value, 0, 'f', 6));
        if (!ok) {
            qWarning() << "saveValues failed at col" << dv.columnId << ":" << q.lastError().text();
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

QVector<DisplayRow> DailyDao::queryForDisplay(
    const QDate& startDate, const QDate& endDate,
    int entityTypeId, const QVector<int>& columnIds) {

    QVector<DisplayRow> result;

    QString sql = QString(
        "SELECT e.id AS entity_id, e.name AS entity_name, e.parent_id,"
        " et.name AS entity_type, dv.report_date, dv.column_id, dv.value "
        "FROM daily_values dv "
        "JOIN entities e ON dv.entity_id=e.id "
        "JOIN entity_types et ON e.type_id=et.id "
        "WHERE dv.report_date BETWEEN '%1' AND '%2' AND e.is_active=1")
        .arg(startDate.toString("yyyy-MM-dd")).arg(endDate.toString("yyyy-MM-dd"));

    if (entityTypeId > 0)
        sql += QString(" AND e.type_id=%1").arg(entityTypeId);

    if (!columnIds.isEmpty()) {
        QStringList ids;
        for (int id : columnIds) ids << QString::number(id);
        sql += " AND dv.column_id IN (" + ids.join(",") + ")";
    }

    sql += " ORDER BY dv.report_date DESC, et.sort_order, e.parent_id, e.sort_order";

    QSqlQuery q(Database::instance().db());
    if (!q.exec(sql)) {
        qWarning() << "queryForDisplay failed:" << q.lastError().text();
        return result;
    }

    QMap<QString, DisplayRow> rowMap;
    while (q.next()) {
        QString key = QString("%1|%2").arg(q.value("entity_id").toInt()).arg(q.value("report_date").toString());
        if (!rowMap.contains(key)) {
            DisplayRow row;
            row.entityId = q.value("entity_id").toInt();
            row.entityName = q.value("entity_name").toString();
            row.entityType = q.value("entity_type").toString();
            row.parentCompanyId = q.value("parent_id").isNull() ? 0 : q.value("parent_id").toInt();
            row.date = QDate::fromString(q.value("report_date").toString(), "yyyy-MM-dd");
            rowMap[key] = row;
        }
        rowMap[key].values[q.value("column_id").toInt()] = q.value("value").toDouble();
    }

    for (auto it = rowMap.begin(); it != rowMap.end(); ++it)
        result.append(it.value());
    return result;
}

QVector<DisplayRow> DailyDao::queryForChart(
    const QDate& startDate, const QDate& endDate,
    const QVector<int>& entityIds,
    const QVector<int>& columnIds) {

    QVector<DisplayRow> result;

    QStringList eids, cids;
    for (int id : entityIds) eids << QString::number(id);
    for (int id : columnIds) cids << QString::number(id);

    QString sql = QString(
        "SELECT e.id AS entity_id, e.name AS entity_name, e.parent_id,"
        " et.name AS entity_type, dv.report_date, dv.column_id, dv.value "
        "FROM daily_values dv "
        "JOIN entities e ON dv.entity_id=e.id "
        "JOIN entity_types et ON e.type_id=et.id "
        "WHERE dv.report_date BETWEEN '%1' AND '%2' AND e.is_active=1 "
        "AND dv.entity_id IN (%3) AND dv.column_id IN (%4) "
        "ORDER BY dv.report_date, e.sort_order")
        .arg(startDate.toString("yyyy-MM-dd")).arg(endDate.toString("yyyy-MM-dd"))
        .arg(eids.join(",")).arg(cids.join(","));

    QSqlQuery q(Database::instance().db());
    if (!q.exec(sql)) return result;

    QMap<QString, DisplayRow> rowMap;
    while (q.next()) {
        QString key = QString("%1|%2").arg(q.value("entity_id").toInt()).arg(q.value("report_date").toString());
        if (!rowMap.contains(key)) {
            DisplayRow row;
            row.entityId = q.value("entity_id").toInt();
            row.entityName = q.value("entity_name").toString();
            row.entityType = q.value("entity_type").toString();
            row.parentCompanyId = q.value("parent_id").isNull() ? 0 : q.value("parent_id").toInt();
            row.date = QDate::fromString(q.value("report_date").toString(), "yyyy-MM-dd");
            rowMap[key] = row;
        }
        rowMap[key].values[q.value("column_id").toInt()] = q.value("value").toDouble();
    }

    for (auto it = rowMap.begin(); it != rowMap.end(); ++it)
        result.append(it.value());
    return result;
}

bool DailyDao::remove(int entityId, const QString& date) {
    QSqlQuery q(Database::instance().db());
    return q.exec(QString("DELETE FROM daily_values WHERE entity_id=%1 AND report_date='%2'")
        .arg(entityId).arg(date));
}

bool DailyDao::removeRange(int entityId, const QDate& startDate, const QDate& endDate) {
    QSqlQuery q(Database::instance().db());
    return q.exec(QString("DELETE FROM daily_values WHERE entity_id=%1 AND report_date BETWEEN '%2' AND '%3'")
        .arg(entityId).arg(startDate.toString("yyyy-MM-dd")).arg(endDate.toString("yyyy-MM-dd")));
}

QDate DailyDao::getLatestDate() {
    QSqlQuery q(Database::instance().db());
    q.exec("SELECT MAX(report_date) FROM daily_values");
    if (q.next() && !q.value(0).isNull())
        return QDate::fromString(q.value(0).toString(), "yyyy-MM-dd");
    return QDate::currentDate();
}

QDate DailyDao::getEarliestDate() {
    QSqlQuery q(Database::instance().db());
    q.exec("SELECT MIN(report_date) FROM daily_values");
    if (q.next() && !q.value(0).isNull())
        return QDate::fromString(q.value(0).toString(), "yyyy-MM-dd");
    return QDate::currentDate();
}

QVector<QDate> DailyDao::getAvailableDates() {
    QVector<QDate> dates;
    QSqlQuery q(Database::instance().db());
    q.exec("SELECT DISTINCT report_date FROM daily_values ORDER BY report_date DESC");
    while (q.next())
        dates.append(QDate::fromString(q.value(0).toString(), "yyyy-MM-dd"));
    return dates;
}
