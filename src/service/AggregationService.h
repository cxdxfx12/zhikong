#pragma once
#include <QMap>
#include <QDate>
#include <QVector>
#include "model/DailyValue.h"
#include "model/ColumnDef.h"

class AggregationService {
public:
    // Compute aggregated values for a company on a single date
    static QMap<int, double> computeCompanyAggregate(
        int companyEntityId, const QString& date);

    // Compute aggregated values for a company across a date range
    static QVector<DailyValue> computeCompanyAggregates(
        int companyEntityId, const QDate& startDate, const QDate& endDate);

    // Re-aggregate and save company data after contractor changes
    // Returns the saved company daily_values
    static bool regenerateCompanyData(
        int companyEntityId, const QDate& date);

private:
    static double aggregateValues(
        const QString& aggregateType, const QVector<double>& childValues);
};
