#pragma once
#include <QVector>
#include <QStringList>
#include "model/Entity.h"

class EntityDao {
public:
    static QVector<Entity> getAll(int typeId = 0);
    static QVector<Entity> getCompanies();
    static QVector<Entity> getContractors(int parentCompanyId = 0);
    static QVector<Entity> getContractorsByCompany(int companyId);
    static Entity getById(int id);
    static bool insert(Entity& e);
    static bool update(const Entity& e);
    static bool remove(int id);
    static QStringList getTypeNames();

    static QVector<EntityTreeNode> getTree();
    static QString getFullPath(int entityId);
};
