#include "DailyDao.h"
#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QString placeholders(int n) { QStringList l; for(int i=0;i<n;++i) l<<"?"; return l.join(","); }

// ---------------------------------------------------------------------------
bool DailyDao::saveValue(int entityId, const QString& date, int columnId, double value) {
    QSqlDatabase& db = Database::instance().db();
    db.transaction();
    QSqlQuery q(db);
    q.prepare("DELETE FROM daily_values WHERE entity_id=? AND report_date=? AND column_id=?");
    q.addBindValue(entityId); q.addBindValue(date); q.addBindValue(columnId);
    if(!q.exec()){db.rollback();return false;}
    q.prepare("INSERT INTO daily_values (entity_id,report_date,column_id,value) VALUES (?,?,?,?)");
    q.addBindValue(entityId); q.addBindValue(date); q.addBindValue(columnId); q.addBindValue(value);
    if(!q.exec()){db.rollback();return false;}
    return db.commit();
}

bool DailyDao::getValue(int entityId, const QString& date, int columnId, DailyValue& out) {
    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT * FROM daily_values WHERE entity_id=? AND report_date=? AND column_id=?");
    q.addBindValue(entityId); q.addBindValue(date); q.addBindValue(columnId);
    if(q.exec()&&q.next()){out.id=q.value("id").toInt();out.entityId=q.value("entity_id").toInt();out.reportDate=q.value("report_date").toString();out.columnId=q.value("column_id").toInt();out.value=q.value("value").toDouble();return true;}
    return false;
}

bool DailyDao::saveValues(const QVector<DailyValue>& values) {
    if(values.isEmpty()) return true;
    QSqlDatabase& db = Database::instance().db();
    db.transaction();
    QSqlQuery q(db);
    for(const auto& dv:values){
        q.prepare("DELETE FROM daily_values WHERE entity_id=? AND report_date=? AND column_id=?");
        q.addBindValue(dv.entityId); q.addBindValue(dv.reportDate); q.addBindValue(dv.columnId);
        if(!q.exec()){db.rollback();return false;}
        q.prepare("INSERT INTO daily_values (entity_id,report_date,column_id,value) VALUES (?,?,?,?)");
        q.addBindValue(dv.entityId); q.addBindValue(dv.reportDate); q.addBindValue(dv.columnId); q.addBindValue(dv.value);
        if(!q.exec()){db.rollback();return false;}
    }
    return db.commit();
}

QVector<DisplayRow> DailyDao::queryForDisplay(const QDate& startDate, const QDate& endDate, int entityTypeId, const QVector<int>& columnIds) {
    QVector<DisplayRow> result;
    QString sql = "SELECT e.id AS entity_id, e.name AS entity_name, e.parent_id, et.name AS entity_type, dv.report_date, dv.column_id, dv.value FROM daily_values dv JOIN entities e ON dv.entity_id=e.id JOIN entity_types et ON e.type_id=et.id WHERE dv.report_date BETWEEN ? AND ? AND e.is_active=1";
    if(entityTypeId>0) sql+=" AND e.type_id=?";
    if(!columnIds.isEmpty()) sql+=" AND dv.column_id IN ("+placeholders(columnIds.size())+")";
    sql+=" ORDER BY dv.report_date DESC, et.sort_order, e.parent_id, e.sort_order";

    QSqlQuery q(Database::instance().db());
    q.prepare(sql);
    q.addBindValue(startDate.toString("yyyy-MM-dd")); q.addBindValue(endDate.toString("yyyy-MM-dd"));
    int idx=2;
    if(entityTypeId>0) q.addBindValue(entityTypeId);
    for(int id:columnIds) q.addBindValue(id);
    if(!q.exec()){qWarning()<<"queryForDisplay failed:"<<q.lastError().text();return result;}

    QMap<QPair<int,QString>,DisplayRow> rowMap;
    while(q.next()){
        QPair<int,QString> key(q.value("entity_id").toInt(),q.value("report_date").toString());
        if(!rowMap.contains(key)){DisplayRow r;r.entityId=q.value("entity_id").toInt();r.entityName=q.value("entity_name").toString();r.entityType=q.value("entity_type").toString();r.parentCompanyId=q.value("parent_id").isNull()?0:q.value("parent_id").toInt();r.date=QDate::fromString(q.value("report_date").toString(),"yyyy-MM-dd");rowMap[key]=r;}
        rowMap[key].values[q.value("column_id").toInt()]=q.value("value").toDouble();
    }
    for(auto it=rowMap.begin();it!=rowMap.end();++it) result.append(it.value());
    return result;
}

QVector<DisplayRow> DailyDao::queryForChart(const QDate& startDate, const QDate& endDate, const QVector<int>& entityIds, const QVector<int>& columnIds) {
    QVector<DisplayRow> result;
    if(entityIds.isEmpty()||columnIds.isEmpty()) return result;

    QString sql = "SELECT e.id AS entity_id, e.name AS entity_name, e.parent_id, et.name AS entity_type, dv.report_date, dv.column_id, dv.value FROM daily_values dv JOIN entities e ON dv.entity_id=e.id JOIN entity_types et ON e.type_id=et.id WHERE dv.report_date BETWEEN ? AND ? AND e.is_active=1 AND dv.entity_id IN ("+placeholders(entityIds.size())+") AND dv.column_id IN ("+placeholders(columnIds.size())+") ORDER BY dv.report_date, e.sort_order";

    QSqlQuery q(Database::instance().db());
    q.prepare(sql);
    q.addBindValue(startDate.toString("yyyy-MM-dd")); q.addBindValue(endDate.toString("yyyy-MM-dd"));
    for(int id:entityIds) q.addBindValue(id);
    for(int id:columnIds) q.addBindValue(id);
    if(!q.exec()){qWarning()<<"queryForChart failed:"<<q.lastError().text();return result;}

    QMap<QPair<int,QString>,DisplayRow> rowMap;
    while(q.next()){
        QPair<int,QString> key(q.value("entity_id").toInt(),q.value("report_date").toString());
        if(!rowMap.contains(key)){DisplayRow r;r.entityId=q.value("entity_id").toInt();r.entityName=q.value("entity_name").toString();r.entityType=q.value("entity_type").toString();r.parentCompanyId=q.value("parent_id").isNull()?0:q.value("parent_id").toInt();r.date=QDate::fromString(q.value("report_date").toString(),"yyyy-MM-dd");rowMap[key]=r;}
        rowMap[key].values[q.value("column_id").toInt()]=q.value("value").toDouble();
    }
    for(auto it=rowMap.begin();it!=rowMap.end();++it) result.append(it.value());
    return result;
}

bool DailyDao::remove(int entityId, const QString& date) {
    QSqlQuery q(Database::instance().db());
    q.prepare("DELETE FROM daily_values WHERE entity_id=? AND report_date=?");
    q.addBindValue(entityId); q.addBindValue(date);
    return q.exec();
}

bool DailyDao::removeRange(int entityId, const QDate& startDate, const QDate& endDate) {
    QSqlDatabase& db = Database::instance().db();
    db.transaction();
    QSqlQuery q(db);
    q.prepare("DELETE FROM daily_values WHERE entity_id=? AND report_date BETWEEN ? AND ?");
    q.addBindValue(entityId); q.addBindValue(startDate.toString("yyyy-MM-dd")); q.addBindValue(endDate.toString("yyyy-MM-dd"));
    if(!q.exec()){db.rollback();return false;}
    return db.commit();
}

QDate DailyDao::getLatestDate() {
    QSqlQuery q(Database::instance().db());
    q.exec("SELECT MAX(report_date) FROM daily_values");
    if(q.next()&&!q.value(0).isNull()) return QDate::fromString(q.value(0).toString(),"yyyy-MM-dd");
    return QDate();
}

QDate DailyDao::getEarliestDate() {
    QSqlQuery q(Database::instance().db());
    q.exec("SELECT MIN(report_date) FROM daily_values");
    if(q.next()&&!q.value(0).isNull()) return QDate::fromString(q.value(0).toString(),"yyyy-MM-dd");
    return QDate();
}

QVector<QDate> DailyDao::getAvailableDates() {
    QVector<QDate> dates;
    QSqlQuery q(Database::instance().db());
    q.exec("SELECT DISTINCT report_date FROM daily_values ORDER BY report_date DESC");
    while(q.next()) dates.append(QDate::fromString(q.value(0).toString(),"yyyy-MM-dd"));
    return dates;
}
