#pragma once
#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include "model/Entity.h"

class EntityManageDialog : public QDialog {
    Q_OBJECT
public:
    explicit EntityManageDialog(QWidget* parent = nullptr);

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onTreeSelectionChanged();
    void onSaveNew();

private:
    void setupUI();
    void refreshTree();
    void populateForm(const Entity& e);
    void clearForm();

    // Tree
    QTreeWidget* m_tree = nullptr;

    // Buttons
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;

    // Edit form
    QComboBox* m_typeCombo = nullptr;
    QComboBox* m_parentCombo = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QPushButton* m_saveBtn = nullptr;

    // State
    int m_editingId = 0; // 0 = new
};
