#include "MainWindow.h"
#include "ui/DataEntryDialog.h"
#include "ui/EntityManageDialog.h"
#include "ui/ColumnManageDialog.h"
#include "ui/ImportDialog.h"
#include "ui/ChartDialog.h"
#include "db/Database.h"
#include "db/EntityDao.h"
#include "db/DailyDao.h"
#include "service/AggregationService.h"
#include "service/ExportService.h"
#include "utils/FormatUtils.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QShortcut>
#include <QLabel>
#include <QButtonGroup>
#include <QActionGroup>
#include <QSqlQuery>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupTable();
    setupStatusBar();
    setupConnections();

    // Load recent 7 days by default, or today if DB is empty
    QDate latest = DailyDao::getLatestDate();
    if (!latest.isValid()) latest = QDate::currentDate();
    m_startDateEdit->setDate(latest.addDays(-7));
    m_endDateEdit->setDate(latest);
    loadData();

    setWindowTitle("快递日报数据管理系统");
    resize(1280, 720);
}

// =========================================================================
// UI Setup
// =========================================================================
void MainWindow::setupUI() {
    QWidget* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setCentralWidget(central);
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu("文件(&F)");

    m_actImport = fileMenu->addAction("导入 Excel...(&I)");
    m_actImport->setShortcut(QKeySequence("Ctrl+I"));

    m_actExport = fileMenu->addAction("导出 Excel...(&E)");
    m_actExport->setShortcut(QKeySequence("Ctrl+E"));

    fileMenu->addAction("导出 CSV...")->setShortcut(QKeySequence("Ctrl+Shift+E"));
    connect(fileMenu->actions().last(), &QAction::triggered, this, &MainWindow::onExportCsv);

    fileMenu->addSeparator();
    fileMenu->addAction("退出(&Q)")->setShortcut(QKeySequence("Alt+F4"));
    connect(fileMenu->actions().last(), &QAction::triggered, this, &QMainWindow::close);

    // Edit
    auto* editMenu = menuBar()->addMenu("编辑(&E)");

    m_actAdd = editMenu->addAction("新增数据(&N)");
    m_actAdd->setShortcut(QKeySequence("Ctrl+N"));

    m_actEdit = editMenu->addAction("编辑选中行(&E)");
    m_actEdit->setShortcut(QKeySequence("Ctrl+Enter"));

    m_actDelete = editMenu->addAction("删除选中行(&D)");
    m_actDelete->setShortcut(QKeySequence("Delete"));

    editMenu->addSeparator();
    editMenu->addAction("复制前一天数据")->setShortcut(QKeySequence("Ctrl+D"));
    connect(editMenu->actions().last(), &QAction::triggered, this, &MainWindow::onCopyPreviousDay);

    // Data
    auto* dataMenu = menuBar()->addMenu("数据(&D)");

    m_actRefresh = dataMenu->addAction("刷新(&R)");
    m_actRefresh->setShortcut(QKeySequence("F5"));
    dataMenu->addAction("清空筛选");
    connect(dataMenu->actions().last(), &QAction::triggered, this, [this]() {
        m_startDateEdit->setDate(DailyDao::getEarliestDate());
        m_endDateEdit->setDate(DailyDao::getLatestDate());
        m_filterEntityType = 0;
        m_activeCategory.clear();
        m_model->setCategoryFilter("");
        updateSidebarHighlight("");
        // Re-check "全部" button
        for(auto* b:m_sidebarBtns) b->setChecked(b->text()=="全部");
        loadData();
    });

    // View
    auto* viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction("趋势图表...(&G)")->setShortcut(QKeySequence("Ctrl+G"));
    connect(viewMenu->actions().last(), &QAction::triggered, this, &MainWindow::onShowTrendChart);

    viewMenu->addAction("对比图表...")->setShortcut(QKeySequence("Ctrl+Shift+G"));
    connect(viewMenu->actions().last(), &QAction::triggered, this, &MainWindow::onShowCompareChart);

    viewMenu->addSeparator();
    auto* filterGroup = new QActionGroup(this);
    filterGroup->setExclusive(true);

    auto* actCompany = viewMenu->addAction("仅显示公司"); actCompany->setCheckable(true);
    filterGroup->addAction(actCompany);
    connect(actCompany, &QAction::triggered, this, &MainWindow::onFilterCompany);

    auto* actContractor = viewMenu->addAction("仅显示承包区"); actContractor->setCheckable(true);
    filterGroup->addAction(actContractor);
    connect(actContractor, &QAction::triggered, this, &MainWindow::onFilterContractor);

    viewMenu->addAction("显示全部");
    connect(viewMenu->actions().last(), &QAction::triggered, this, &MainWindow::onFilterAll);

    viewMenu->addSeparator();
    auto* statsAct = viewMenu->addAction("显示统计行");
    statsAct->setCheckable(true);
    statsAct->setChecked(true);
    connect(statsAct, &QAction::toggled, this, [this](bool on) { m_model->setShowStats(on); });

    viewMenu->addAction("选择显示的列...");
    connect(viewMenu->actions().last(), &QAction::triggered, this, &MainWindow::onSelectVisibleColumns);

    // Manage
    auto* mgrMenu = menuBar()->addMenu("管理(&M)");
    mgrMenu->addAction("实体管理... (公司/承包区)");
    connect(mgrMenu->actions().last(), &QAction::triggered, this, &MainWindow::onManageEntities);

    mgrMenu->addAction("列管理... (字段定义)");
    connect(mgrMenu->actions().last(), &QAction::triggered, this, &MainWindow::onManageColumns);

    // Help
    auto* helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("检查数据库...");
    connect(helpMenu->actions().last(), &QAction::triggered, this, [this]() {
        QStringList lines;

        // -- DB data (first 10 rows) --
        auto& db = Database::instance();
        QSqlQuery q(db.db());
        q.exec("SELECT dv.id, e.name, dv.report_date, cd.display_name, dv.value FROM daily_values dv JOIN entities e ON dv.entity_id=e.id JOIN column_defs cd ON dv.column_id=cd.id ORDER BY dv.report_date DESC, e.name, cd.sort_order LIMIT 20");
        lines << "=== 数据库 (前20条) ===";
        while (q.next()) {
            lines << QString("[%1] %2 | %3 | %4 = %5")
                .arg(q.value(0).toInt())
                .arg(q.value(1).toString())
                .arg(q.value(2).toString())
                .arg(q.value(3).toString())
                .arg(q.value(4).toDouble());
        }
        if (lines.size() == 1) lines << "(空)";

        // -- Model data (first 3 rows) --
        lines << "";
        lines << "=== 表模型 (前3行) ===";
        if (m_model->rowCount() > 0) {
            auto cols = m_model->displayedColumns();
            lines << QString("共 %1 行, %2 列").arg(m_model->rowCount()).arg(cols.size());
            for (int r = 0; r < qMin(3, m_model->rowCount()); ++r) {
                auto row = m_model->rowAt(r);
                QStringList vals;
                for (int c = 0; c < qMin(8, cols.size()); ++c) {
                    double v = row.values.value(cols[c].id, -999999);
                    vals << QString("[%1]%2=%3")
                        .arg(cols[c].id).arg(cols[c].displayName)
                        .arg(v == -999999 ? "-" : QString::number(v, 'f', 0));
                }
                lines << QString("行%1: %2 %3 %4 | %5")
                    .arg(r).arg(row.date.toString("yyyy-MM-dd"))
                    .arg(row.entityType).arg(row.entityName)
                    .arg(vals.join(" "));
            }
        } else {
            lines << "(表为空)";
        }

        QMessageBox::information(this, "数据库 vs 表模型", lines.join("\n"));
    });

    helpMenu->addAction("关于...");
    connect(helpMenu->actions().last(), &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "关于",
            "快递日报数据管理系统 v1.0\n\n"
            "杭州喵喵至家网络有限公司\n"
            "电话: 17771300068 / 19171045360\n\n"
            "基于 Qt6 + SQLite 开发\n"
            "数据文件: express_daily.db (程序同目录)");
    });
}

void MainWindow::setupToolBar() {
    auto* toolbar = new QToolBar("工具栏", this);
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));

    // Import button
    auto* importBtn = new QPushButton("📂 导入");
    toolbar->addWidget(importBtn);
    connect(importBtn, &QPushButton::clicked, this, &MainWindow::onImportExcel);

    auto* addBtn = new QPushButton("➕ 新增");
    toolbar->addWidget(addBtn);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onAddData);

    auto* editBtn = new QPushButton("✏️ 编辑");
    toolbar->addWidget(editBtn);
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::onEditData);

    auto* delBtn = new QPushButton("🗑 删除");
    toolbar->addWidget(delBtn);
    connect(delBtn, &QPushButton::clicked, this, &MainWindow::onDeleteData);

    auto* refreshBtn = new QPushButton("🔄 刷新");
    toolbar->addWidget(refreshBtn);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefresh);

    auto* exportBtn = new QPushButton("📤 导出");
    toolbar->addWidget(exportBtn);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExportExcel);

    toolbar->addSeparator();

    // Quick date jump
    toolbar->addWidget(new QLabel(" 快捷: "));
    m_quickJumpCombo = new QComboBox();
    m_quickJumpCombo->addItems({"今天", "昨天", "最近7天", "最近30天", "本月", "上月", "全部"});
    m_quickJumpCombo->setCurrentIndex(2); // 最近7天
    toolbar->addWidget(m_quickJumpCombo);
    connect(m_quickJumpCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onQuickJump);

    toolbar->addSeparator();

    // Date range
    toolbar->addWidget(new QLabel(" 日期: "));
    m_startDateEdit = new QDateEdit();
    m_startDateEdit->setCalendarPopup(true);
    m_startDateEdit->setDisplayFormat("yyyy-MM-dd");
    toolbar->addWidget(m_startDateEdit);

    toolbar->addWidget(new QLabel(" ~ "));

    m_endDateEdit = new QDateEdit();
    m_endDateEdit->setCalendarPopup(true);
    m_endDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_endDateEdit->setDate(QDate::currentDate());
    toolbar->addWidget(m_endDateEdit);

    addToolBar(Qt::TopToolBarArea, toolbar);
}

void MainWindow::setupTable() {
    m_model = new DisplayDataModel(this);

    m_tableView = new QTableView();
    m_tableView->setObjectName("dataTable");
    m_tableView->setModel(m_model);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSortingEnabled(false);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->setDefaultSectionSize(28);

    // Fix first 3 column widths
    // Auto-resize: Qt calculates width from content automatically
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setStretchLastSection(false);
    m_tableView->horizontalHeader()->setMinimumSectionSize(60);
    m_tableView->horizontalHeader()->setDefaultSectionSize(80);

    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumHeight(4);
    m_progressBar->setTextVisible(false);

    // --- Content area: sidebar + table ---
    auto* contentRow = new QHBoxLayout();
    contentRow->setContentsMargins(0, 0, 0, 0);
    contentRow->setSpacing(0);

    // Sidebar
    auto* sidebar = new QWidget();
    sidebar->setFixedWidth(140);
    sidebar->setStyleSheet("background-color: #2c3e50;");
    m_sidebarLayout = new QVBoxLayout(sidebar);
    m_sidebarLayout->setContentsMargins(4, 8, 4, 8);
    m_sidebarLayout->setSpacing(4);

    contentRow->addWidget(sidebar);

    // Table area
    auto* rightArea = new QVBoxLayout();
    rightArea->setContentsMargins(4, 0, 4, 4);
    rightArea->addWidget(m_progressBar);
    rightArea->addWidget(m_tableView);
    contentRow->addLayout(rightArea, 1);

    auto* central = qobject_cast<QWidget*>(this->centralWidget());
    if (central) {
        qobject_cast<QVBoxLayout*>(central->layout())->addLayout(contentRow);
    }

    // Build sidebar buttons
    setupSidebar();
}

void MainWindow::setupStatusBar() {
    m_statusLabel = new QLabel("就绪");
    m_countLabel = new QLabel();
    m_dbLabel = new QLabel("数据库: express_daily.db");

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addWidget(m_countLabel);
    statusBar()->addPermanentWidget(m_dbLabel);
}

void MainWindow::setupConnections() {
    // Menu actions
    connect(m_actImport, &QAction::triggered, this, &MainWindow::onImportExcel);
    connect(m_actExport, &QAction::triggered, this, &MainWindow::onExportExcel);
    connect(m_actAdd, &QAction::triggered, this, &MainWindow::onAddData);
    connect(m_actEdit, &QAction::triggered, this, &MainWindow::onEditData);
    connect(m_actDelete, &QAction::triggered, this, &MainWindow::onDeleteData);
    connect(m_actRefresh, &QAction::triggered, this, &MainWindow::onRefresh);

    // Date range changes
    connect(m_startDateEdit, &QDateEdit::dateChanged, this, &MainWindow::onDateRangeChanged);
    connect(m_endDateEdit, &QDateEdit::dateChanged, this, &MainWindow::onDateRangeChanged);

    // Table interaction
    connect(m_tableView, &QTableView::doubleClicked, this, &MainWindow::onTableDoubleClicked);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, &MainWindow::onTableContextMenu);
}

// =========================================================================
// Data loading
// =========================================================================
void MainWindow::loadData() {
    m_model->load(m_startDateEdit->date(), m_endDateEdit->date(), m_filterEntityType);

    updateStatusBar();
}

// =========================================================================
// Sidebar
// =========================================================================
void MainWindow::setupSidebar() {
    // Title
    auto* title = new QLabel("数据分类");
    title->setStyleSheet(
        "color: #ecf0f1; font-size: 12px; font-weight: bold;"
        "padding: 8px 6px 4px 6px; border-bottom: 1px solid #4a6278;"
    );
    m_sidebarLayout->addWidget(title);

    // Category definitions: (buttonText, dbCategory, color)
    struct CatInfo { QString label; QString dbCat; QString color; };
    QVector<CatInfo> categories = {
        {"全部", "",      "#6c757d"},
        {"业务", "业务",  "#3498db"},
        {"客服", "客服",  "#e74c3c"},
        {"操作", "操作",  "#27ae60"},
        {"质量", "小件员取派签质量", "#8e44ad"},
        {"运营", "运营",  "#e67e22"},
    };

    auto* btnGroup = new QButtonGroup(this);

    for (const auto& cat : categories) {
        auto* btn = new QPushButton(cat.label);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(cat.label == "全部" ? "显示全部列" : "仅显示「" + cat.dbCat + "」相关列");
        btn->setMinimumHeight(36);
        btn->setStyleSheet(QString(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #3d5166, stop:1 #2c3e50);
                color: #bdc3c7;
                border: 1px solid #1a252f;
                border-left: 4px solid %1;
                border-radius: 6px;
                padding: 8px 10px;
                text-align: left;
                font-size: 13px;
                font-weight: bold;
                margin: 1px 0px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 %1, stop:1 #1a252f);
                color: white;
                border-left: 4px solid white;
            }
            QPushButton:checked {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 %1, stop:1 #1a252f);
                color: white;
                border-left: 4px solid white;
                font-weight: bold;
            }
        )").arg(cat.color));

        btnGroup->addButton(btn);
        m_sidebarLayout->addWidget(btn);
        m_sidebarBtns.append(btn);

        QString dbCat = cat.dbCat;
        connect(btn, &QPushButton::clicked, this, [this, dbCat]() {
            onCategoryClicked(dbCat);
        });
    }

    m_sidebarLayout->addStretch();

    // Default: "全部" selected
    if (!m_sidebarBtns.isEmpty()) {
        m_sidebarBtns[0]->setChecked(true);
    }
}

void MainWindow::onCategoryClicked(const QString& dbCategory) {
    m_activeCategory = dbCategory;
    m_model->setCategoryFilter(dbCategory);
    statusBar()->showMessage(
        dbCategory.isEmpty() ? "显示全部列" : QString("当前分类: %1").arg(dbCategory), 3000);
}

void MainWindow::updateSidebarHighlight(const QString&) {
    // Handled by QButtonGroup + checkable pushbuttons
}

void MainWindow::updateStatusBar() {
    int count = m_model->rowCount();
    m_countLabel->setText(QString("共 %1 条记录").arg(count));
    QDate latest = DailyDao::getLatestDate();
    m_statusLabel->setText(QString("最新数据: %1").arg(FormatUtils::dateToString(latest)));
}

// =========================================================================
// Slots — File
// =========================================================================
void MainWindow::onImportExcel() {
    ImportDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // Expand date range to cover all data after import
        QDate earliest = DailyDao::getEarliestDate();
        QDate latest = DailyDao::getLatestDate();
        if (earliest.isValid() && latest.isValid()) {
            m_startDateEdit->setDate(earliest);
            m_endDateEdit->setDate(latest);
        }
        loadData();
    }
}

void MainWindow::onExportExcel() {
    QString path = QFileDialog::getSaveFileName(this, "导出Excel",
        "日报_" + QDate::currentDate().toString("yyyyMMdd") + ".xlsx",
        "Excel (*.xlsx);;CSV (*.csv)");
    if (path.isEmpty()) return;

    auto rows = QVector<DisplayRow>(m_model->rowCount());
    for (int i = 0; i < m_model->rowCount(); ++i)
        rows[i] = m_model->rowAt(i);

    bool ok = false;
    if (path.endsWith(".csv")) {
        ok = ExportService::exportToCsv(path, rows, m_model->displayedColumns());
    } else {
        ok = ExportService::exportToExcel(path, rows, m_model->displayedColumns());
    }

    if (ok) {
        statusBar()->showMessage("导出成功: " + path, 5000);
    } else {
        QMessageBox::warning(this, "导出失败", "无法写入文件: " + path);
    }
}

void MainWindow::onExportCsv() {
    QString path = QFileDialog::getSaveFileName(this, "导出CSV",
        "日报_" + QDate::currentDate().toString("yyyyMMdd") + ".csv",
        "CSV (*.csv)");
    if (path.isEmpty()) return;

    auto rows = QVector<DisplayRow>(m_model->rowCount());
    for (int i = 0; i < m_model->rowCount(); ++i)
        rows[i] = m_model->rowAt(i);

    if (ExportService::exportToCsv(path, rows, m_model->displayedColumns())) {
        statusBar()->showMessage("导出成功: " + path, 5000);
    } else {
        QMessageBox::warning(this, "导出失败", "无法写入文件: " + path);
    }
}

// =========================================================================
// Slots — Edit
// =========================================================================
void MainWindow::onAddData() {
    DataEntryDialog dlg(this);
    dlg.setMode(DataEntryDialog::ModeNew);
    dlg.setDate(QDate::currentDate());
    connect(&dlg, &DataEntryDialog::dataSaved, this, [this]() { loadData(); });
    dlg.exec();
}

void MainWindow::onEditData() {
    QModelIndex idx = m_tableView->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, "提示", "请先选择要编辑的数据行");
        return;
    }

    DisplayRow row = m_model->rowAt(idx.row());

    // Check if this is an aggregated company row
    if (row.isCompany()) {
        // Allow editing only non-aggregated fields via a different flow
        // For now, let the dialog handle the distinction
    }

    DataEntryDialog dlg(this);
    dlg.setMode(DataEntryDialog::ModeEdit);
    dlg.setDate(row.date);
    dlg.setEditTarget(row.entityId, row.entityType, row.date);
    connect(&dlg, &DataEntryDialog::dataSaved, this, [this]() { loadData(); });
    dlg.exec();
}

void MainWindow::onDeleteData() {
    QModelIndex idx = m_tableView->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, "提示", "请先选择要删除的数据行");
        return;
    }

    DisplayRow row = m_model->rowAt(idx.row());

    auto answer = QMessageBox::question(this, "确认删除",
        QString("确定要删除 %1 [%2] 在 %3 的全部数据吗？\n此操作不可恢复。")
            .arg(row.entityName)
            .arg(row.entityType)
            .arg(FormatUtils::dateToString(row.date)));

    if (answer == QMessageBox::Yes) {
        if (DailyDao::remove(row.entityId, FormatUtils::dateToString(row.date))) {
            // If deleting a contractor, re-aggregate its parent company
            if (row.isContractor() && row.parentCompanyId > 0) {
                AggregationService::regenerateCompanyData(
                    row.parentCompanyId, row.date);
            }
            loadData();
            statusBar()->showMessage("已删除", 3000);
        }
    }
}

void MainWindow::onCopyPreviousDay() {
    DataEntryDialog dlg(this);
    dlg.setMode(DataEntryDialog::ModeNew);
    dlg.setDate(QDate::currentDate());
    // The dialog has its own "copy previous day" button
    connect(&dlg, &DataEntryDialog::dataSaved, this, [this]() { loadData(); });
    dlg.exec();
}

// =========================================================================
// Slots — Data
// =========================================================================
void MainWindow::onRefresh() {
    loadData();
    statusBar()->showMessage("数据已刷新", 3000);
}

// =========================================================================
// Slots — View
// =========================================================================
void MainWindow::onShowTrendChart() {
    ChartDialog dlg(this);
    dlg.exec();
}

void MainWindow::onShowCompareChart() {
    // Same dialog — user picks chart type inside
    ChartDialog dlg(this);
    dlg.exec();
}

void MainWindow::onFilterCompany() {
    m_filterEntityType = (m_filterEntityType == 1) ? 0 : 1;
    loadData();
}

void MainWindow::onFilterContractor() {
    m_filterEntityType = (m_filterEntityType == 2) ? 0 : 2;
    loadData();
}

void MainWindow::onFilterAll() {
    m_filterEntityType = 0;
    loadData();
}

void MainWindow::onSelectVisibleColumns() {
    ColumnManageDialog dlg(this);
    dlg.exec();
    loadData(); // reload to apply column changes
}

// =========================================================================
// Slots — Manage
// =========================================================================
void MainWindow::onManageEntities() {
    EntityManageDialog dlg(this);
    dlg.exec();
    loadData();
}

void MainWindow::onManageColumns() {
    ColumnManageDialog dlg(this);
    dlg.exec();
    loadData();
}

// =========================================================================
// Slots — Date
// =========================================================================
void MainWindow::onDateRangeChanged() {
    loadData();
}

void MainWindow::onQuickJump(int index) {
    QDate today = QDate::currentDate();
    QDate start, end;

    switch (index) {
    case 0: start = end = today; break;                       // 今天
    case 1: start = end = today.addDays(-1); break;            // 昨天
    case 2: start = today.addDays(-7); end = today; break;     // 最近7天
    case 3: start = today.addDays(-30); end = today; break;    // 最近30天
    case 4: start = QDate(today.year(), today.month(), 1);     // 本月
            end = today; break;
    case 5: start = QDate(today.year(), today.month(), 1).addMonths(-1); // 上月
            end = QDate(today.year(), today.month(), 1).addDays(-1); break;
    case 6: start = DailyDao::getEarliestDate();               // 全部
            end = DailyDao::getLatestDate(); break;
    default: return;
    }

    m_startDateEdit->setDate(start);
    m_endDateEdit->setDate(end);
    // dateChanged signal triggers onDateRangeChanged → loadData
}

// =========================================================================
// Slots — Table interaction
// =========================================================================
void MainWindow::onTableDoubleClicked(const QModelIndex& index) {
    onEditData();
}

void MainWindow::onTableContextMenu(const QPoint& pos) {
    QModelIndex idx = m_tableView->indexAt(pos);
    if (!idx.isValid()) return;

    QMenu menu;
    menu.addAction("编辑", this, &MainWindow::onEditData);
    menu.addAction("删除", this, &MainWindow::onDeleteData);
    menu.addSeparator();
    menu.addAction("复制日期", this, [this, idx]() {
        QApplication::clipboard()->setText(
            m_model->data(m_model->index(idx.row(), DisplayDataModel::ColDate)).toString());
    });
    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}
