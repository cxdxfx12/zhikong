#pragma once
#include <QVector>
#include <QDate>
#include "model/DailyValue.h"
#include "model/ColumnDef.h"

class DailyDao {
public:
    static bool saveValue(int entityId, const QString& date, int columnId, double value);
    static bool saveValues(const QVector<DailyValue>& values);
    static bool getValue(int entityId, const QString& date, int columnId, DailyValue& out);

    static QVector<DisplayRow> queryForDisplay(
        const QDate& startDate, const QDate& endDate,
        int entityTypeId = 0,
        const QVector<int>& columnIds = {}
    );

    static QVector<DisplayRow> queryForChart(
        const QDate& startDate, const QDate& endDate,
        const QVector<int>& entityIds,
        const QVector<int>& columnIds
    );

    static bool remove(int entityId, const QString& date);
    static bool removeRange(int entityId, const QDate& startDate, const QDate& endDate);

    static QDate getLatestDate();
    static QDate getEarliestDate();
    static QVector<QDate> getAvailableDates();
};
