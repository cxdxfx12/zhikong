#pragma once
#include <QVector>
#include <QDate>
#include <QMap>
#include "model/DailyValue.h"
#include "model/ColumnDef.h"

// Simple data series for charting: list of (date, value) pairs
struct ChartSeriesData {
    int entityId;
    QString entityName;
    int columnId;
    QString columnName;
    QVector<QPair<QDate, double>> points;  // sorted by date
};

class ChartService {
public:
    static QVector<ChartSeriesData> prepareSeries(
        const QDate& startDate, const QDate& endDate,
        const QVector<int>& entityIds,
        const QVector<int>& columnIds);

    // Build combined series: for N entities × M columns,
    // each (entity, column) pair becomes a chart series
    static QVector<ChartSeriesData> buildChartSeries(
        const QVector<DisplayRow>& rows,
        const QVector<int>& entityIds,
        const QVector<ColumnDef>& columns);
};
