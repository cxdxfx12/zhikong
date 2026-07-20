#pragma once
#include <QVector>
#include "model/ColumnDef.h"

class ColumnDao {
public:
    static QVector<ColumnDef> getAll(bool coreOnly = false);
    static QVector<ColumnDef> getByEntityType(int entityTypeId, bool coreOnly = false);
    static ColumnDef getById(int id);
    static ColumnDef getByKey(const QString& key);
    static bool insert(ColumnDef& c);
    static bool update(const ColumnDef& c);
    static bool remove(int id);
    static bool updateSortOrders(const QVector<int>& orderedIds);
};
