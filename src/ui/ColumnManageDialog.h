#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include "model/ColumnDef.h"

class ColumnManageDialog : public QDialog {
    Q_OBJECT
public:
    explicit ColumnManageDialog(QWidget* parent = nullptr);

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onMoveUp();
    void onMoveDown();
    void onTableSelectionChanged();
    void onSave();

private:
    void setupUI();
    void refreshTable();
    void populateForm(const ColumnDef& c);
    void clearForm();

    // Table
    QTableWidget* m_table = nullptr;

    // Buttons
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_upBtn = nullptr;
    QPushButton* m_downBtn = nullptr;

    // Edit form
    QLineEdit* m_keyEdit = nullptr;
    QLineEdit* m_displayEdit = nullptr;
    QComboBox* m_dataTypeCombo = nullptr;
    QLineEdit* m_unitEdit = nullptr;
    QComboBox* m_entityTypeCombo = nullptr;
    QComboBox* m_aggTypeCombo = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QCheckBox* m_coreCheck = nullptr;
    QPushButton* m_saveBtn = nullptr;

    int m_editingId = 0;
};
