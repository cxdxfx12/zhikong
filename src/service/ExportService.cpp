#include "ExportService.h"
#include "utils/FormatUtils.h"
#include <QFile>
#include <QTextStream>

#ifdef HAS_QXLSX
#include <xlsxdocument.h>
#endif

bool ExportService::exportToCsv(const QString& filePath,
                                 const QVector<DisplayRow>& rows,
                                 const QVector<ColumnDef>& columns) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    // UTF-8 BOM for Excel compatibility
    out << "\xEF\xBB\xBF";

    // Header
    out << "日期,类型,名称";
    for (const auto& col : columns) {
        out << "," << col.displayNameWithUnit();
    }
    out << "\n";

    // Data
    for (const auto& row : rows) {
        out << FormatUtils::dateToString(row.date) << ","
            << row.entityType << ","
            << "\"" << row.entityName << "\"";

        for (const auto& col : columns) {
            out << ",";
            if (row.values.contains(col.id)) {
                out << FormatUtils::formatValue(row.values[col.id], col);
            }
        }
        out << "\n";
    }

    file.close();
    return true;
}

bool ExportService::exportToExcel(const QString& filePath,
                                   const QVector<DisplayRow>& rows,
                                   const QVector<ColumnDef>& columns) {
#ifdef HAS_QXLSX
    QXlsx::Document xlsx;

    // Header row
    xlsx.write(1, 1, "日期");
    xlsx.write(1, 2, "类型");
    xlsx.write(1, 3, "名称");
    for (int i = 0; i < columns.size(); ++i) {
        xlsx.write(1, 4 + i, columns[i].displayNameWithUnit());
    }

    // Data rows
    for (int r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        int excelRow = r + 2;
        xlsx.write(excelRow, 1, FormatUtils::dateToString(row.date));
        xlsx.write(excelRow, 2, row.entityType);
        xlsx.write(excelRow, 3, row.entityName);

        for (int c = 0; c < columns.size(); ++c) {
            if (row.values.contains(columns[c].id)) {
                xlsx.write(excelRow, 4 + c, row.values[columns[c].id]);
            }
        }
    }

    return xlsx.saveAs(filePath);
#else
    // Fallback to CSV
    QString csvPath = filePath;
    csvPath.replace(".xlsx", ".csv");
    return exportToCsv(csvPath, rows, columns);
#endif
}
