#include "CompareDialog.h"
#include "db/Database.h"
#include "db/EntityDao.h"
#include "db/ColumnDao.h"
#include "utils/FormatUtils.h"
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSqlQuery>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <QStringConverter>
#include <QFrame>

// Card widget helper
static QWidget* makeCard(const QString& title, QLabel*& valueLabel, const QColor& accent) {
    auto* card = new QFrame();
    card->setStyleSheet(QString("QFrame{background:white;border-radius:8px;border-left:4px solid %1;padding:12px;}").arg(accent.name()));
    auto* lay = new QVBoxLayout(card); lay->setSpacing(4);
    auto* tl = new QLabel(title); tl->setStyleSheet("color:#888;font-size:11px;font-weight:bold;");
    lay->addWidget(tl);
    valueLabel = new QLabel("--"); valueLabel->setStyleSheet("font-size:18px;font-weight:bold;color:#333;");
    lay->addWidget(valueLabel);
    return card;
}

CompareDialog::CompareDialog(QWidget* parent) : QDialog(parent) {
    setupUI(); setWindowTitle("经营数据环比分析"); resize(1200, 800);
}

void CompareDialog::setupUI() {
    auto* ml = new QVBoxLayout(this); ml->setSpacing(8);

    // === TOP: Period controls ===
    auto* topBar = new QHBoxLayout();
    auto* titleLabel = new QLabel("<b style='font-size:15px;color:#333;'>经营数据环比分析</b>");
    topBar->addWidget(titleLabel);
    topBar->addStretch();

    m_periodLabel = new QLabel();
    m_periodLabel->setStyleSheet("color:#888;font-size:11px;");
    topBar->addWidget(m_periodLabel);

    topBar->addWidget(new QLabel("对比:"));
    m_periodCombo = new QComboBox();
    m_periodCombo->addItems({"本周 vs 上周", "本月 vs 上月", "本月 vs 去年同月", "自定义"});
    topBar->addWidget(m_periodCombo);
    topBar->addWidget(new QLabel("当期:"));
    m_startCur = new QDateEdit(QDate::currentDate().addDays(-7));
    m_startCur->setCalendarPopup(true); m_startCur->setDisplayFormat("yyyy-MM-dd");
    topBar->addWidget(m_startCur);
    topBar->addWidget(new QLabel("~"));
    m_endCur = new QDateEdit(QDate::currentDate());
    m_endCur->setCalendarPopup(true); m_endCur->setDisplayFormat("yyyy-MM-dd");
    topBar->addWidget(m_endCur);
    topBar->addWidget(new QLabel("对比期:"));
    m_startPrev = new QDateEdit(QDate::currentDate().addDays(-14));
    m_startPrev->setCalendarPopup(true); m_startPrev->setDisplayFormat("yyyy-MM-dd");
    topBar->addWidget(m_startPrev);
    topBar->addWidget(new QLabel("~"));
    m_endPrev = new QDateEdit(QDate::currentDate().addDays(-7));
    m_endPrev->setCalendarPopup(true); m_endPrev->setDisplayFormat("yyyy-MM-dd");
    topBar->addWidget(m_endPrev);

    m_refreshBtn = new QPushButton("刷新");
    m_refreshBtn->setStyleSheet("QPushButton{background:#118DFF;color:white;border:none;padding:6px 16px;border-radius:4px;font-weight:bold;} QPushButton:hover{background:#0B6ED4;}");
    topBar->addWidget(m_refreshBtn);

    auto* exportBtn = new QPushButton("导出CSV");
    topBar->addWidget(exportBtn);
    ml->addLayout(topBar);

    // === Summary cards ===
    auto* cardsRow = new QHBoxLayout();
    cardsRow->addWidget(makeCard("出港量", m_cardOutbound, QColor("#3498db")));
    cardsRow->addWidget(makeCard("进港量", m_cardInbound, QColor("#e67e22")));
    cardsRow->addWidget(makeCard("当天签收率", m_cardSignRate, QColor("#27ae60")));
    cardsRow->addWidget(makeCard("⚠ 需关注指标", m_cardWarning, QColor("#e74c3c")));
    ml->addLayout(cardsRow);

    // === Entity & metric selectors ===
    auto* filterRow = new QHBoxLayout();
    auto* eg = new QGroupBox("选择实体");
    m_entityLayout2 = new QVBoxLayout(eg); m_entityLayout2->setSpacing(1);
    filterRow->addWidget(eg, 1);

    auto* mg = new QGroupBox("选择指标");
    m_metricLayout2 = new QVBoxLayout(mg); m_metricLayout2->setSpacing(1);
    auto* allCheck = new QCheckBox("全选"); allCheck->setChecked(true);
    m_metricLayout2->addWidget(allCheck);
    connect(allCheck, &QCheckBox::toggled, this, [this](bool on){ for(auto* cb:m_metricChecks) cb->setChecked(on); });
    filterRow->addWidget(mg, 2);
    ml->addLayout(filterRow);

    populateEntities();
    populateMetrics();

    // === Tabs ===
    m_tabWidget = new QTabWidget();
    m_metricsTable = new QTableWidget();
    m_metricsTable->setAlternatingRowColors(true);
    m_metricsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_metricsTable->verticalHeader()->setDefaultSectionSize(28);
    m_tabWidget->addTab(m_metricsTable, "指标对比");

    m_rankingTable = new QTableWidget();
    m_rankingTable->setAlternatingRowColors(true);
    m_rankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rankingTable->verticalHeader()->setDefaultSectionSize(28);
    m_tabWidget->addTab(m_rankingTable, "实体排行");

    ml->addWidget(m_tabWidget, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, &CompareDialog::onRefresh);
    connect(exportBtn, &QPushButton::clicked, this, &CompareDialog::onExport);
    connect(m_periodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CompareDialog::onPeriodChanged);
    onPeriodChanged(0);
}

void CompareDialog::populateEntities() {
    for(auto* cb:m_entityChecks) delete cb; m_entityChecks.clear();
    if(!m_entityLayout2) return;
    auto tree = EntityDao::getTree();
    for(const auto& comp:tree) {
        auto* cb = new QCheckBox(comp.entity.name); cb->setProperty("eid",comp.entity.id); cb->setChecked(true);
        m_entityChecks<<cb; m_entityLayout2->addWidget(cb);
        for(const auto& ctr:comp.children) {
            auto* cb2 = new QCheckBox("  "+ctr.entity.name); cb2->setProperty("eid",ctr.entity.id); cb2->setChecked(true);
            m_entityChecks<<cb2; m_entityLayout2->addWidget(cb2);
        }
    }
}

void CompareDialog::populateMetrics() {
    for(auto* cb:m_metricChecks) delete cb; m_metricChecks.clear();
    if(!m_metricLayout2) return;
    for(const auto& col:ColumnDao::getAll(false)) {
        auto* cb = new QCheckBox(col.displayNameWithUnit()); cb->setProperty("cid",col.id); cb->setChecked(true);
        m_metricChecks<<cb; m_metricLayout2->addWidget(cb);
    }
}

void CompareDialog::onPeriodChanged(int idx) {
    QDate today = QDate::currentDate();
    int dow = today.dayOfWeek();
    QDate startCur = m_startCur->date(), endCur = m_endCur->date();
    QDate startPrev = m_startPrev->date(), endPrev = m_endPrev->date();

    switch(idx) {
    case 0: // 本周 vs 上周
        startCur = today.addDays(1 - dow); endCur = today;
        startPrev = startCur.addDays(-7); endPrev = startCur.addDays(-1);
        break;
    case 1: // 本月 vs 上月
        startCur = QDate(today.year(), today.month(), 1); endCur = today;
        startPrev = startCur.addMonths(-1); endPrev = startCur.addDays(-1);
        break;
    case 2: // 本月 vs 去年同月
        startCur = QDate(today.year(), today.month(), 1); endCur = today;
        startPrev = startCur.addYears(-1);
        endPrev = QDate(today.year()-1, today.month(), 1).addDays(today.day()-1);
        break;
    case 3: break; // custom
    }

    m_startCur->setDate(startCur); m_endCur->setDate(endCur);
    m_startPrev->setDate(startPrev); m_endPrev->setDate(endPrev);
    m_periodLabel->setText(QString("当期: %1~%2 | 对比: %3~%4")
        .arg(startCur.toString("MM/dd")).arg(endCur.toString("MM/dd"))
        .arg(startPrev.toString("MM/dd")).arg(endPrev.toString("MM/dd")));
    compute();
}

void CompareDialog::onRefresh() { compute(); }

QMap<int,double> CompareDialog::queryPeriod(int entTypeFilter, const QDate& start, const QDate& end) {
    QMap<int,double> result;
    QVector<int> eids;
    for(auto* cb:m_entityChecks) if(cb->isChecked()) eids<<cb->property("eid").toInt();
    if(eids.isEmpty()) return result;

    QStringList es; for(int id:eids) es<<QString::number(id);
    QSqlQuery q(Database::instance().db());
    QString sql = QString(
        "SELECT dv.column_id, SUM(dv.value) FROM daily_values dv "
        "JOIN entities e ON dv.entity_id=e.id "
        "WHERE dv.entity_id IN (%1) AND dv.report_date BETWEEN '%2' AND '%3'")
        .arg(es.join(",")).arg(start.toString("yyyy-MM-dd")).arg(end.toString("yyyy-MM-dd"));
    if(entTypeFilter > 0) sql += QString(" AND e.type_id=%1").arg(entTypeFilter);
    sql += " GROUP BY dv.column_id";
    if(q.exec(sql)) while(q.next()) result[q.value(0).toInt()] = q.value(1).toDouble();
    return result;
}

void CompareDialog::compute() {
    // Query both periods (only contractor rows for accurate comparison)
    auto curData = queryPeriod(2, m_startCur->date(), m_endCur->date()); // type_id=2 = contractor only
    auto prevData = queryPeriod(2, m_startPrev->date(), m_endPrev->date());
    m_curData = curData; m_prevData = prevData;

    // Build metric list
    m_showCols.clear();
    for(auto* cb:m_metricChecks) {
        if(!cb->isChecked()) continue;
        int cid = cb->property("cid").toInt();
        auto cd = ColumnDao::getById(cid);
        if(cd.id > 0) m_showCols<<cd;
    }

    buildSummaryCards(curData, prevData);
    buildMetricsTable(curData, prevData);
    buildRankingTable(curData, prevData);
}

void CompareDialog::buildSummaryCards(const QMap<int,double>& cur, const QMap<int,double>& prev) {
    auto cardText = [&](const QString& key, const QString& unit) -> QString {
        auto cd = ColumnDao::getByKey(key);
        double c = cur.value(cd.id, 0), p = prev.value(cd.id, 0);
        double pct = (p != 0) ? ((c-p)/p*100.0) : (c != 0 ? 999.0 : 0);
        QString arrow = (pct > 0.5) ? "▲" : (pct < -0.5) ? "▼" : "─";
        return QString("%1 %2 %3%").arg(FormatUtils::formatValue(c, cd)).arg(arrow).arg(QString::number(pct,'f',1));
    };

    auto warnText = [&]() -> QString {
        // Find metric with biggest negative change
        double worst = 0; QString worstName;
        for(const auto& col : m_showCols) {
            double c = cur.value(col.id, 0), p = prev.value(col.id, 0);
            if(p == 0) continue;
            double pct = (c-p)/p*100.0;
            if(pct < worst) { worst = pct; worstName = col.displayName; }
        }
        if(worst < -1.0) return QString("%1 %2%").arg(worstName).arg(QString::number(worst,'f',1));
        return "无明显下降";
    };

    m_cardOutbound->setText(cardText("outbound", "票"));
    m_cardInbound->setText(cardText("inbound", "票"));
    m_cardSignRate->setText(cardText("sign_rate", "%"));
    m_cardWarning->setText(warnText());
}

void CompareDialog::buildMetricsTable(const QMap<int,double>& cur, const QMap<int,double>& prev) {
    m_metricsTable->setColumnCount(8);
    m_metricsTable->setHorizontalHeaderLabels({"指标","当期合计","当期日均","对比期合计","对比期日均","差值","环比%","趋势"});
    m_metricsTable->setRowCount(m_showCols.size());

    int daysCur = qMax(1, m_startCur->date().daysTo(m_endCur->date()) + 1);
    int daysPrev = qMax(1, m_startPrev->date().daysTo(m_endPrev->date()) + 1);

    for(int r = 0; r < m_showCols.size(); ++r) {
        const auto& col = m_showCols[r];
        double c = cur.value(col.id, 0), p = prev.value(col.id, 0);
        double diff = c - p;
        double pct = (p != 0) ? (diff/p*100.0) : (c != 0 ? 999.0 : 0);
        QString trend = (diff > 0.01) ? "▲" : (diff < -0.01) ? "▼" : "─";
        QColor clr = (diff > 0.01) ? QColor("#27ae60") : (diff < -0.01) ? QColor("#e74c3c") : QColor("#999");

        auto setCell = [&](int colIdx, const QString& text, Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(align);
            if(colIdx >= 4) item->setForeground(clr);
            m_metricsTable->setItem(r, colIdx, item);
        };

        setCell(0, col.displayNameWithUnit(), Qt::AlignLeft | Qt::AlignVCenter);
        setCell(1, FormatUtils::formatValue(c, col));
        setCell(2, FormatUtils::formatValue(c / daysCur, col));
        setCell(3, FormatUtils::formatValue(p, col));
        setCell(4, FormatUtils::formatValue(p / daysPrev, col));
        setCell(5, (diff >= 0 ? "+" : "") + FormatUtils::formatValue(diff, col));
        setCell(6, (pct > 999) ? "新增" : QString("%1%").arg(QString::number(pct,'f',1)));
        setCell(7, trend, Qt::AlignCenter);

        // Highlight the indicator column with light background
        if(diff > 0.01) {
            for(int cc=5; cc<8; ++cc) m_metricsTable->item(r, cc)->setBackground(QColor("#f0faf3"));
        } else if(diff < -0.01) {
            for(int cc=5; cc<8; ++cc) m_metricsTable->item(r, cc)->setBackground(QColor("#fef5f5"));
        }
    }

    m_metricsTable->resizeColumnsToContents();
    m_metricsTable->setColumnWidth(0, 150);
    for(int c=1; c<8; ++c) m_metricsTable->setColumnWidth(c, qMax(78, m_metricsTable->columnWidth(c)));
}

void CompareDialog::buildRankingTable(const QMap<int,double>& cur, const QMap<int,double>& prev) {
    // Rank entities by a key metric (use first checked metric, or outbound)
    int rankCid = 1; // default outbound
    if(!m_showCols.isEmpty()) rankCid = m_showCols.first().id;

    QVector<int> eids;
    for(auto* cb:m_entityChecks) if(cb->isChecked()) eids<<cb->property("eid").toInt();

    // Query per-entity data for ranking
    QStringList es; for(int id:eids) es<<QString::number(id);
    auto queryEntity = [&](const QDate& start, const QDate& end) -> QMap<int,double> {
        QMap<int,double> r;
        QSqlQuery q(Database::instance().db());
        q.prepare(QString("SELECT dv.entity_id, SUM(dv.value) FROM daily_values dv WHERE dv.entity_id IN (%1) AND dv.column_id=? AND dv.report_date BETWEEN ? AND ? GROUP BY dv.entity_id").arg(es.join(",")));
        q.addBindValue(rankCid); q.addBindValue(start.toString("yyyy-MM-dd")); q.addBindValue(end.toString("yyyy-MM-dd"));
        if(q.exec()) while(q.next()) r[q.value(0).toInt()] = q.value(1).toDouble();
        return r;
    };

    auto eCur = queryEntity(m_startCur->date(), m_endCur->date());
    auto ePrev = queryEntity(m_startPrev->date(), m_endPrev->date());

    // Build sorted list by current period value desc
    QVector<QPair<int,double>> sorted;
    for(auto it=eCur.begin(); it!=eCur.end(); ++it) sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return a.second > b.second; });

    auto cd = ColumnDao::getById(rankCid);
    m_rankingTable->setColumnCount(6);
    m_rankingTable->setHorizontalHeaderLabels({"排名","实体",QString("当期%1").arg(cd.displayName),"对比期","差值","环比%"});
    m_rankingTable->setRowCount(sorted.size());

    for(int r=0; r<sorted.size(); ++r) {
        int eid = sorted[r].first;
        double c = eCur.value(eid, 0), p = ePrev.value(eid, 0);
        double diff = c - p;
        double pct = (p != 0) ? (diff/p*100.0) : (c != 0 ? 999.0 : 0);
        QColor clr = (diff > 0.01) ? QColor("#27ae60") : (diff < -0.01) ? QColor("#e74c3c") : QColor("#999");

        auto en = EntityDao::getById(eid);

        auto setCell = [&](int colIdx, const QString& text, Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(align);
            if(colIdx >= 4) item->setForeground(clr);
            m_rankingTable->setItem(r, colIdx, item);
        };

        m_rankingTable->setItem(r, 0, new QTableWidgetItem(QString::number(r+1)));
        auto* nameItem = new QTableWidgetItem((en.isContractor() ? "  └ " : "") + en.name);
        if(en.isCompany()) { QFont f; f.setBold(true); nameItem->setFont(f); }
        m_rankingTable->setItem(r, 1, nameItem);
        setCell(2, FormatUtils::formatValue(c, cd));
        setCell(3, FormatUtils::formatValue(p, cd));
        setCell(4, (diff >= 0 ? "+" : "") + FormatUtils::formatValue(diff, cd));
        setCell(5, (pct > 999) ? "新增" : QString("%1%").arg(QString::number(pct,'f',1)));
    }

    m_rankingTable->resizeColumnsToContents();
    m_rankingTable->setColumnWidth(0, 50);
    m_rankingTable->setColumnWidth(1, 160);
    for(int c=2; c<6; ++c) m_rankingTable->setColumnWidth(c, qMax(80, m_rankingTable->columnWidth(c)));
}

void CompareDialog::onExport() {
    QString path = QFileDialog::getSaveFileName(this, "导出对比报告", "环比对比_" + QDate::currentDate().toString("yyyyMMdd") + ".csv", "CSV (*.csv)");
    if(path.isEmpty()) return;

    QFile f(path);
    if(!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << "\xEF\xBB\xBF"; // BOM

    out << "指标,当期合计,当期日均,对比期合计,对比期日均,差值,环比%\n";
    int daysCur = qMax(1, m_startCur->date().daysTo(m_endCur->date()) + 1);
    int daysPrev = qMax(1, m_startPrev->date().daysTo(m_endPrev->date()) + 1);
    for(const auto& col : m_showCols) {
        double cv = m_curData.value(col.id, 0), pv = m_prevData.value(col.id, 0);
        double diff = cv - pv;
        double pct = (pv != 0) ? (diff/pv*100.0) : 0;
        out << col.displayName << ","
            << FormatUtils::formatValue(cv, col) << ","
            << FormatUtils::formatValue(cv / daysCur, col) << ","
            << FormatUtils::formatValue(pv, col) << ","
            << FormatUtils::formatValue(pv / daysPrev, col) << ","
            << (diff >= 0 ? "+" : "") << FormatUtils::formatValue(diff, col) << ","
            << QString::number(pct, 'f', 1) << "%\n";
    }
    f.close();
    QMessageBox::information(this, "导出成功", "已保存到: " + path);
}
