#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include "model/DailyValue.h"
#include "model/ColumnDef.h"
#include "model/Entity.h"

struct ImportPreviewRow {
    QString date;
    QString entityName;
    int matchedEntityId = 0;
    QMap<int, double> values;
    QMap<QString, double> rawValues;
};

struct ColumnMapping {
    QString excelHeader;
    int columnDefId = 0;
    QString matchedColumnKey;
    bool isMapped = false;
};

struct ImportResult {
    bool success = false;
    QString errorMessage;
    int totalRows = 0;
    int successRows = 0;
    int skippedRows = 0;
    QDate startDate;
    QDate endDate;
    QString verifyMsg;
};

class ImportService : public QObject {
    Q_OBJECT
public:
    explicit ImportService(QObject* parent = nullptr);

    bool parsePreview(const QString& filePath,
                      QVector<ImportPreviewRow>& previewRows,
                      QVector<ColumnMapping>& mappings,
                      QStringList& excelHeaders);

    ImportResult executeImportDirect(const QString& filePath, int forceEntityId);

    struct CachedRow { QString date; QVector<QPair<int,double>> values; };
    struct CachedData { QVector<CachedRow> rows; QDate minDate, maxDate; };
    CachedData m_cached;
    QString m_diagMsg;

signals:
    void progressChanged(int percent);
    void importFinished(const ImportResult& result);
    void importError(const QString& error);

private:
    QVector<Entity> m_allEntities;
    QVector<ColumnDef> m_allColumns;
};
