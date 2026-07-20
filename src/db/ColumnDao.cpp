#include "ColumnDao.h"
#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

static ColumnDef colFromQuery(const QSqlQuery& q) {
    ColumnDef c;
    c.id            = q.value("id").toInt();
    c.key           = q.value("key").toString();
    c.displayName   = q.value("display_name").toString();
    c.dataType      = q.value("data_type").toString();
    c.unit          = q.value("unit").toString();
    c.entityTypeId  = q.value("entity_type_id").isNull() ? 0 : q.value("entity_type_id").toInt();
    c.aggregateType = q.value("aggregate_type").toString();
    c.category      = q.value("category").toString();
    c.isCore        = q.value("is_core").toBool();
    c.sortOrder     = q.value("sort_order").toInt();
    if (c.aggregateType.isEmpty()) c.aggregateType = "NONE";
    return c;
}

QVector<ColumnDef> ColumnDao::getAll(bool coreOnly) {
    QVector<ColumnDef> result;
    QSqlQuery q(Database::instance().db());
    QString sql = "SELECT * FROM column_defs";
    if (coreOnly) sql += " WHERE is_core = 1";
    sql += " ORDER BY sort_order, id";

    if (!q.exec(sql)) {
        qWarning() << "ColumnDao::getAll failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) result.append(colFromQuery(q));
    return result;
}

QVector<ColumnDef> ColumnDao::getByEntityType(int entityTypeId, bool coreOnly) {
    QVector<ColumnDef> result;
    QSqlQuery q(Database::instance().db());
    QString sql = R"(
        SELECT * FROM column_defs
        WHERE (entity_type_id IS NULL OR entity_type_id = :typeId)
    )";
    if (coreOnly) sql += " AND is_core = 1";
    sql += " ORDER BY sort_order, id";

    q.prepare(sql);
    q.bindValue(":typeId", entityTypeId);

    if (!q.exec()) {
        qWarning() << "ColumnDao::getByEntityType failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) result.append(colFromQuery(q));
    return result;
}

ColumnDef ColumnDao::getById(int id) {
    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT * FROM column_defs WHERE id = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) return colFromQuery(q);
    return ColumnDef();
}

ColumnDef ColumnDao::getByKey(const QString& key) {
    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT * FROM column_defs WHERE key = :key");
    q.bindValue(":key", key);
    if (q.exec() && q.next()) return colFromQuery(q);
    return ColumnDef();
}

bool ColumnDao::insert(ColumnDef& c) {
    QSqlQuery q(Database::instance().db());
    q.prepare(R"(
        INSERT INTO column_defs (key, display_name, data_type, unit, entity_type_id,
                                 aggregate_type, category, is_core, sort_order)
        VALUES (:key, :displayName, :dataType, :unit, :entityTypeId,
                :aggregateType, :category, :isCore, :sortOrder)
    )");
    q.bindValue(":key", c.key);
    q.bindValue(":displayName", c.displayName);
    q.bindValue(":dataType", c.dataType);
    q.bindValue(":unit", c.unit);
    q.bindValue(":entityTypeId", c.entityTypeId > 0 ? QVariant(c.entityTypeId) : QVariant());
    q.bindValue(":aggregateType", c.aggregateType);
    q.bindValue(":category", c.category);
    q.bindValue(":isCore", c.isCore ? 1 : 0);
    q.bindValue(":sortOrder", c.sortOrder);

    if (!q.exec()) {
        qWarning() << "ColumnDao::insert failed:" << q.lastError().text();
        return false;
    }
    c.id = q.lastInsertId().toInt();
    return true;
}

bool ColumnDao::update(const ColumnDef& c) {
    QSqlQuery q(Database::instance().db());
    q.prepare(R"(
        UPDATE column_defs SET key=:key, display_name=:displayName,
               data_type=:dataType, unit=:unit, entity_type_id=:entityTypeId,
               aggregate_type=:aggregateType, category=:category,
               is_core=:isCore, sort_order=:sortOrder
        WHERE id=:id
    )");
    q.bindValue(":id", c.id);
    q.bindValue(":key", c.key);
    q.bindValue(":displayName", c.displayName);
    q.bindValue(":dataType", c.dataType);
    q.bindValue(":unit", c.unit);
    q.bindValue(":entityTypeId", c.entityTypeId > 0 ? QVariant(c.entityTypeId) : QVariant());
    q.bindValue(":aggregateType", c.aggregateType);
    q.bindValue(":category", c.category);
    q.bindValue(":isCore", c.isCore ? 1 : 0);
    q.bindValue(":sortOrder", c.sortOrder);

    if (!q.exec()) {
        qWarning() << "ColumnDao::update failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool ColumnDao::remove(int id) {
    QSqlQuery q(Database::instance().db());
    q.prepare("DELETE FROM column_defs WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "ColumnDao::remove failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool ColumnDao::updateSortOrders(const QVector<int>& orderedIds) {
    QSqlDatabase& db = Database::instance().db();
    db.transaction();
    QSqlQuery q(db);
    q.prepare("UPDATE column_defs SET sort_order = :order WHERE id = :id");
    for (int i = 0; i < orderedIds.size(); ++i) {
        q.bindValue(":order", i);
        q.bindValue(":id", orderedIds[i]);
        if (!q.exec()) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}
