#pragma once
#include <QString>

struct ColumnDef {
    int id = 0;
    QString key;
    QString displayName;
    QString dataType;       // "INTEGER" | "REAL" | "TEXT"
    QString unit;
    int entityTypeId = 0;   // 0 = all types
    QString aggregateType;  // "SUM" | "AVG" | "NONE"
    QString category;       // grouping: 业务/客服/操作/小件员取派签质量/运营
    bool isCore = true;
    int sortOrder = 0;

    bool needsAggregation() const { return aggregateType != u"NONE"; }
    bool isManualEntry()    const { return aggregateType == u"NONE"; }

    QString displayNameWithUnit() const {
        if (unit.isEmpty()) return displayName;
        return displayName + "(" + unit + ")";
    }
};
