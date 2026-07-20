#include "EntityDao.h"
#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

static Entity entityFromQuery(const QSqlQuery& q) {
    Entity e;
    e.id         = q.value("id").toInt();
    e.typeId     = q.value("type_id").toInt();
    e.parentId   = q.value("parent_id").isNull() ? 0 : q.value("parent_id").toInt();
    e.name       = q.value("name").toString();
    e.sortOrder  = q.value("sort_order").toInt();
    e.isActive   = q.value("is_active").toBool();
    e.typeName   = q.value("type_name").toString();
    e.parentName = q.value("parent_name").toString();
    return e;
}

QVector<Entity> EntityDao::getAll(int typeId) {
    QVector<Entity> result;
    QSqlQuery q(Database::instance().db());
    QString sql = R"(
        SELECT e.*, et.name AS type_name,
               COALESCE(p.name, '') AS parent_name
        FROM entities e
        JOIN entity_types et ON e.type_id = et.id
        LEFT JOIN entities p ON e.parent_id = p.id
        WHERE e.is_active = 1
    )";
    if (typeId > 0) {
        sql += " AND e.type_id = :typeId";
    }
    sql += " ORDER BY et.sort_order, e.parent_id, e.sort_order";

    q.prepare(sql);
    if (typeId > 0) q.bindValue(":typeId", typeId);

    if (!q.exec()) {
        qWarning() << "EntityDao::getAll failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) result.append(entityFromQuery(q));
    return result;
}

QVector<Entity> EntityDao::getCompanies() {
    return getAll(1);  // type_id=1
}

QVector<Entity> EntityDao::getContractors(int parentCompanyId) {
    QVector<Entity> result;
    QSqlQuery q(Database::instance().db());
    QString sql = R"(
        SELECT e.*, et.name AS type_name,
               COALESCE(p.name, '') AS parent_name
        FROM entities e
        JOIN entity_types et ON e.type_id = et.id
        LEFT JOIN entities p ON e.parent_id = p.id
        WHERE e.type_id = 2 AND e.is_active = 1
    )";
    if (parentCompanyId > 0) {
        sql += " AND e.parent_id = :pid";
    }
    sql += " ORDER BY e.parent_id, e.sort_order";

    q.prepare(sql);
    if (parentCompanyId > 0) q.bindValue(":pid", parentCompanyId);

    if (q.exec()) {
        while (q.next()) result.append(entityFromQuery(q));
    }
    return result;
}

QVector<Entity> EntityDao::getContractorsByCompany(int companyId) {
    return getContractors(companyId);
}

Entity EntityDao::getById(int id) {
    QSqlQuery q(Database::instance().db());
    q.prepare(R"(
        SELECT e.*, et.name AS type_name,
               COALESCE(p.name, '') AS parent_name
        FROM entities e
        JOIN entity_types et ON e.type_id = et.id
        LEFT JOIN entities p ON e.parent_id = p.id
        WHERE e.id = :id
    )");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) return entityFromQuery(q);
    return Entity();
}

bool EntityDao::insert(Entity& e) {
    QSqlQuery q(Database::instance().db());
    q.prepare(R"(
        INSERT INTO entities (type_id, parent_id, name, sort_order, is_active)
        VALUES (:typeId, :parentId, :name, :sortOrder, :isActive)
    )");
    q.bindValue(":typeId", e.typeId);
    q.bindValue(":parentId", e.parentId > 0 ? QVariant(e.parentId) : QVariant());
    q.bindValue(":name", e.name);
    q.bindValue(":sortOrder", e.sortOrder);
    q.bindValue(":isActive", e.isActive ? 1 : 0);

    if (!q.exec()) {
        qWarning() << "EntityDao::insert failed:" << q.lastError().text();
        return false;
    }
    e.id = q.lastInsertId().toInt();
    return true;
}

bool EntityDao::update(const Entity& e) {
    QSqlQuery q(Database::instance().db());
    q.prepare(R"(
        UPDATE entities SET type_id=:typeId, parent_id=:parentId,
               name=:name, sort_order=:sortOrder, is_active=:isActive
        WHERE id=:id
    )");
    q.bindValue(":id", e.id);
    q.bindValue(":typeId", e.typeId);
    q.bindValue(":parentId", e.parentId > 0 ? QVariant(e.parentId) : QVariant());
    q.bindValue(":name", e.name);
    q.bindValue(":sortOrder", e.sortOrder);
    q.bindValue(":isActive", e.isActive ? 1 : 0);

    if (!q.exec()) {
        qWarning() << "EntityDao::update failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool EntityDao::remove(int id) {
    QSqlQuery q(Database::instance().db());
    q.prepare("DELETE FROM entities WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "EntityDao::remove failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QStringList EntityDao::getTypeNames() {
    QStringList names;
    QSqlQuery q(Database::instance().db());
    q.exec("SELECT name FROM entity_types ORDER BY sort_order");
    while (q.next()) names.append(q.value(0).toString());
    return names;
}

QVector<EntityTreeNode> EntityDao::getTree() {
    QVector<EntityTreeNode> result;
    auto companies = getCompanies();
    for (const auto& comp : companies) {
        EntityTreeNode node;
        node.entity = comp;
        auto contractors = getContractorsByCompany(comp.id);
        for (const auto& c : contractors) {
            EntityTreeNode child;
            child.entity = c;
            node.children.append(child);
        }
        result.append(node);
    }
    return result;
}

QString EntityDao::getFullPath(int entityId) {
    auto e = getById(entityId);
    return e.fullPath();
}
