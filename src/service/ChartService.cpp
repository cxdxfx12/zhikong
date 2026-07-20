#include "ChartService.h"
#include "db/DailyDao.h"
#include "db/EntityDao.h"
#include "db/ColumnDao.h"
#include <algorithm>

QVector<ChartSeriesData> ChartService::prepareSeries(
    const QDate& startDate, const QDate& endDate,
    const QVector<int>& entityIds,
    const QVector<int>& columnIds) {

    QVector<ChartSeriesData> result;

    auto rows = DailyDao::queryForChart(startDate, endDate, entityIds, columnIds);

    // Gather entity names
    QMap<int, QString> entityNames;
    for (int eid : entityIds) {
        auto e = EntityDao::getById(eid);
        entityNames[eid] = e.fullPath();
    }

    // Gather column defs
    QMap<int, ColumnDef> colMap;
    for (int cid : columnIds) {
        colMap[cid] = ColumnDao::getById(cid);
    }

    // Build series: one per (entityId, columnId) combination
    for (int eid : entityIds) {
        for (int cid : columnIds) {
            ChartSeriesData series;
            series.entityId = eid;
            series.entityName = entityNames.value(eid, QString::number(eid));
            series.columnId = cid;
            series.columnName = colMap.value(cid).displayNameWithUnit();

            // Extract points for this entity+column, sorted by date
            QMap<QDate, double> pointMap;
            for (const auto& row : rows) {
                if (row.entityId == eid && row.values.contains(cid)) {
                    pointMap[row.date] = row.values[cid];
                }
            }

            for (auto it = pointMap.begin(); it != pointMap.end(); ++it) {
                series.points.append({it.key(), it.value()});
            }

            result.append(series);
        }
    }

    return result;
}

QVector<ChartSeriesData> ChartService::buildChartSeries(
    const QVector<DisplayRow>& rows,
    const QVector<int>& entityIds,
    const QVector<ColumnDef>& columns) {

    QVector<ChartSeriesData> result;

    QMap<int, QString> entityNames;
    for (const auto& row : rows) {
        entityNames[row.entityId] = row.entityName;
    }

    for (int eid : entityIds) {
        for (const auto& col : columns) {
            ChartSeriesData series;
            series.entityId = eid;
            series.entityName = entityNames.value(eid, QString::number(eid));
            series.columnId = col.id;
            series.columnName = col.displayNameWithUnit();

            QMap<QDate, double> pointMap;
            for (const auto& row : rows) {
                if (row.entityId == eid && row.values.contains(col.id)) {
                    pointMap[row.date] = row.values[col.id];
                }
            }
            for (auto it = pointMap.begin(); it != pointMap.end(); ++it) {
                series.points.append({it.key(), it.value()});
            }

            result.append(series);
        }
    }

    return result;
}
