#include "ImportDialog.h"
#include "db/ColumnDao.h"
#include "db/EntityDao.h"
#include "utils/FormatUtils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileInfo>
#include <QRegularExpression>

ImportDialog::ImportDialog(QWidget* parent)
    : QDialog(parent) {
    m_service = new ImportService(this);
    setupUI();
    setWindowTitle("导入 Excel 数据");
    resize(1000, 650);

    connect(m_service, &ImportService::progressChanged, m_progressBar, &QProgressBar::setValue);
    connect(m_service, &ImportService::importFinished, this, [this](const ImportResult& r) {
        m_progressBar->setVisible(false);
        if (r.success) {
            m_statusLabel->setText(
                QString("✅ 导入完成: %1 成功, %2 跳过")
                    .arg(r.successRows).arg(r.skippedRows));
            QMessageBox::information(this, "导入完成",
                QString("成功导入 %1 条记录\n跳过 %2 条")
                    .arg(r.successRows).arg(r.skippedRows));
            accept();
        } else {
            m_statusLabel->setText("❌ " + r.errorMessage);
        }
        m_importBtn->setEnabled(true);
    });
    connect(m_service, &ImportService::importError, this, [this](const QString& err) {
        m_progressBar->setVisible(false);
        m_statusLabel->setText("❌ " + err);
        m_importBtn->setEnabled(true);
    });
}

void ImportDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // -- File selection --
    auto* fileRow = new QHBoxLayout();
    fileRow->addWidget(new QLabel("文件:"));
    m_filePathEdit = new QLineEdit();
    m_filePathEdit->setReadOnly(true);
    fileRow->addWidget(m_filePathEdit);
    m_browseBtn = new QPushButton("浏览...");
    fileRow->addWidget(m_browseBtn);
    m_loadBtn = new QPushButton("加载预览");
    fileRow->addWidget(m_loadBtn);
    mainLayout->addLayout(fileRow);

    // -- Entity selector --
    auto* entityRow = new QHBoxLayout();
    entityRow->addWidget(new QLabel("导入到:"));
    m_entityCombo = new QComboBox();
    m_entityCombo->setMinimumWidth(200);
    // Populate with all entities
    auto entities = EntityDao::getAll();
    for (const auto& e : entities) {
        m_entityCombo->addItem(e.fullPath(), e.id);
    }
    entityRow->addWidget(m_entityCombo);
    entityRow->addStretch();
    mainLayout->addLayout(entityRow);

    // -- Mapping table --
    auto* mapGroup = new QGroupBox("列映射 (Excel列头 → 系统字段)");
    auto* mapLayout = new QVBoxLayout(mapGroup);
    m_mappingTable = new QTableWidget();
    m_mappingTable->setColumnCount(3);
    m_mappingTable->setHorizontalHeaderLabels({"Excel列头", "映射到系统字段", "状态"});
    m_mappingTable->horizontalHeader()->setStretchLastSection(true);
    m_mappingTable->setColumnWidth(0, 180);
    m_mappingTable->setColumnWidth(1, 200);
    m_mappingTable->setMaximumHeight(200);
    mapLayout->addWidget(m_mappingTable);
    mainLayout->addWidget(mapGroup);

    // -- Preview table --
    auto* prevGroup = new QGroupBox("数据预览 (前20行)");
    auto* prevLayout = new QVBoxLayout(prevGroup);
    m_previewTable = new QTableWidget();
    m_previewTable->horizontalHeader()->setStretchLastSection(true);
    prevLayout->addWidget(m_previewTable);
    mainLayout->addWidget(prevGroup, 1);

    // -- Bottom --
    auto* bottomRow = new QHBoxLayout();
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumHeight(20);
    bottomRow->addWidget(m_progressBar, 1);

    m_statusLabel = new QLabel();
    bottomRow->addWidget(m_statusLabel);

    m_importBtn = new QPushButton("📥 确认导入");
    m_importBtn->setEnabled(false);
    m_importBtn->setMinimumHeight(32);
    bottomRow->addWidget(m_importBtn);

    auto* cancelBtn = new QPushButton("取消");
    bottomRow->addWidget(cancelBtn);
    mainLayout->addLayout(bottomRow);

    // Connections
    connect(m_browseBtn, &QPushButton::clicked, this, &ImportDialog::onBrowseFile);
    connect(m_loadBtn, &QPushButton::clicked, this, &ImportDialog::onLoadPreview);
    connect(m_importBtn, &QPushButton::clicked, this, &ImportDialog::onExecuteImport);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ImportDialog::onBrowseFile() {
    QString path = QFileDialog::getOpenFileName(
        this, "选择Excel文件", QString(),
        "Excel文件 (*.xlsx *.xls);;CSV文件 (*.csv)");
    if (!path.isEmpty()) {
        m_filePathEdit->setText(path);
        m_filePath = path;
        onLoadPreview();
    }
}

void ImportDialog::onLoadPreview() {
    if (m_filePath.isEmpty()) {
        m_filePath = m_filePathEdit->text();
    }
    if (m_filePath.isEmpty()) return;

    m_previewRows.clear();
    m_mappings.clear();
    m_excelHeaders.clear();

    if (m_service->parsePreview(m_filePath, m_previewRows, m_mappings, m_excelHeaders)) {
        displayMappings();
        displayPreview();
        m_importBtn->setEnabled(true);
        m_statusLabel->setText(
            QString("预览: %1 行, %2 列 | %3")
                .arg(m_previewRows.size()).arg(m_excelHeaders.size()).arg(m_service->m_diagMsg));
    } else {
        m_statusLabel->setText("❌ 解析失败，请检查文件格式");
    }
}

void ImportDialog::displayMappings() {
    m_mappingTable->setRowCount(m_mappings.size());

    auto allCols = ColumnDao::getAll();
    for (int i = 0; i < m_mappings.size(); ++i) {
        const auto& m = m_mappings[i];

        auto* headerItem = new QTableWidgetItem(m.excelHeader);
        headerItem->setFlags(headerItem->flags() & ~Qt::ItemIsEditable);
        m_mappingTable->setItem(i, 0, headerItem);

        // Combo to select target column
        auto* combo = new QComboBox();
        combo->addItem("(跳过)", 0);
        for (const auto& col : allCols) {
            combo->addItem(col.displayNameWithUnit(), col.id);
        }
        if (m.isMapped) {
            int idx = combo->findData(m.columnDefId);
            if (idx >= 0) combo->setCurrentIndex(idx);
        }
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i](int) {
            onMappingChanged(i, 0);
        });
        m_mappingTable->setCellWidget(i, 1, combo);

        // Date column is handled specially, not through column_defs
        bool isDateCol = false;
        QString h = m.excelHeader;
        h.remove(QRegularExpression("[,，、。\\s]"));
        isDateCol = (h == "日期" || h == "date");

        auto* statusItem = new QTableWidgetItem(
            m.isMapped ? "✅ 已匹配" : (isDateCol ? "📅 日期列" : "⚠ 未匹配"));
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        m_mappingTable->setItem(i, 2, statusItem);
    }
}

void ImportDialog::displayPreview() {
    if (m_previewRows.isEmpty()) return;

    m_previewTable->setColumnCount(1 + m_excelHeaders.size());
    QStringList headers;
    headers << "日期";
    headers << m_excelHeaders;
    m_previewTable->setHorizontalHeaderLabels(headers);
    m_previewTable->setRowCount(m_previewRows.size());

    for (int r = 0; r < m_previewRows.size(); ++r) {
        const auto& row = m_previewRows[r];
        m_previewTable->setItem(r, 0, new QTableWidgetItem(row.date));

        for (int c = 0; c < m_excelHeaders.size(); ++c) {
            QString val;
            if (row.rawValues.contains(m_excelHeaders[c])) {
                double v = row.rawValues[m_excelHeaders[c]];
                // Show integer values without decimals
                if (v == static_cast<long long>(v))
                    val = QString::number(static_cast<long long>(v));
                else
                    val = QString::number(v, 'f', 2);
            }
            m_previewTable->setItem(r, c + 1, new QTableWidgetItem(val));
        }
    }

    for (int c = 0; c < m_previewTable->columnCount(); ++c) {
        m_previewTable->setColumnWidth(c, 100);
    }
}

void ImportDialog::onMappingChanged(int row, int col) {
    Q_UNUSED(col);
    auto* combo = qobject_cast<QComboBox*>(m_mappingTable->cellWidget(row, 1));
    if (!combo) return;

    int colDefId = combo->currentData().toInt();
    m_mappings[row].columnDefId = colDefId;
    m_mappings[row].isMapped = (colDefId > 0);
    m_mappings[row].matchedColumnKey = colDefId > 0
        ? ColumnDao::getById(colDefId).key : QString();

    m_mappingTable->item(row, 2)->setText(
        colDefId > 0 ? "✅ 已匹配" : "⚠ 跳过");
}

void ImportDialog::onExecuteImport() {
    // Validate entity selection
    int entityId = m_entityCombo->currentData().toInt();
    if (entityId <= 0) {
        QMessageBox::warning(this, "导入失败", "请选择要导入到哪个实体（公司/承包区）");
        return;
    }

    m_importBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText("正在导入...");

    // Update mappings from UI combo boxes
    int mappedCount = 0;
    for (int i = 0; i < m_mappings.size(); ++i) {
        auto* combo = qobject_cast<QComboBox*>(m_mappingTable->cellWidget(i, 1));
        if (combo) {
            int colDefId = combo->currentData().toInt();
            m_mappings[i].columnDefId = colDefId;
            m_mappings[i].isMapped = (colDefId > 0);
            if (colDefId > 0) mappedCount++;
        }
    }

    if (mappedCount == 0) {
        m_importBtn->setEnabled(true);
        m_progressBar->setVisible(false);
        QMessageBox::warning(this, "导入失败", "没有映射任何列，请先在列映射表中匹配Excel列到系统字段");
        return;
    }

    // Use direct import — reads file & matches headers independently
    auto result = m_service->executeImportDirect(m_filePath, entityId);

    m_progressBar->setVisible(false);
    m_importBtn->setEnabled(true);

    if (result.success) {
        m_statusLabel->setText(QString("✅ 完成: %1行, %2个值, %3跳过")
            .arg(result.successRows).arg(result.successRows * mappedCount).arg(result.skippedRows));
        QMessageBox::information(this, "导入完成",
            QString("实体: %1\n成功: %2 行 / 跳过: %3 行\n日期: %4 ~ %5\n\n%6")
                .arg(m_entityCombo->currentText())
                .arg(result.successRows).arg(result.skippedRows)
                .arg(FormatUtils::dateToString(result.startDate))
                .arg(FormatUtils::dateToString(result.endDate))
                .arg(result.verifyMsg));
        accept();
    } else {
        m_statusLabel->setText("❌ " + result.errorMessage);
        QMessageBox::warning(this, "导入失败", result.errorMessage);
    }
}
