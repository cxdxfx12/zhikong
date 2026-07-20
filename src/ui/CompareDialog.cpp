#include "CompareDialog.h"
#include "db/Database.h"
#include "db/EntityDao.h"
#include "db/ColumnDao.h"
#include "utils/FormatUtils.h"
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QSqlQuery>
#include <QHeaderView>

CompareDialog::CompareDialog(QWidget* parent) : QDialog(parent) {
    setupUI(); setWindowTitle("环比数据对比"); resize(1050, 620);
}

void CompareDialog::setupUI() {
    auto* ml = new QVBoxLayout(this);

    // -- Top controls --
    auto* top = new QHBoxLayout();
    top->addWidget(new QLabel("对比周期:"));
    m_periodCombo = new QComboBox();
    m_periodCombo->addItems({"本周 vs 上周", "本月 vs 上月", "本月 vs 去年同月", "自定义"});
    top->addWidget(m_periodCombo);

    top->addWidget(new QLabel("  当前:"));
    m_startCur = new QDateEdit(QDate::currentDate().addDays(-7));
    m_startCur->setCalendarPopup(true); m_startCur->setDisplayFormat("yyyy-MM-dd");
    top->addWidget(m_startCur);
    top->addWidget(new QLabel("~"));
    m_endCur = new QDateEdit(QDate::currentDate());
    m_endCur->setCalendarPopup(true); m_endCur->setDisplayFormat("yyyy-MM-dd");
    top->addWidget(m_endCur);

    top->addWidget(new QLabel("  对比:"));
    m_startPrev = new QDateEdit(QDate::currentDate().addDays(-14));
    m_startPrev->setCalendarPopup(true); m_startPrev->setDisplayFormat("yyyy-MM-dd");
    top->addWidget(m_startPrev);
    top->addWidget(new QLabel("~"));
    m_endPrev = new QDateEdit(QDate::currentDate().addDays(-7));
    m_endPrev->setCalendarPopup(true); m_endPrev->setDisplayFormat("yyyy-MM-dd");
    top->addWidget(m_endPrev);

    auto* refreshBtn = new QPushButton("刷新"); refreshBtn->setStyleSheet("QPushButton{background:#118DFF;color:white;border:none;padding:6px 16px;border-radius:4px;font-weight:bold;}");
    top->addWidget(refreshBtn);
    top->addStretch();
    ml->addLayout(top);

    // -- Entity & metric selectors --
    auto* sel = new QHBoxLayout();
    auto* eg = new QGroupBox("实体");
    m_entityLayout = new QVBoxLayout(eg); m_entityLayout->setSpacing(1);
    populateEntities();
    auto* es = new QScrollArea(); es->setWidget(eg); es->setWidgetResizable(true); es->setMaximumHeight(120);
    sel->addWidget(es);

    auto* mg = new QGroupBox("指标");
    m_metricLayout = new QVBoxLayout(mg); m_metricLayout->setSpacing(1);
    auto* allCheck = new QCheckBox("全选"); allCheck->setChecked(true);
    m_metricLayout->addWidget(allCheck);
    populateMetrics();
    connect(allCheck, &QCheckBox::toggled, this, [this](bool on){ for(auto* cb:m_metricChecks) cb->setChecked(on); });
    auto* ms = new QScrollArea(); ms->setWidget(mg); ms->setWidgetResizable(true); ms->setMaximumHeight(120);
    sel->addWidget(ms, 1);
    ml->addLayout(sel);

    // -- Table --
    m_table = new QTableWidget();
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({"指标", "当期合计", "对比期合计", "差值", "环比%", "趋势"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    ml->addWidget(m_table, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &CompareDialog::onRefresh);
    connect(m_periodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CompareDialog::onPeriodChanged);
    onPeriodChanged(0);
}

void CompareDialog::populateEntities() {
    for(auto* cb:m_entityChecks) delete cb; m_entityChecks.clear();
    auto tree = EntityDao::getTree();
    for(const auto& comp:tree) {
        auto* cb = new QCheckBox(comp.entity.name); cb->setProperty("eid",comp.entity.id); cb->setChecked(true);
        m_entityChecks<<cb; m_entityLayout->addWidget(cb);
        for(const auto& ctr:comp.children) {
            auto* cb2 = new QCheckBox("  "+ctr.entity.name); cb2->setProperty("eid",ctr.entity.id); cb2->setChecked(true);
            m_entityChecks<<cb2; m_entityLayout->addWidget(cb2);
        }
    }
}

void CompareDialog::populateMetrics() {
    for(auto* cb:m_metricChecks) delete cb; m_metricChecks.clear();
    for(const auto& col:ColumnDao::getAll(false)) {
        auto* cb = new QCheckBox(col.displayNameWithUnit()); cb->setProperty("cid",col.id); cb->setChecked(true);
        m_metricChecks<<cb; m_metricLayout->addWidget(cb);
    }
}

void CompareDialog::onPeriodChanged(int idx) {
    QDate today = QDate::currentDate();
    QDate startCur = m_startCur->date(), endCur = m_endCur->date();
    QDate startPrev = m_startPrev->date(), endPrev = m_endPrev->date();
    int dow = today.dayOfWeek(); // Mon=1, Sun=7

    switch(idx) {
    case 0: // 本周 vs 上周
        startCur = today.addDays(1 - dow); // Monday
        endCur = today;
        startPrev = startCur.addDays(-7);
        endPrev = startCur.addDays(-1);
        break;
    case 1: // 本月 vs 上月
        startCur = QDate(today.year(), today.month(), 1);
        endCur = today;
        startPrev = startCur.addMonths(-1);
        endPrev = startCur.addDays(-1);
        break;
    case 2: // 本月 vs 去年同月
        startCur = QDate(today.year(), today.month(), 1);
        endCur = today;
        startPrev = startCur.addYears(-1);
        endPrev = QDate(today.year()-1, today.month(), today.day()).addDays(-1);
        break;
    case 3: // 自定义 — keep current dates, just compute
        break;
    }

    m_startCur->setDate(startCur); m_endCur->setDate(endCur);
    m_startPrev->setDate(startPrev); m_endPrev->setDate(endPrev);
    compute();
}

void CompareDialog::onRefresh() { compute(); }

void CompareDialog::compute() {
    // Gather selected entity IDs and column IDs
    QVector<int> eids;
    for(auto* cb:m_entityChecks) if(cb->isChecked()) eids<<cb->property("eid").toInt();
    if(eids.isEmpty()) { m_table->setRowCount(0); return; }

    QStringList eidsStr; for(int id:eids) eidsStr<<QString::number(id);
    QString eidsIn = eidsStr.join(",");

    // Build query for current and previous periods
    auto queryPeriod = [&](const QDate& start, const QDate& end) -> QMap<int,double> {
        QMap<int,double> result;
        QSqlQuery q(Database::instance().db());
        QString sql = QString(
            "SELECT dv.column_id, SUM(dv.value) FROM daily_values dv "
            "WHERE dv.entity_id IN (%1) AND dv.report_date BETWEEN '%2' AND '%3' "
            "GROUP BY dv.column_id").arg(eidsIn).arg(start.toString("yyyy-MM-dd")).arg(end.toString("yyyy-MM-dd"));
        if(q.exec(sql)) while(q.next()) result[q.value(0).toInt()] = q.value(1).toDouble();
        return result;
    };

    auto curData = queryPeriod(m_startCur->date(), m_endCur->date());
    auto prevData = queryPeriod(m_startPrev->date(), m_endPrev->date());

    // Build table rows for checked metrics
    QVector<ColumnDef> showCols;
    for(auto* cb:m_metricChecks) {
        if(!cb->isChecked()) continue;
        int cid = cb->property("cid").toInt();
        auto cd = ColumnDao::getById(cid);
        if(cd.id > 0) showCols<<cd;
    }

    m_table->setRowCount(showCols.size());
    for(int r = 0; r < showCols.size(); ++r) {
        const auto& col = showCols[r];
        double cur = curData.value(col.id, 0);
        double prev = prevData.value(col.id, 0);
        double diff = cur - prev;
        double pct = (prev != 0) ? (diff / prev * 100.0) : (cur != 0 ? 999.0 : 0);
        QString trend = (diff > 0.01) ? "▲" : (diff < -0.01) ? "▼" : "─";
        QString pctStr = (prev == 0 && cur == 0) ? "-" : QString("%1% %2").arg(QString::number(pct,'f',1)).arg(trend);

        auto* item0 = new QTableWidgetItem(col.displayNameWithUnit());
        m_table->setItem(r, 0, item0);

        auto* item1 = new QTableWidgetItem(FormatUtils::formatValue(cur, col));
        item1->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, 1, item1);

        auto* item2 = new QTableWidgetItem(FormatUtils::formatValue(prev, col));
        item2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, 2, item2);

        auto* item3 = new QTableWidgetItem((diff >= 0 ? "+" : "") + FormatUtils::formatValue(diff, col));
        item3->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item3->setForeground(diff > 0.01 ? QColor("#27ae60") : (diff < -0.01 ? QColor("#e74c3c") : QColor("#999")));
        m_table->setItem(r, 3, item3);

        auto* item4 = new QTableWidgetItem(pctStr);
        item4->setTextAlignment(Qt::AlignCenter);
        item4->setForeground(diff > 0.01 ? QColor("#27ae60") : (diff < -0.01 ? QColor("#e74c3c") : QColor("#999")));
        m_table->setItem(r, 4, item4);

        // Trend icon
        auto* item5 = new QTableWidgetItem(trend);
        item5->setTextAlignment(Qt::AlignCenter);
        QFont tf; tf.setPointSize(14);
        item5->setFont(tf);
        item5->setForeground(diff > 0.01 ? QColor("#27ae60") : (diff < -0.01 ? QColor("#e74c3c") : QColor("#bbb")));
        m_table->setItem(r, 5, item5);
    }

    m_table->resizeColumnsToContents();
    m_table->setColumnWidth(0, 150);
    for(int c=1; c<6; ++c) m_table->setColumnWidth(c, qMax(85, m_table->columnWidth(c)));
}
