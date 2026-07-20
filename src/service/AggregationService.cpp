#include "AggregationService.h"
#include "db/EntityDao.h"
#include "db/ColumnDao.h"
#include "db/DailyDao.h"
#include <QDebug>
#include <algorithm>

QMap<int, double> AggregationService::computeCompanyAggregate(
    int companyEntityId, const QString& date) {

    QMap<int, double> result;

    // 1. Get all child contractors of this company
    auto contractors = EntityDao::getContractorsByCompany(companyEntityId);
    if (contractors.isEmpty()) return result;

    QVector<int> childIds;
    for (const auto& c : contractors)
        childIds.append(c.id);

    // 2. Get all columns that need aggregation
    auto allColumns = ColumnDao::getAll();
    QVector<ColumnDef> aggColumns;
    for (const auto& col : allColumns) {
        if (col.needsAggregation())
            aggColumns.append(col);
    }

    // 3. For each aggregation column, collect child values and aggregate
    for (const auto& col : aggColumns) {
        QVector<double> childValues;

        for (int childId : childIds) {
            DailyValue dv;
            if (DailyDao::getValue(childId, date, col.id, dv)) {
                childValues.append(dv.value);
            }
        }

        if (!childValues.isEmpty()) {
            result[col.id] = aggregateValues(col.aggregateType, childValues);
        }
    }

    return result;
}

QVector<DailyValue> AggregationService::computeCompanyAggregates(
    int companyEntityId, const QDate& startDate, const QDate& endDate) {

    QVector<DailyValue> result;

    for (QDate d = startDate; d <= endDate; d = d.addDays(1)) {
        auto agg = computeCompanyAggregate(companyEntityId, d.toString("yyyy-MM-dd"));
        for (auto it = agg.begin(); it != agg.end(); ++it) {
            DailyValue dv;
            dv.entityId = companyEntityId;
            dv.reportDate = d.toString("yyyy-MM-dd");
            dv.columnId = it.key();
            dv.value = it.value();
            result.append(dv);
        }
    }

    return result;
}

bool AggregationService::regenerateCompanyData(
    int companyEntityId, const QDate& date) {

    auto agg = computeCompanyAggregate(companyEntityId, date.toString("yyyy-MM-dd"));

    if (agg.isEmpty()) return true; // nothing to aggregate

    QVector<DailyValue> values;
    for (auto it = agg.begin(); it != agg.end(); ++it) {
        DailyValue dv;
        dv.entityId = companyEntityId;
        dv.reportDate = date.toString("yyyy-MM-dd");
        dv.columnId = it.key();
        dv.value = it.value();
        values.append(dv);
    }

    bool ok = DailyDao::saveValues(values);
    if (!ok) {
        qWarning() << "AggregationService: failed to save aggregated data for company"
                   << companyEntityId << "on" << date;
    }
    return ok;
}

double AggregationService::aggregateValues(
    const QString& type, const QVector<double>& values) {

    if (type == "SUM") {
        double sum = 0;
        for (double v : values) sum += v;
        return sum;
    }

    if (type == "AVG") {
        double sum = 0;
        for (double v : values) sum += v;
        return values.isEmpty() ? 0 : sum / values.size();
    }

    return 0; // NONE — shouldn't reach here
}
