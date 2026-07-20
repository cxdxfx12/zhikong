#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include "service/ImportService.h"

class ImportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImportDialog(QWidget* parent = nullptr);

private slots:
    void onBrowseFile();
    void onLoadPreview();
    void onExecuteImport();
    void onMappingChanged(int row, int col);

private:
    void setupUI();
    void displayPreview();
    void displayMappings();

    // File selection
    QLineEdit* m_filePathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QPushButton* m_loadBtn = nullptr;

    // Entity selector (since Excel may not have entity column)
    QComboBox* m_entityCombo = nullptr;

    // Preview table
    QTableWidget* m_previewTable = nullptr;

    // Mapping table (excel header → column def)
    QTableWidget* m_mappingTable = nullptr;

    // Import button
    QPushButton* m_importBtn = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;

    // Data
    ImportService* m_service = nullptr;
    QVector<ImportPreviewRow> m_previewRows;
    QVector<ColumnMapping> m_mappings;
    QStringList m_excelHeaders;
    QString m_filePath;
};
