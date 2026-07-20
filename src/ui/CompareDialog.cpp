#include "CompareDialog.h"
#include "db/Database.h"
#include "db/EntityDao.h"
#include "db/ColumnDao.h"
#include "utils/FormatUtils.h"
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QSqlQuery>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <QStringConverter>
#include <QFrame>

// ===== Helper: make a KPI card =====
static QWidget* makeCard(QWidget* parent, QLabel*& title, QLabel*& value, QLabel*& change, QLabel*& status) {
    auto* card = new QFrame(parent);
    card->setStyleSheet("QFrame{background:white;border-radius:8px;border:1px solid #e8e8e8;padding:10px;}");
    auto* lay = new QVBoxLayout(card); lay->setSpacing(2);
    title = new QLabel("--"); title->setStyleSheet("color:#888;font-size:10px;font-weight:bold;");
    lay->addWidget(title);
    auto* row = new QHBoxLayout();
    value = new QLabel("--"); value->setStyleSheet("font-size:22px;font-weight:bold;color:#333;");
    row->addWidget(value);
    change = new QLabel(""); change->setStyleSheet("font-size:12px;");
    row->addWidget(change);
    row->addStretch();
    lay->addLayout(row);
    status = new QLabel(""); status->setStyleSheet("font-size:10px;color:#888;");
    lay->addWidget(status);
    return card;
}

// ===== Positive indicators (bigger is better) =====
static const QSet<QString> POSITIVE_KEYS = {
    "outbound", "inbound", "sign_rate", "sign_rate_first", "sign_rate_second",
    "pickup_rate", "intercept_rate", "outbound_timely_rate1", "outbound_timely_rate2",
    "delivery_schedule_score", "scatter_achievement_rate", "first_pickup_rate",
    "on_demand", "call_rate", "sms_rate", "customer_voice", "kpi_score",
    "auto_throughput"
};
// Everything else = negative (smaller is better): fake_sign, miss_rate, complaint, fine, penalty, unsign, etc.

CompareDialog::CompareDialog(QWidget* parent) : QDialog(parent) {
    m_positiveKeys = POSITIVE_KEYS;
    setupUI(); setWindowTitle("数据分析中心"); resize(1300, 850);
}

void CompareDialog::setupUI() {
    auto* ml = new QVBoxLayout(this); ml->setSpacing(6);

    // === TOP BAR ===
    auto* topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel("<b style='font-size:16px;color:#333;'>数据分析中心</b>"));
    topBar->addStretch();
    m_periodLabel = new QLabel(); m_periodLabel->setStyleSheet("color:#888;font-size:11px;");
    topBar->addWidget(m_periodLabel);
    topBar->addWidget(new QLabel("对比周期:"));
    m_periodCombo = new QComboBox();
    m_periodCombo->addItems({"本周 vs 上周", "本月 vs 上月", "上月 vs 去年同月", "自定义"});
    topBar->addWidget(m_periodCombo);

    topBar->addWidget(new QLabel("当期:"));
    m_startCur = new QDateEdit(QDate::currentDate().addDays(-7)); m_startCur->setCalendarPopup(true); m_startCur->setDisplayFormat("yyyy-MM-dd");
    topBar->addWidget(m_startCur); topBar->addWidget(new QLabel("~"));
    m_endCur = new QDateEdit(QDate::currentDate()); m_endCur->setCalendarPopup(true); m_endCur->setDisplayFormat("yyyy-MM-dd");
    topBar->addWidget(m_endCur);
    topBar->addWidget(new QLabel("对比期:"));
    m_startPrev = new QDateEdit(QDate::currentDate().addDays(-14)); m_startPrev->setCalendarPopup(true); m_startPrev->setDisplayFormat("yyyy-MM-dd");
    topBar->addWidget(m_startPrev); topBar->addWidget(new QLabel("~"));
    m_endPrev = new QDateEdit(QDate::currentDate().addDays(-7)); m_endPrev->setCalendarPopup(true); m_endPrev->setDisplayFormat("yyyy-MM-dd");
    topBar->addWidget(m_endPrev);

    m_refreshBtn = new QPushButton("▶ 开始对比"); m_refreshBtn->setStyleSheet("QPushButton{background:#118DFF;color:white;border:none;padding:6px 14px;border-radius:4px;font-weight:bold;}");
    topBar->addWidget(m_refreshBtn);
    auto* exportBtn = new QPushButton("导出CSV"); topBar->addWidget(exportBtn);
    ml->addLayout(topBar);

    // === Legend ===
    auto* legend = new QLabel("<span style='color:#27ae60'>■ 正向改善(利好)</span> &nbsp; <span style='color:#e74c3c'>■ 负向恶化(预警)</span> &nbsp; | &nbsp; 正向指标(签收率等): 升绿降红 &nbsp; 负向指标(破损/投诉等): 升红降绿");
    legend->setStyleSheet("color:#888;font-size:11px;background:#fafafa;padding:4px 10px;border-radius:4px;");
    ml->addWidget(legend);

    // === SUMMARY CARDS (4 quadrants) ===
    auto* cardsRow = new QHBoxLayout();
    cardsRow->addWidget(makeCard(this, m_cardEff.title, m_cardEff.value, m_cardEff.change, m_cardEff.status));
    cardsRow->addWidget(makeCard(this, m_cardQuality.title, m_cardQuality.value, m_cardQuality.change, m_cardQuality.status));
    cardsRow->addWidget(makeCard(this, m_cardService.title, m_cardService.value, m_cardService.change, m_cardService.status));
    cardsRow->addWidget(makeCard(this, m_cardVolume.title, m_cardVolume.value, m_cardVolume.change, m_cardVolume.status));
    ml->addLayout(cardsRow);

    // === FILTERS (collapsible) ===
    auto* filterRow = new QHBoxLayout();

    // Entity panel
    auto* entityPanel = new QWidget();
    auto* entityPanelLay = new QVBoxLayout(entityPanel); entityPanelLay->setContentsMargins(0,0,0,0); entityPanelLay->setSpacing(0);
    auto* entityToggle = new QPushButton("▾ 实体选择"); entityToggle->setStyleSheet("QPushButton{text-align:left;padding:4px 8px;font-weight:bold;border:none;background:#f0f0f0;}");
    entityToggle->setCursor(Qt::PointingHandCursor);
    auto* entityContent = new QWidget();
    m_entityLayout2 = new QVBoxLayout(entityContent); m_entityLayout2->setSpacing(1); m_entityLayout2->setContentsMargins(4,2,4,2);
    entityPanelLay->addWidget(entityToggle);
    entityPanelLay->addWidget(entityContent);
    filterRow->addWidget(entityPanel, 1);

    // Metric panel
    auto* metricPanel = new QWidget();
    auto* metricPanelLay = new QVBoxLayout(metricPanel); metricPanelLay->setContentsMargins(0,0,0,0); metricPanelLay->setSpacing(0);
    auto* metricToggle = new QPushButton("▾ 指标选择"); metricToggle->setStyleSheet("QPushButton{text-align:left;padding:4px 8px;font-weight:bold;border:none;background:#f0f0f0;}");
    metricToggle->setCursor(Qt::PointingHandCursor);
    auto* metricContent = new QWidget();
    m_metricLayout2 = new QVBoxLayout(metricContent); m_metricLayout2->setSpacing(1); m_metricLayout2->setContentsMargins(4,2,4,2);
    metricPanelLay->addWidget(metricToggle);
    metricPanelLay->addWidget(metricContent);
    filterRow->addWidget(metricPanel, 3);

    ml->addLayout(filterRow);
    populateEntities(); populateMetrics();

    // Toggle collapse/expand
    connect(entityToggle, &QPushButton::clicked, this, [entityToggle, entityContent](){
        bool vis = entityContent->isVisible();
        entityContent->setVisible(!vis);
        entityToggle->setText(vis ? "▸ 实体选择" : "▾ 实体选择");
    });
    connect(metricToggle, &QPushButton::clicked, this, [metricToggle, metricContent](){
        bool vis = metricContent->isVisible();
        metricContent->setVisible(!vis);
        metricToggle->setText(vis ? "▸ 指标选择" : "▾ 指标选择");
    });

    // === TABS ===
    m_tabWidget = new QTabWidget();
    m_metricsTable = new QTableWidget(); m_metricsTable->setAlternatingRowColors(true); m_metricsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_metricsTable->verticalHeader()->setDefaultSectionSize(28);
    m_tabWidget->addTab(m_metricsTable, "指标对比明细");
    m_rankingTable = new QTableWidget(); m_rankingTable->setAlternatingRowColors(true); m_rankingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rankingTable->verticalHeader()->setDefaultSectionSize(28);
    m_tabWidget->addTab(m_rankingTable, "实体恶化排行");

    // Splitter with drag handle to resize table area
    auto* splitter = new QSplitter(Qt::Vertical);
    splitter->setChildrenCollapsible(false);
    auto* spacer = new QWidget(); spacer->setMaximumHeight(8);
    splitter->addWidget(spacer);
    splitter->addWidget(m_tabWidget);
    splitter->setSizes({0, 500});
    ml->addWidget(splitter, 1);

    // Connections
    connect(m_refreshBtn, &QPushButton::clicked, this, &CompareDialog::onRefresh);
    connect(exportBtn, &QPushButton::clicked, this, &CompareDialog::onExport);
    connect(m_periodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CompareDialog::onPeriodChanged);
    connect(m_startCur, &QDateEdit::dateChanged, this, &CompareDialog::onRefresh);
    connect(m_endCur, &QDateEdit::dateChanged, this, &CompareDialog::onRefresh);
    connect(m_startPrev, &QDateEdit::dateChanged, this, &CompareDialog::onRefresh);
    connect(m_endPrev, &QDateEdit::dateChanged, this, &CompareDialog::onRefresh);
    onPeriodChanged(0);
}

void CompareDialog::populateEntities() {
    for(auto* cb:m_entityChecks) delete cb; m_entityChecks.clear();
    if(!m_entityLayout2) return;
    auto tree = EntityDao::getTree();
    for(const auto& comp:tree) {
        auto* cb = new QCheckBox(comp.entity.name); cb->setProperty("eid",comp.entity.id); cb->setChecked(true);
        connect(cb, &QCheckBox::toggled, this, &CompareDialog::onRefresh); m_entityChecks<<cb; m_entityLayout2->addWidget(cb);
        for(const auto& ctr:comp.children) {
            auto* cb2 = new QCheckBox("  "+ctr.entity.name); cb2->setProperty("eid",ctr.entity.id); cb2->setChecked(true);
            connect(cb2, &QCheckBox::toggled, this, &CompareDialog::onRefresh); m_entityChecks<<cb2; m_entityLayout2->addWidget(cb2);
        }
    }
}

void CompareDialog::populateMetrics() {
    for(auto* cb:m_metricChecks) delete cb; m_metricChecks.clear();
    if(!m_metricLayout2) return;

    // Remove old widgets and sub-layouts
    QLayoutItem* child;
    while((child=m_metricLayout2->takeAt(0))!=nullptr) {
        if(child->widget()) { delete child->widget(); }
        else if(child->layout()) {
            QLayoutItem* sub; while((sub=child->layout()->takeAt(0))!=nullptr) { delete sub->widget(); delete sub; }
            delete child->layout();
        }
        delete child;
    }

    QStringList cats = {"业务", "客服", "操作", "小件员取派签质量", "运营"};
    QMap<QString,QVector<ColumnDef>> g;
    for(const auto& col:ColumnDao::getAll(false)) { QString c=col.category.isEmpty()?"其他":col.category; g[c].append(col); }

    for(const auto& cat:cats) { if(!g.contains(cat)||g[cat].isEmpty()) continue;
        auto* l=new QLabel("▎"+cat); l->setStyleSheet("color:#888;font-weight:bold;font-size:11px;padding-top:4px;"); m_metricLayout2->addWidget(l);
        auto* grid = new QGridLayout(); grid->setSpacing(1);
        const int COLS = 3;
        for(int i=0; i<g[cat].size(); ++i) {
            const auto& col = g[cat][i];
            auto* cb=new QCheckBox(col.displayNameWithUnit()); cb->setProperty("cid",col.id); cb->setChecked(true);
            cb->setStyleSheet("font-size:11px;");
            connect(cb,&QCheckBox::toggled,this,&CompareDialog::onRefresh);
            m_metricChecks<<cb;
            grid->addWidget(cb, i/COLS, i%COLS);
        }
        m_metricLayout2->addLayout(grid);
    }
}

bool CompareDialog::isPositive(const QString& key) const { return m_positiveKeys.contains(key); }
QColor CompareDialog::changeColor(double diff, const ColumnDef& col) const {
    bool pos = isPositive(col.key);
    if(diff > 0.01) return pos ? QColor("#27ae60") : QColor("#e74c3c");  // pos up=green, neg up=red
    if(diff < -0.01) return pos ? QColor("#e74c3c") : QColor("#27ae60"); // pos down=red, neg down=green
    return QColor("#999");
}

void CompareDialog::onPeriodChanged(int idx) {
    QDate today=QDate::currentDate(); int dow=today.dayOfWeek();
    QDate sc=m_startCur->date(), ec=m_endCur->date(), sp=m_startPrev->date(), ep=m_endPrev->date();
    switch(idx) {
    case 0: sc=today.addDays(1-dow); ec=today; sp=sc.addDays(-7); ep=sc.addDays(-1); break;
    case 1: sc=QDate(today.year(),today.month(),1); ec=today; sp=sc.addMonths(-1); ep=sc.addDays(-1); break;
    case 2: sc=QDate(today.year(),today.month(),1); ec=today; sp=sc.addYears(-1); ep=QDate(today.year()-1,today.month(),1).addDays(today.day()-1); break;
    case 3: break;
    }
    m_startCur->setDate(sc); m_endCur->setDate(ec); m_startPrev->setDate(sp); m_endPrev->setDate(ep);
    m_periodLabel->setText(QString("当期:%1~%2 | 对比:%3~%4").arg(sc.toString("MM/dd")).arg(ec.toString("MM/dd")).arg(sp.toString("MM/dd")).arg(ep.toString("MM/dd")));
    compute();
}

void CompareDialog::onRefresh() { compute(); }

QMap<int,double> CompareDialog::queryPeriod(int etFilter, const QDate& s, const QDate& e) {
    QMap<int,double> r; QVector<int> eids;
    for(auto* cb:m_entityChecks) if(cb->isChecked()) eids<<cb->property("eid").toInt();
    if(eids.isEmpty()) return r;
    QStringList ph; for(int i=0;i<eids.size();++i) ph<<"?";
    QSqlQuery q(Database::instance().db());
    QString sql="SELECT dv.column_id, CASE WHEN cd.aggregate_type='AVG' THEN AVG(dv.value) ELSE SUM(dv.value) END FROM daily_values dv JOIN entities e ON dv.entity_id=e.id JOIN column_defs cd ON dv.column_id=cd.id WHERE dv.entity_id IN ("+ph.join(",")+") AND dv.report_date BETWEEN ? AND ?";
    if(etFilter>0) sql+=" AND e.type_id=?";
    sql+=" GROUP BY dv.column_id";
    q.prepare(sql); for(int id:eids) q.addBindValue(id);
    q.addBindValue(s.toString("yyyy-MM-dd")); q.addBindValue(e.toString("yyyy-MM-dd"));
    if(etFilter>0) q.addBindValue(etFilter);
    if(q.exec()) while(q.next()) r[q.value(0).toInt()]=q.value(1).toDouble();
    return r;
}

void CompareDialog::compute() {
    auto cd=queryPeriod(0,m_startCur->date(),m_endCur->date());
    auto pd=queryPeriod(0,m_startPrev->date(),m_endPrev->date());
    m_curData=cd; m_prevData=pd;
    if(cd.isEmpty()&&pd.isEmpty()){ m_metricsTable->setRowCount(0); m_rankingTable->setRowCount(0); return; }

    m_showCols.clear();
    for(auto* cb:m_metricChecks){ if(!cb->isChecked()) continue; int cid=cb->property("cid").toInt(); auto c=ColumnDao::getById(cid); if(c.id>0) m_showCols<<c; }

    buildSummaryCards(cd,pd); buildMetricsTable(cd,pd); buildRankingTable(cd,pd);
}

void CompareDialog::buildSummaryCards(const QMap<int,double>& cur, const QMap<int,double>& prev) {
    auto findVal = [&](const QString& key) -> QPair<double,double> {
        auto c=ColumnDao::getByKey(key); return {cur.value(c.id,0), prev.value(c.id,0)}; };

    auto setCard = [](CardWidget& cw, const QString& title, double cv, double pv, bool pos) {
        cw.title->setText(title);
        double diff=cv-pv, pct=(pv!=0)?(diff/pv*100.0):(cv!=0?999.0:0);
        cw.value->setText(FormatUtils::formatValue(cv, ColumnDao::getByKey(title.contains("签收")?"sign_rate":title.contains("出港")?"outbound":title.contains("进港")?"inbound":title.contains("投诉")?"fake_sign":"outbound")));
        QColor clr = (diff>0.01)?(pos?QColor("#27ae60"):QColor("#e74c3c")):((diff<-0.01)?(pos?QColor("#e74c3c"):QColor("#27ae60")):QColor("#999"));
        cw.change->setText(QString("<span style='color:%1;'>%2%3%</span>").arg(clr.name()).arg(diff>=0?"▲":"▼").arg(QString::number(qAbs(pct),'f',1)));
        cw.change->setTextFormat(Qt::RichText);
    };

    auto [co,po]=findVal("outbound"); setCard(m_cardVolume, "出港量(票)", co, po, true);
    auto [ci,pi]=findVal("inbound"); setCard(m_cardEff, "进港量(票)", ci, pi, true);
    auto [cs,ps]=findVal("sign_rate"); setCard(m_cardQuality, "当天签收率", cs, ps, true);
    auto [cf,pf]=findVal("fake_sign"); setCard(m_cardService, "虚假签收", cf, pf, false);
}

void CompareDialog::buildMetricsTable(const QMap<int,double>& cur, const QMap<int,double>& prev) {
    int dc=qMax(1,m_startCur->date().daysTo(m_endCur->date())+1);
    int dp=qMax(1,m_startPrev->date().daysTo(m_endPrev->date())+1);

    // Sort: negative indicators with worsening trend first
    QVector<QPair<int,double>> sorted;
    for(const auto& col:m_showCols) {
        double c=cur.value(col.id,0), p=prev.value(col.id,0);
        double diff=c-p; bool pos=isPositive(col.key);
        double score=(pos?-diff:diff); // high score = more concerning
        sorted.append({col.id, score});
    }
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return a.second > b.second; });

    m_metricsTable->setColumnCount(9);
    m_metricsTable->setHorizontalHeaderLabels({"指标","当期合计","当期日均","对比期合计","对比期日均","差值","环比%","恶化指数","趋势"});
    m_metricsTable->setRowCount(sorted.size());

    for(int r=0; r<sorted.size(); ++r) {
        ColumnDef col; for(const auto& c:m_showCols) if(c.id==sorted[r].first){col=c;break;}
        double c=cur.value(col.id,0), p=prev.value(col.id,0);
        double diff=c-p, pct=(p!=0)?(diff/p*100.0):(c!=0?999.0:0);
        QColor clr=changeColor(diff,col);
        bool pos=isPositive(col.key);
        double index=qAbs(pct);

        auto setCell=[&](int ci, const QString& t, Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto* it=new QTableWidgetItem(t); it->setTextAlignment(a);
            if(ci>=5) it->setForeground(clr); m_metricsTable->setItem(r,ci,it);
        };

        // Row background: light red for deteriorating, light green for improving
        bool worsening=(pos&&diff<-0.01)||(!pos&&diff>0.01);
        if(worsening) for(int cc=0;cc<9;++cc){ if(!m_metricsTable->item(r,cc)) m_metricsTable->setItem(r,cc,new QTableWidgetItem("")); m_metricsTable->item(r,cc)->setBackground(QColor("#fff5f5")); }
        else if((pos&&diff>0.01)||(!pos&&diff<-0.01)) for(int cc=0;cc<9;++cc){ if(!m_metricsTable->item(r,cc)) m_metricsTable->setItem(r,cc,new QTableWidgetItem("")); m_metricsTable->item(r,cc)->setBackground(QColor("#f5fff5")); }

        setCell(0,col.displayNameWithUnit(),Qt::AlignLeft|Qt::AlignVCenter);
        setCell(1,FormatUtils::formatValue(c,col)); setCell(2,FormatUtils::formatValue(c/dc,col));
        setCell(3,FormatUtils::formatValue(p,col)); setCell(4,FormatUtils::formatValue(p/dp,col));
        setCell(5,(diff>=0?"+":"")+FormatUtils::formatValue(diff,col));
        setCell(6,(pct>999?"新增":QString::number(pct,'f',1)+"%"));
        setCell(7,QString::number(index,'f',1));
        setCell(8,(diff>0.01?"▲":(diff<-0.01?"▼":"─")),Qt::AlignCenter);
    }
    m_metricsTable->resizeColumnsToContents();
    m_metricsTable->setColumnWidth(0,150);
    for(int c=1;c<9;++c) m_metricsTable->setColumnWidth(c,qMax(75,m_metricsTable->columnWidth(c)));
}

void CompareDialog::buildRankingTable(const QMap<int,double>& cur, const QMap<int,double>& prev) {
    QVector<int> eids; for(auto* cb:m_entityChecks) if(cb->isChecked()) eids<<cb->property("eid").toInt();
    if(eids.isEmpty()){m_rankingTable->setRowCount(0);return;}

    // Use first checked negative indicator for ranking, or outbound
    int rankCid=1; for(const auto& col:m_showCols){ if(!isPositive(col.key)){rankCid=col.id;break;}}

    QStringList ph; for(int i=0;i<eids.size();++i) ph<<"?";
    auto qe=[&](const QDate& s,const QDate& e)->QMap<int,double>{ QMap<int,double> r; QSqlQuery q(Database::instance().db());
        QString sql="SELECT dv.entity_id, CASE WHEN cd.aggregate_type='AVG' THEN AVG(dv.value) ELSE SUM(dv.value) END FROM daily_values dv JOIN column_defs cd ON dv.column_id=cd.id WHERE dv.entity_id IN ("+ph.join(",")+") AND dv.column_id=? AND dv.report_date BETWEEN ? AND ? GROUP BY dv.entity_id";
        q.prepare(sql); for(int id:eids) q.addBindValue(id); q.addBindValue(rankCid); q.addBindValue(s.toString("yyyy-MM-dd")); q.addBindValue(e.toString("yyyy-MM-dd"));
        if(q.exec()) while(q.next()) r[q.value(0).toInt()]=q.value(1).toDouble(); return r; };
    auto ec=qe(m_startCur->date(),m_endCur->date()), ep=qe(m_startPrev->date(),m_endPrev->date());

    QVector<QPair<int,double>> sorted; for(auto it=ec.begin();it!=ec.end();++it){
        double p=ep.value(it.key(),0), diff=it.value()-p; sorted.append({it.key(),diff});}
    std::sort(sorted.begin(),sorted.end(),[](auto& a,auto& b){return a.second>b.second;});

    auto cd=ColumnDao::getById(rankCid);
    m_rankingTable->setColumnCount(7);
    m_rankingTable->setHorizontalHeaderLabels({"排名","实体",QString("当期%1").arg(cd.displayName),"对比期","差值","环比%","恶化指数"});
    m_rankingTable->setRowCount(sorted.size());

    for(int r=0;r<sorted.size();++r){
        int eid=sorted[r].first; double c=ec.value(eid,0), p=ep.value(eid,0); double diff=c-p;
        double pct=(p!=0)?(diff/p*100.0):(c!=0?999.0:0);
        QColor clr=(diff>0.01)?QColor("#e74c3c"):((diff<-0.01)?QColor("#27ae60"):QColor("#999"));
        auto en=EntityDao::getById(eid);
        auto sc=[&](int ci,const QString& t,Qt::Alignment a=Qt::AlignRight|Qt::AlignVCenter){
            auto* it=new QTableWidgetItem(t);it->setTextAlignment(a);if(ci>=4)it->setForeground(clr);m_rankingTable->setItem(r,ci,it);};
        m_rankingTable->setItem(r,0,new QTableWidgetItem(QString::number(r+1)));
        auto* ni=new QTableWidgetItem((en.isContractor()?"  └ ":"")+en.name);
        if(en.isCompany()){QFont f;f.setBold(true);ni->setFont(f);} m_rankingTable->setItem(r,1,ni);
        sc(2,FormatUtils::formatValue(c,cd)); sc(3,FormatUtils::formatValue(p,cd));
        sc(4,(diff>=0?"+":"")+FormatUtils::formatValue(diff,cd));
        sc(5,(pct>999?"新增":QString::number(pct,'f',1)+"%"));
        sc(6,QString::number(qAbs(pct),'f',1));
    }
    m_rankingTable->resizeColumnsToContents();
    m_rankingTable->setColumnWidth(0,50); m_rankingTable->setColumnWidth(1,160);
    for(int c=2;c<7;++c) m_rankingTable->setColumnWidth(c,qMax(75,m_rankingTable->columnWidth(c)));
}

void CompareDialog::onExport() {
    QString path=QFileDialog::getSaveFileName(this,"导出","环比对比_"+QDate::currentDate().toString("yyyyMMdd")+".csv","CSV(*.csv)");
    if(path.isEmpty()) return;
    QFile f(path); if(!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
    QTextStream out(&f); out.setEncoding(QStringConverter::Utf8); out<<"\xEF\xBB\xBF";
    out<<"指标,当期合计,当期日均,对比期合计,对比期日均,差值,环比%\n";
    int dc=qMax(1,m_startCur->date().daysTo(m_endCur->date())+1), dp=qMax(1,m_startPrev->date().daysTo(m_endPrev->date())+1);
    for(const auto& col:m_showCols){ double cv=m_curData.value(col.id,0),pv=m_prevData.value(col.id,0),diff=cv-pv,pct=(pv!=0)?(diff/pv*100.0):0;
        out<<col.displayName<<","<<FormatUtils::formatValue(cv,col)<<","<<FormatUtils::formatValue(cv/dc,col)<<","<<FormatUtils::formatValue(pv,col)<<","<<FormatUtils::formatValue(pv/dp,col)<<","<<(diff>=0?"+":"")<<FormatUtils::formatValue(diff,col)<<","<<QString::number(pct,'f',1)<<"%\n"; }
    f.close(); QMessageBox::information(this,"导出成功","已保存: "+path);
}
