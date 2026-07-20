#pragma once
#include <QString>
#include <QVector>
#include "model/DailyValue.h"
#include "model/ColumnDef.h"

class ExportService {
public:
    // Export display rows to CSV
    static bool exportToCsv(const QString& filePath,
                            const QVector<DisplayRow>& rows,
                            const QVector<ColumnDef>& columns);

    // Export to Excel (requires QXlsx)
    static bool exportToExcel(const QString& filePath,
                              const QVector<DisplayRow>& rows,
                              const QVector<ColumnDef>& columns);
};
