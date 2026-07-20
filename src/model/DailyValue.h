#pragma once
#include <QString>
#include <QDate>
#include <QMap>
#include <QVector>

struct DailyValue {
    int id = 0;
    int entityId = 0;
    QString reportDate;    // "yyyy-MM-dd"
    int columnId = 0;
    double value = 0.0;
};

// A display row: one entity on one date, with all column values mapped
struct DisplayRow {
    int entityId = 0;
    QString entityName;
    QString entityType;       // "公司" | "承包区"
    int parentCompanyId = 0;  // for contractors, their parent company
    QDate date;
    QMap<int, double> values; // columnId -> value

    bool isCompany() const { return entityType == u"公司"; }
    bool isContractor() const { return entityType == u"承包区"; }
};
