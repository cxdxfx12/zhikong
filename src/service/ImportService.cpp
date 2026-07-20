#include "ImportService.h"
#include "db/Database.h"
#include "db/EntityDao.h"
#include "db/ColumnDao.h"
#include "db/DailyDao.h"
#include "utils/FormatUtils.h"
#include <QRegularExpression>
#include <QSqlQuery>

#ifdef HAS_QXLSX
#include <xlsxdocument.h>
#endif

ImportService::ImportService(QObject* parent) : QObject(parent) {
    m_allEntities = EntityDao::getAll();
    m_allColumns  = ColumnDao::getAll();
}

bool ImportService::parsePreview(const QString& filePath,
                                  QVector<ImportPreviewRow>& previewRows,
                                  QVector<ColumnMapping>& mappings,
                                  QStringList& excelHeaders) {
#ifdef HAS_QXLSX
    QXlsx::Document xlsx(filePath);
    if (!xlsx.load()) { emit importError("无法打开Excel文件"); return false; }

    auto allCols = ColumnDao::getAll();
    QMap<QString, int> cleanToId;
    for (const auto& col : allCols) {
        QString c = col.displayName;
        c.remove(QRegularExpression("[,，、。\\s\\(\\)（）ppm]"));
        if (!c.isEmpty()) cleanToId[c] = col.id;
    }

    // Read headers
    int col = 1;
    while (true) {
        QVariant cell = xlsx.read(1, col);
        if (cell.isNull()) break;
        QString header = cell.toString().trimmed();
        if (header.isEmpty()) break;
        excelHeaders << header;

        QString cleanHeader = header;
        cleanHeader.remove(QRegularExpression("[,，、。\\s\\(\\)（）pmPM]"));
        ColumnMapping m; m.excelHeader = header;
        if (cleanToId.contains(cleanHeader)) {
            m.columnDefId = cleanToId[cleanHeader]; m.isMapped = true;
            for (const auto& cd : allCols) if (cd.id == m.columnDefId) { m.matchedColumnKey = cd.key; break; }
        } else {
            int bestId = 0, bestLen = 0;
            for (const auto& cd : allCols) {
                QString cdClean = cd.displayName; cdClean.remove(QRegularExpression("[,，、。\\s\\(\\)（）pmPM]"));
                if (cleanHeader.contains(cdClean) || cdClean.contains(cleanHeader)) {
                    if (cdClean.length() > bestLen) { bestId = cd.id; bestLen = cdClean.length(); }
                }
            }
            if (bestId > 0) { m.columnDefId = bestId; m.isMapped = true; }
        }
        mappings << m; col++;
    }

    // DIAGNOSTIC: test what QXlsx actually returns for specific cells
    double testRead2  = FormatUtils::variantToDouble(xlsx.read(2, 2));
    double testRead3  = FormatUtils::variantToDouble(xlsx.read(2, 3));
    double testRead20 = FormatUtils::variantToDouble(xlsx.read(2, 20));
    m_diagMsg = QString("QXlsx直读: (2,2)=%1 (2,3)=%2 (2,20)=%3 | 预期20016/45270/614.9")
        .arg(testRead2).arg(testRead3).arg(testRead20);

    // Find date column
    int dateCol = 1;
    for (int c = 0; c < excelHeaders.size(); ++c) {
        QString h = excelHeaders[c]; h.remove(QRegularExpression("[,，、。\\s]"));
        if (h == "日期") { dateCol = c + 1; break; }
    }

    // Read ALL data rows and CACHE for later import
    m_cached.rows.clear();
    m_cached.minDate = QDate();
    m_cached.maxDate = QDate();
    int row = 2;
    while (true) {
        QVariant fc = xlsx.read(row, 1);
        if (fc.isNull() || fc.toString().trimmed().isEmpty()) break;

        QDate d = FormatUtils::variantToDate(xlsx.read(row, dateCol));
        QString dateStr = d.isValid() ? d.toString("yyyy-MM-dd") : "";

        QVector<QPair<int, double>> rowData;
        ImportPreviewRow pr;
        pr.date = dateStr;
        for (int c = 0; c < mappings.size(); ++c) {
            double v = FormatUtils::variantToDouble(xlsx.read(row, c + 1));
            pr.rawValues[mappings[c].excelHeader] = v;
            if (mappings[c].isMapped) {
                pr.values[mappings[c].columnDefId] = v;
                rowData.append({mappings[c].columnDefId, v});
            }
        }

        if (row - 2 < 20) previewRows << pr;
        if (d.isValid() && !dateStr.isEmpty()) {
            CachedRow cr;
            cr.date = dateStr;
            cr.values = rowData;
            m_cached.rows.append(cr);
            if (!m_cached.minDate.isValid() || d < m_cached.minDate) m_cached.minDate = d;
            if (!m_cached.maxDate.isValid() || d > m_cached.maxDate) m_cached.maxDate = d;
        }
        row++;
    }
    return true;
#else
    Q_UNUSED(filePath); Q_UNUSED(previewRows); Q_UNUSED(mappings); Q_UNUSED(excelHeaders);
    emit importError("QXlsx not available"); return false;
#endif
}

ImportResult ImportService::executeImport(const QString&, const QVector<ColumnMapping>&, int) {
    ImportResult r; r.errorMessage = "Use the Import dialog button"; return r;
}

ImportResult ImportService::executeImportDirect(const QString&, int forceEntityId) {
    ImportResult result;

    if (forceEntityId <= 0) { result.errorMessage = "未指定实体"; return result; }
    if (m_cached.rows.isEmpty()) { result.errorMessage = "无缓存数据，请先点加载预览"; return result; }

    // Build values from cache (data was already read correctly in parsePreview)
    QVector<DailyValue> allValues;
    for (const auto& cr : m_cached.rows) {
        for (const auto& [colId, val] : cr.values) {
            DailyValue dv;
            dv.entityId = forceEntityId;
            dv.reportDate = cr.date;
            dv.columnId = colId;
            dv.value = val;
            allValues << dv;
        }
    }

    result.successRows = m_cached.rows.size();
    result.totalRows = allValues.size();

    // Save to DB
    {
        QSqlDatabase& db = Database::instance().db();
        db.transaction();
        QSqlQuery q(db);
        q.exec(QString("DELETE FROM daily_values WHERE entity_id=%1 AND report_date BETWEEN '%2' AND '%3'")
            .arg(forceEntityId).arg(m_cached.minDate.toString("yyyy-MM-dd")).arg(m_cached.maxDate.toString("yyyy-MM-dd")));
        for (const auto& dv : allValues) {
            q.exec(QString("DELETE FROM daily_values WHERE entity_id=%1 AND report_date='%2' AND column_id=%3")
                .arg(dv.entityId).arg(dv.reportDate).arg(dv.columnId));
            q.exec(QString("INSERT INTO daily_values (entity_id, report_date, column_id, value) VALUES (%1, '%2', %3, %4)")
                .arg(dv.entityId).arg(dv.reportDate).arg(dv.columnId).arg(dv.value, 0, 'f', 6));
        }
        db.commit();
    }

    // Verify
    {
        QSqlQuery vq(Database::instance().db());
        vq.exec(QString("SELECT dv.column_id, cd.display_name, dv.value FROM daily_values dv JOIN column_defs cd ON dv.column_id=cd.id WHERE dv.entity_id=%1 AND dv.report_date='%2' ORDER BY dv.column_id LIMIT 8")
            .arg(forceEntityId).arg(m_cached.minDate.toString("yyyy-MM-dd")));
        QStringList vl;
        vl << QString("缓存导入: %1行 %2值").arg(m_cached.rows.size()).arg(allValues.size());
        vl << QString("DB验证(%1):").arg(m_cached.minDate.toString("yyyy-MM-dd"));
        while (vq.next()) vl << QString("  col[%1] %2 = %3").arg(vq.value(0).toInt()).arg(vq.value(1).toString()).arg(vq.value(2).toDouble());
        result.verifyMsg = vl.join("\n");
    }

    result.success = true;
    result.startDate = m_cached.minDate;
    result.endDate = m_cached.maxDate;
    return result;
}
