#include "EntityManageDialog.h"
#include "db/EntityDao.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>

EntityManageDialog::EntityManageDialog(QWidget* parent)
    : QDialog(parent) {
    setupUI();
    refreshTree();
    setWindowTitle("实体管理 — 公司 / 承包区");
    resize(700, 500);
}

void EntityManageDialog::setupUI() {
    auto* mainLayout = new QHBoxLayout(this);

    // Left: tree
    auto* leftPanel = new QVBoxLayout();
    leftPanel->addWidget(new QLabel("<b>实体列表</b>"));

    m_tree = new QTreeWidget();
    m_tree->setHeaderLabels({"名称", "类型", "状态"});
    m_tree->setColumnWidth(0, 220);
    m_tree->setColumnWidth(1, 70);
    m_tree->setRootIsDecorated(true);
    leftPanel->addWidget(m_tree);

    auto* btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("➕ 新增");
    m_editBtn = new QPushButton("✏️ 编辑");
    m_deleteBtn = new QPushButton("🗑 删除");
    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_editBtn);
    btnLayout->addWidget(m_deleteBtn);
    leftPanel->addLayout(btnLayout);

    mainLayout->addLayout(leftPanel, 2);

    // Right: edit form
    auto* rightPanel = new QGroupBox("编辑实体");
    auto* form = new QFormLayout(rightPanel);

    m_typeCombo = new QComboBox();
    auto types = EntityDao::getTypeNames();
    m_typeCombo->addItems(types);
    form->addRow("类型:", m_typeCombo);

    m_parentCombo = new QComboBox();
    m_parentCombo->addItem("(无 — 顶级)", 0);
    form->addRow("上级公司:", m_parentCombo);

    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("输入实体名称");
    form->addRow("名称:", m_nameEdit);

    m_saveBtn = new QPushButton("💾 保存");
    form->addRow("", m_saveBtn);

    mainLayout->addWidget(rightPanel, 1);

    // Connections
    connect(m_addBtn, &QPushButton::clicked, this, &EntityManageDialog::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &EntityManageDialog::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &EntityManageDialog::onDelete);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &EntityManageDialog::onTreeSelectionChanged);
    connect(m_saveBtn, &QPushButton::clicked, this, &EntityManageDialog::onSaveNew);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        // Only show parent combo for contractors
        m_parentCombo->setVisible(m_typeCombo->currentText() == "承包区");
    });
}

void EntityManageDialog::refreshTree() {
    m_tree->clear();
    m_parentCombo->clear();
    m_parentCombo->addItem("(无 — 顶级)", 0);

    auto companies = EntityDao::getCompanies();
    for (const auto& comp : companies) {
        auto* compItem = new QTreeWidgetItem(m_tree);
        compItem->setText(0, comp.name);
        compItem->setText(1, "公司");
        compItem->setText(2, comp.isActive ? "启用" : "停用");
        compItem->setData(0, Qt::UserRole, comp.id);
        compItem->setData(0, Qt::UserRole + 1, 1); // type id

        m_parentCombo->addItem(comp.name, comp.id);

        auto contractors = EntityDao::getContractorsByCompany(comp.id);
        for (const auto& c : contractors) {
            auto* childItem = new QTreeWidgetItem(compItem);
            childItem->setText(0, c.name);
            childItem->setText(1, "承包区");
            childItem->setText(2, c.isActive ? "启用" : "停用");
            childItem->setData(0, Qt::UserRole, c.id);
            childItem->setData(0, Qt::UserRole + 1, 2);
        }

        compItem->setExpanded(true);
    }
}

void EntityManageDialog::onTreeSelectionChanged() {
    auto items = m_tree->selectedItems();
    bool hasSel = !items.isEmpty();
    m_editBtn->setEnabled(hasSel);
    m_deleteBtn->setEnabled(hasSel);

    if (hasSel) {
        auto* item = items.first();
        int id = item->data(0, Qt::UserRole).toInt();
        int typeId = item->data(0, Qt::UserRole + 1).toInt();
        auto e = EntityDao::getById(id);
        populateForm(e);
    }
}

void EntityManageDialog::populateForm(const Entity& e) {
    m_editingId = e.id;
    m_typeCombo->setCurrentText(e.typeName);
    if (e.parentId > 0) {
        int idx = m_parentCombo->findData(e.parentId);
        if (idx >= 0) m_parentCombo->setCurrentIndex(idx);
    } else {
        m_parentCombo->setCurrentIndex(0);
    }
    m_parentCombo->setVisible(e.typeName == "承包区");
    m_nameEdit->setText(e.name);
}

void EntityManageDialog::clearForm() {
    m_editingId = 0;
    m_nameEdit->clear();
    m_typeCombo->setCurrentIndex(0);
    m_parentCombo->setCurrentIndex(0);
}

void EntityManageDialog::onAdd() {
    clearForm();
    m_nameEdit->setFocus();
}

void EntityManageDialog::onEdit() {
    // Already populated via selection
    m_nameEdit->setFocus();
}

void EntityManageDialog::onDelete() {
    auto items = m_tree->selectedItems();
    if (items.isEmpty()) return;

    auto* item = items.first();
    int id = item->data(0, Qt::UserRole).toInt();
    QString name = item->text(0);
    QString type = item->text(1);

    QString msg = (type == "公司")
        ? QString("删除公司「%1」将同时删除其下所有承包区及历史数据，确定吗？").arg(name)
        : QString("确定删除 %1「%2」及其历史数据吗？").arg(type, name);

    if (QMessageBox::question(this, "确认删除", msg) != QMessageBox::Yes) return;

    if (EntityDao::remove(id)) {
        refreshTree();
        clearForm();
    } else {
        QMessageBox::critical(this, "错误", "删除失败");
    }
}

void EntityManageDialog::onSaveNew() {
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入名称");
        return;
    }

    Entity e;
    if (m_editingId > 0) {
        e = EntityDao::getById(m_editingId);
    } else {
        e.isActive = true; // new entities default to active
    }

    QString typeName = m_typeCombo->currentText();
    e.typeId = (typeName == "公司") ? 1 : 2;

    if (typeName == "承包区") {
        e.parentId = m_parentCombo->currentData().toInt();
        if (e.parentId <= 0) {
            QMessageBox::warning(this, "提示", "承包区必须指定上级公司");
            return;
        }
    } else {
        e.parentId = 0;
    }

    e.name = name;

    bool ok = false;
    if (m_editingId > 0) {
        ok = EntityDao::update(e);
    } else {
        ok = EntityDao::insert(e);
    }

    if (ok) {
        refreshTree();
        clearForm();
    } else {
        QMessageBox::critical(this, "错误", "保存失败（名称可能重复）");
    }
}
