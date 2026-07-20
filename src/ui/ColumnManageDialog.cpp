#include "ColumnManageDialog.h"
#include "db/ColumnDao.h"
#include "db/EntityDao.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>

ColumnManageDialog::ColumnManageDialog(QWidget* parent)
    : QDialog(parent) {
    setupUI();
    refreshTable();
    setWindowTitle("列管理 — 字段定义");
    resize(900, 600);
}

void ColumnManageDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // Table
    m_table = new QTableWidget();
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels({
        "Key", "显示名", "数据类型", "单位", "适用类型",
        "汇总方式", "分类", "核心", "排序"
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 130);
    m_table->setColumnWidth(1, 130);
    m_table->setColumnWidth(2, 80);
    m_table->setColumnWidth(3, 50);
    m_table->setColumnWidth(4, 80);
    m_table->setColumnWidth(5, 80);
    m_table->setColumnWidth(6, 50);
    mainLayout->addWidget(m_table, 1);

    // Buttons row
    auto* btnRow = new QHBoxLayout();
    m_addBtn = new QPushButton("➕ 新增");
    m_editBtn = new QPushButton("✏️ 编辑");
    m_deleteBtn = new QPushButton("🗑 删除");
    m_upBtn = new QPushButton("🔼 上移");
    m_downBtn = new QPushButton("🔽 下移");
    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_upBtn->setEnabled(false);
    m_downBtn->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addWidget(m_upBtn);
    btnRow->addWidget(m_downBtn);
    btnRow->addStretch();
    mainLayout->addLayout(btnRow);

    // Edit form
    auto* editGroup = new QGroupBox("新增 / 编辑列");
    auto* form = new QFormLayout(editGroup);

    m_keyEdit = new QLineEdit();
    m_keyEdit->setPlaceholderText("英文标识，如 outbound");
    form->addRow("内部Key:", m_keyEdit);

    m_displayEdit = new QLineEdit();
    m_displayEdit->setPlaceholderText("中文显示名，如 出港量");
    form->addRow("显示名:", m_displayEdit);

    m_dataTypeCombo = new QComboBox();
    m_dataTypeCombo->addItems({"INTEGER", "REAL", "TEXT"});
    form->addRow("数据类型:", m_dataTypeCombo);

    m_unitEdit = new QLineEdit();
    m_unitEdit->setPlaceholderText("票 / % / 元 / ppm");
    form->addRow("单位:", m_unitEdit);

    m_entityTypeCombo = new QComboBox();
    m_entityTypeCombo->addItem("全部类型", 0);
    auto types = EntityDao::getTypeNames();
    int typeId = 1;
    for (const auto& t : types) {
        m_entityTypeCombo->addItem(t, typeId++);
    }
    form->addRow("适用类型:", m_entityTypeCombo);

    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItems({"", "业务", "客服", "操作", "小件员取派签质量", "运营"});
    m_categoryCombo->setEditable(true);
    form->addRow("分类:", m_categoryCombo);

    m_aggTypeCombo = new QComboBox();
    m_aggTypeCombo->addItems({"NONE", "SUM", "AVG"});
    m_aggTypeCombo->setCurrentText("NONE");
    m_aggTypeCombo->setToolTip(
        "NONE=手动录入 | SUM=子承包区求和 | AVG=子承包区平均");
    form->addRow("汇总方式:", m_aggTypeCombo);

    m_coreCheck = new QCheckBox("核心指标（默认在表格中显示）");
    m_coreCheck->setChecked(true);
    form->addRow("", m_coreCheck);

    m_saveBtn = new QPushButton("💾 保存");
    form->addRow("", m_saveBtn);

    mainLayout->addWidget(editGroup);

    // Connections
    connect(m_addBtn, &QPushButton::clicked, this, &ColumnManageDialog::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &ColumnManageDialog::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ColumnManageDialog::onDelete);
    connect(m_upBtn, &QPushButton::clicked, this, &ColumnManageDialog::onMoveUp);
    connect(m_downBtn, &QPushButton::clicked, this, &ColumnManageDialog::onMoveDown);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ColumnManageDialog::onTableSelectionChanged);
    connect(m_saveBtn, &QPushButton::clicked, this, &ColumnManageDialog::onSave);
}

void ColumnManageDialog::refreshTable() {
    auto cols = ColumnDao::getAll();
    m_table->setRowCount(cols.size());

    for (int i = 0; i < cols.size(); ++i) {
        const auto& c = cols[i];
        m_table->setItem(i, 0, new QTableWidgetItem(c.key));
        m_table->setItem(i, 1, new QTableWidgetItem(c.displayName));
        m_table->setItem(i, 2, new QTableWidgetItem(c.dataType));
        m_table->setItem(i, 3, new QTableWidgetItem(c.unit));
        m_table->setItem(i, 4, new QTableWidgetItem(
            c.entityTypeId == 0 ? "全部" : QString::number(c.entityTypeId)));
        m_table->setItem(i, 5, new QTableWidgetItem(c.aggregateType));
        m_table->setItem(i, 6, new QTableWidgetItem(c.category));
        m_table->setItem(i, 7, new QTableWidgetItem(c.isCore ? "✓" : ""));
        m_table->setItem(i, 8, new QTableWidgetItem(QString::number(c.sortOrder)));

        // Store column id in first cell
        m_table->item(i, 0)->setData(Qt::UserRole, c.id);
    }
}

void ColumnManageDialog::populateForm(const ColumnDef& c) {
    m_editingId = c.id;
    m_keyEdit->setText(c.key);
    m_displayEdit->setText(c.displayName);
    m_dataTypeCombo->setCurrentText(c.dataType);
    m_unitEdit->setText(c.unit);
    m_aggTypeCombo->setCurrentText(c.aggregateType);
    int catIdx = m_categoryCombo->findText(c.category);
    if (catIdx >= 0) m_categoryCombo->setCurrentIndex(catIdx);
    else m_categoryCombo->setEditText(c.category);
    m_coreCheck->setChecked(c.isCore);

    int idx = m_entityTypeCombo->findData(c.entityTypeId);
    if (idx >= 0) m_entityTypeCombo->setCurrentIndex(idx);
    else m_entityTypeCombo->setCurrentIndex(0);
}

void ColumnManageDialog::clearForm() {
    m_editingId = 0;
    m_keyEdit->clear();
    m_displayEdit->clear();
    m_dataTypeCombo->setCurrentIndex(0);
    m_unitEdit->clear();
    m_entityTypeCombo->setCurrentIndex(0);
    m_aggTypeCombo->setCurrentText("NONE");
    m_coreCheck->setChecked(true);
}

void ColumnManageDialog::onTableSelectionChanged() {
    bool has = !m_table->selectedItems().isEmpty();
    m_editBtn->setEnabled(has);
    m_deleteBtn->setEnabled(has);
    m_upBtn->setEnabled(has);
    m_downBtn->setEnabled(has);

    if (has) {
        int row = m_table->currentRow();
        int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
        populateForm(ColumnDao::getById(id));
    }
}

void ColumnManageDialog::onAdd() {
    clearForm();
    m_keyEdit->setFocus();
}

void ColumnManageDialog::onEdit() {
    // Already populated via selection
    m_keyEdit->setFocus();
}

void ColumnManageDialog::onDelete() {
    int row = m_table->currentRow();
    if (row < 0) return;
    int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    QString key = m_table->item(row, 0)->text();

    if (QMessageBox::question(this, "确认删除",
            QString("删除列「%1」将同时删除所有相关数据，确定吗？").arg(key))
        != QMessageBox::Yes) return;

    if (ColumnDao::remove(id)) {
        refreshTable();
        clearForm();
    } else {
        QMessageBox::critical(this, "错误", "删除失败，该列可能被数据引用");
    }
}

void ColumnManageDialog::onMoveUp() {
    int row = m_table->currentRow();
    if (row <= 0) return;
    int id1 = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    int id2 = m_table->item(row - 1, 0)->data(Qt::UserRole).toInt();
    // Swap sort orders
    auto c1 = ColumnDao::getById(id1);
    auto c2 = ColumnDao::getById(id2);
    std::swap(c1.sortOrder, c2.sortOrder);
    ColumnDao::update(c1);
    ColumnDao::update(c2);
    refreshTable();
    m_table->selectRow(row - 1);
}

void ColumnManageDialog::onMoveDown() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount() - 1) return;
    int id1 = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    int id2 = m_table->item(row + 1, 0)->data(Qt::UserRole).toInt();
    auto c1 = ColumnDao::getById(id1);
    auto c2 = ColumnDao::getById(id2);
    std::swap(c1.sortOrder, c2.sortOrder);
    ColumnDao::update(c1);
    ColumnDao::update(c2);
    refreshTable();
    m_table->selectRow(row + 1);
}

void ColumnManageDialog::onSave() {
    QString key = m_keyEdit->text().trimmed();
    QString display = m_displayEdit->text().trimmed();
    if (key.isEmpty() || display.isEmpty()) {
        QMessageBox::warning(this, "提示", "Key 和显示名不能为空");
        return;
    }

    ColumnDef c;
    if (m_editingId > 0) {
        c = ColumnDao::getById(m_editingId);
    }

    c.key = key;
    c.displayName = display;
    c.dataType = m_dataTypeCombo->currentText();
    c.unit = m_unitEdit->text().trimmed();
    c.entityTypeId = m_entityTypeCombo->currentData().toInt();
    c.aggregateType = m_aggTypeCombo->currentText();
    c.category = m_categoryCombo->currentText().trimmed();
    c.isCore = m_coreCheck->isChecked();

    bool ok;
    if (m_editingId > 0) {
        ok = ColumnDao::update(c);
    } else {
        auto all = ColumnDao::getAll();
        c.sortOrder = all.isEmpty() ? 0 : all.last().sortOrder + 1;
        ok = ColumnDao::insert(c);
    }

    if (ok) {
        refreshTable();
        clearForm();
    } else {
        QMessageBox::critical(this, "错误", "保存失败（Key 可能重复）");
    }
}
