#pragma once
#include <QMainWindow>
#include <QTableView>
#include <QDateEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include "model/DisplayDataModel.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    // File
    void onImportExcel();
    void onExportExcel();
    void onExportCsv();

    // Edit
    void onAddData();
    void onEditData();
    void onDeleteData();
    void onCopyPreviousDay();

    // Data
    void onRefresh();

    // View
    void onShowTrendChart();
    void onShowCompareChart();
    void onFilterCompany();
    void onFilterContractor();
    void onFilterAll();
    void onSelectVisibleColumns();

    // Manage
    void onManageEntities();
    void onManageColumns();

    // Date
    void onDateRangeChanged();
    void onQuickJump(int index);

    // Table interaction
    void onTableDoubleClicked(const QModelIndex& index);
    void onTableContextMenu(const QPoint& pos);

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupSidebar();
    void setupStatusBar();
    void setupTable();
    void setupConnections();
    void loadData();
    void updateStatusBar();
    void onCategoryClicked(const QString& category);
    void updateSidebarHighlight(const QString& activeCategory);

    // Core widgets
    QTableView* m_tableView = nullptr;
    DisplayDataModel* m_model = nullptr;

    // Toolbar
    QDateEdit* m_startDateEdit = nullptr;
    QDateEdit* m_endDateEdit = nullptr;
    QComboBox* m_quickJumpCombo = nullptr;

    // Actions / Buttons
    QAction* m_actImport = nullptr;
    QAction* m_actExport = nullptr;
    QAction* m_actAdd = nullptr;
    QAction* m_actEdit = nullptr;
    QAction* m_actDelete = nullptr;
    QAction* m_actRefresh = nullptr;

    // Status bar
    QLabel* m_statusLabel = nullptr;
    QLabel* m_countLabel = nullptr;
    QLabel* m_dbLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;

    // State
    int m_filterEntityType = 0;
    QString m_activeCategory;  // current category filter ("" = all)

    // Sidebar
    QVBoxLayout* m_sidebarLayout = nullptr;
    QVector<QPushButton*> m_sidebarBtns;
};
