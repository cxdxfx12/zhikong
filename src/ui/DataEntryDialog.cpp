#include "DataEntryDialog.h"
#include "db/EntityDao.h"
#include "db/ColumnDao.h"
#include "db/DailyDao.h"
#include "service/AggregationService.h"
#include "utils/FormatUtils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

DataEntryDialog::DataEntryDialog(QWidget* parent)
    : QDialog(parent) {
    m_allEntities = EntityDao::getAll();
    setupUI();
    setWindowTitle("录入 / 编辑数据");
    setMinimumSize(600, 500);
}

void DataEntryDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // ---- Top bar ----
    auto* topBar = new QHBoxLayout();

    topBar->addWidget(new QLabel("日期:"));
    m_dateEdit = new QDateEdit(QDate::currentDate());
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");
    topBar->addWidget(m_dateEdit);

    topBar->addWidget(new QLabel("类型:"));
    m_typeCombo = new QComboBox();
    auto typeNames = EntityDao::getTypeNames();
    m_typeCombo->addItems(typeNames);
    topBar->addWidget(m_typeCombo);

    topBar->addWidget(new QLabel("实体:"));
    m_entityCombo = new QComboBox();
    m_entityCombo->setMinimumWidth(150);
    topBar->addWidget(m_entityCombo);

    topBar->addStretch();
    mainLayout->addLayout(topBar);

    // ---- Dynamic form area ----
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_formWidget = new QWidget();
    m_formLayout = new QFormLayout(m_formWidget);
    m_formLayout->setLabelAlignment(Qt::AlignRight);
    m_scrollArea->setWidget(m_formWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    // ---- Bottom buttons ----
    auto* bottomBar = new QHBoxLayout();
    m_copyPrevBtn = new QPushButton("📋 复制前一天数据");
    bottomBar->addWidget(m_copyPrevBtn);
    bottomBar->addStretch();

    m_saveBtn = new QPushButton("💾 保存");
    m_saveBtn->setDefault(true);
    bottomBar->addWidget(m_saveBtn);

    auto* cancelBtn = new QPushButton("取消");
    bottomBar->addWidget(cancelBtn);
    mainLayout->addLayout(bottomBar);

    // Connections
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataEntryDialog::onTypeOrEntityChanged);
    connect(m_entityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataEntryDialog::onTypeOrEntityChanged);
    connect(m_saveBtn, &QPushButton::clicked, this, &DataEntryDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_copyPrevBtn, &QPushButton::clicked, this, &DataEntryDialog::onCopyPreviousDay);

    // Initialize entity combo
    onTypeOrEntityChanged();
}

void DataEntryDialog::setMode(Mode mode) {
    m_mode = mode;
    if (mode == ModeEdit) {
        m_typeCombo->setEnabled(false);
        m_entityCombo->setEnabled(false);
    }
}

void DataEntryDialog::setDate(const QDate& date) {
    m_dateEdit->setDate(date);
}

void DataEntryDialog::setEditTarget(int entityId, const QString& entityType, const QDate& date) {
    m_mode = ModeEdit;
    m_editEntityId = entityId;
    m_dateEdit->setDate(date);

    // Set type
    int typeIdx = m_typeCombo->findText(entityType);
    if (typeIdx >= 0) {
        m_typeCombo->setCurrentIndex(typeIdx);
        m_typeCombo->setEnabled(false);
    }

    // Set entity
    for (int i = 0; i < m_entityCombo->count(); ++i) {
        if (m_entityCombo->itemData(i).toInt() == entityId) {
            m_entityCombo->setCurrentIndex(i);
            m_entityCombo->setEnabled(false);
            break;
        }
    }

    // Load existing values
    loadExistingData(entityId, date);
}

void DataEntryDialog::onTypeOrEntityChanged() {
    // Guard against recursive signal firing during clear/addItem
    if (m_updatingUI) return;
    m_updatingUI = true;

    // Populate entity combo based on selected type
    QString typeName = m_typeCombo->currentText();
    int entityTypeId = 0;
    if (typeName == "公司") entityTypeId = 1;
    else if (typeName == "承包区") entityTypeId = 2;

    // Block signals on entity combo while clearing/repopulating
    m_entityCombo->blockSignals(true);
    m_entityCombo->clear();

    if (entityTypeId == 1) {
        auto companies = EntityDao::getCompanies();
        for (const auto& e : companies) {
            m_entityCombo->addItem(e.name, e.id);
        }
    } else if (entityTypeId == 2) {
        auto tree = EntityDao::getTree();
        for (const auto& companyNode : tree) {
            for (const auto& contractor : companyNode.children) {
                m_entityCombo->addItem(contractor.entity.fullPath(), contractor.entity.id);
            }
        }
    }
    m_entityCombo->blockSignals(false);

    // Rebuild form for this entity type
    rebuildForm(entityTypeId);

    m_updatingUI = false;
}

// Helper to create an editor widget for a column type
static QWidget* createEditor(const ColumnDef& col) {
    if (col.dataType == "INTEGER") {
        auto* spin = new QSpinBox();
        spin->setRange(-99999999, 99999999);
        spin->setSingleStep(1);
        return spin;
    } else if (col.dataType == "REAL") {
        auto* spin = new QDoubleSpinBox();
        spin->setRange(-99999999.99, 99999999.99);
        spin->setDecimals(2);
        spin->setSingleStep(0.01);
        return spin;
    }
    return new QLineEdit();
}

void DataEntryDialog::rebuildForm(int entityTypeId) {
    clearForm();

    m_columns = ColumnDao::getByEntityType(entityTypeId);

    // Group columns by category, preserving sort order
    QStringList orderedCategories = {"业务", "客服", "操作", "小件员取派签质量", "运营"};
    QMap<QString, QVector<ColumnDef>> grouped;
    for (const auto& col : m_columns) {
        QString cat = col.category.isEmpty() ? "其他" : col.category;
        grouped[cat].append(col);
    }

    for (const auto& catName : orderedCategories) {
        if (!grouped.contains(catName) || grouped[catName].isEmpty())
            continue;

        // Category header
        auto* header = new QLabel("▎" + catName);
        header->setStyleSheet(
            "font-weight: bold; font-size: 14px; color: #2c3e50;"
            "padding: 8px 0 4px 0; margin-top: 6px;"
            "border-bottom: 2px solid #3498db;"
        );
        m_formLayout->addRow(header);

        for (const auto& col : grouped[catName]) {
            QWidget* editor = createEditor(col);

            if (col.needsAggregation()) {
                editor->setToolTip(QString("汇总方式: %1 (公司级数据自动计算)")
                    .arg(col.aggregateType == "SUM" ? "求和" : "平均"));
            }

            m_editorMap[col.id] = editor;
            m_formLayout->addRow(col.displayNameWithUnit(), editor);
        }
    }

    // Any uncategorized columns
    if (grouped.contains("其他") && !grouped["其他"].isEmpty()) {
        auto* header = new QLabel("▎其他");
        header->setStyleSheet("font-weight: bold; font-size: 14px; color: #2c3e50; padding: 8px 0 4px 0; margin-top: 6px; border-bottom: 2px solid #95a5a6;");
        m_formLayout->addRow(header);
        for (const auto& col : grouped["其他"]) {
            QWidget* editor = createEditor(col);
            m_editorMap[col.id] = editor;
            m_formLayout->addRow(col.displayNameWithUnit(), editor);
        }
    }
}

void DataEntryDialog::clearForm() {
    m_editorMap.clear();
    m_columns.clear();
    // Properly remove and delete all form rows
    while (m_formLayout->rowCount() > 0) {
        // Take the row at 0 — removeRow deletes the row but may not delete widgets
        QLayoutItem* labelItem = m_formLayout->itemAt(0, QFormLayout::LabelRole);
        QLayoutItem* fieldItem = m_formLayout->itemAt(0, QFormLayout::FieldRole);
        m_formLayout->removeRow(0);
        // Delete the widgets manually to avoid leaks
        if (labelItem && labelItem->widget()) {
            labelItem->widget()->deleteLater();
        }
        if (fieldItem && fieldItem->widget()) {
            fieldItem->widget()->deleteLater();
        }
    }
}

void DataEntryDialog::loadExistingData(int entityId, const QDate& date) {
    // For each column, try to load the existing value
    for (const auto& col : m_columns) {
        if (!m_editorMap.contains(col.id)) continue;

        DailyValue dv;
        if (DailyDao::getValue(entityId, FormatUtils::dateToString(date), col.id, dv)) {
            QWidget* editor = m_editorMap[col.id];
            if (col.dataType == "INTEGER") {
                qobject_cast<QSpinBox*>(editor)->setValue(static_cast<int>(dv.value));
            } else if (col.dataType == "REAL") {
                qobject_cast<QDoubleSpinBox*>(editor)->setValue(dv.value);
            } else {
                qobject_cast<QLineEdit*>(editor)->setText(QString::number(dv.value));
            }
        }
    }
}

void DataEntryDialog::populateFieldsFromPreviousDay(int entityId, const QDate& prevDate) {
    for (const auto& col : m_columns) {
        if (!m_editorMap.contains(col.id)) continue;

        DailyValue dv;
        if (DailyDao::getValue(entityId, FormatUtils::dateToString(prevDate), col.id, dv)) {
            QWidget* editor = m_editorMap[col.id];
            if (col.dataType == "INTEGER") {
                qobject_cast<QSpinBox*>(editor)->setValue(static_cast<int>(dv.value));
            } else if (col.dataType == "REAL") {
                qobject_cast<QDoubleSpinBox*>(editor)->setValue(dv.value);
            } else {
                qobject_cast<QLineEdit*>(editor)->setText(QString::number(dv.value));
            }
        }
    }
}

void DataEntryDialog::onSave() {
    if (!m_dateEdit->date().isValid()) {
        QMessageBox::warning(this, "提示", "请选择有效日期");
        return;
    }

    int entityId = m_entityCombo->currentData().toInt();
    if (entityId <= 0) {
        QMessageBox::warning(this, "提示", "请选择实体");
        return;
    }

    QString dateStr = FormatUtils::dateToString(m_dateEdit->date());
    QVector<DailyValue> values;

    for (const auto& col : m_columns) {
        if (!m_editorMap.contains(col.id)) continue;

        double val = 0;
        QWidget* editor = m_editorMap[col.id];

        if (col.dataType == "INTEGER") {
            val = qobject_cast<QSpinBox*>(editor)->value();
        } else if (col.dataType == "REAL") {
            val = qobject_cast<QDoubleSpinBox*>(editor)->value();
        } else {
            val = qobject_cast<QLineEdit*>(editor)->text().toDouble();
        }

        DailyValue dv;
        dv.entityId = entityId;
        dv.reportDate = dateStr;
        dv.columnId = col.id;
        dv.value = val;
        values.append(dv);
    }

    if (!DailyDao::saveValues(values)) {
        QMessageBox::critical(this, "错误", "保存数据失败");
        return;
    }

    // If this is a contractor, re-aggregate parent company
    auto entity = EntityDao::getById(entityId);
    if (entity.isContractor() && entity.parentId > 0) {
        AggregationService::regenerateCompanyData(entity.parentId,
            FormatUtils::stringToDate(dateStr));
    }

    emit dataSaved();
    accept();
}

void DataEntryDialog::onCopyPreviousDay() {
    int entityId = m_entityCombo->currentData().toInt();
    if (entityId <= 0) {
        QMessageBox::warning(this, "提示", "请先选择实体");
        return;
    }

    QDate prevDate = m_dateEdit->date().addDays(-1);
    if (!prevDate.isValid()) return;

    populateFieldsFromPreviousDay(entityId, prevDate);
}
