#include "ChartDialog.h"
#include "db/EntityDao.h"
#include "db/ColumnDao.h"
#include "db/DailyDao.h"
#include "service/ChartService.h"
#include "utils/FormatUtils.h"
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QLineSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QPieSeries>
#include <QStackedBarSeries>
#include <QBarCategoryAxis>
#include <QSplineSeries>
#include <QPixmap>
#include <QSplitter>
#include <QLabel>
#include <QtCharts>

static const QColor PBI[] = {"#118DFF","#FE9666","#13CB9D","#9B6BFF","#F9CE4B","#38C6F4","#FF6E7A","#55D98D","#C084FC","#F4A261"};
static const int PBI_N = 10;

ChartDialog::ChartDialog(QWidget* parent) : QDialog(parent) {
    setupUI(); setWindowTitle("数据分析图表"); resize(1280, 760);
}

void ChartDialog::setupUI() {
    auto* ml = new QHBoxLayout(this); ml->setSpacing(0);

    // === Left Panel ===
    auto* lw = new QWidget(); lw->setFixedWidth(270);
    lw->setStyleSheet("background:#f7f8fa; border-right:1px solid #e0e0e0;");
    auto* lp = new QVBoxLayout(lw); lp->setSpacing(6); lp->setContentsMargins(10,10,10,10);

    // Chart type
    lp->addWidget(new QLabel("<b>图表类型</b>"));
    m_chartTypeCombo = new QComboBox();
    m_chartTypeCombo->addItems({"折线图","平滑曲线","柱状图","堆叠柱状","组合图","趋势对比","饼图"});
    lp->addWidget(m_chartTypeCombo);

    // Date preset
    lp->addWidget(new QLabel("<b>日期范围</b>"));
    m_presetCombo = new QComboBox();
    m_presetCombo->addItems({"最近7天","最近30天","本月","上月","本季","全部"});
    lp->addWidget(m_presetCombo);
    auto* dr = new QHBoxLayout();
    m_startDate = new QDateEdit(QDate::currentDate().addDays(-7));
    m_startDate->setCalendarPopup(true); m_startDate->setDisplayFormat("yyyy-MM-dd");
    dr->addWidget(m_startDate); dr->addWidget(new QLabel("~"));
    m_endDate = new QDateEdit(QDate::currentDate());
    m_endDate->setCalendarPopup(true); m_endDate->setDisplayFormat("yyyy-MM-dd");
    dr->addWidget(m_endDate); lp->addLayout(dr);

    // Combo chart metric selectors (hidden by default)
    auto* comboBox = new QWidget();
    auto* cl = new QVBoxLayout(comboBox); cl->setContentsMargins(0,0,0,0);
    cl->addWidget(new QLabel("<b>组合图: 柱指标</b>"));
    m_barMetricCombo = new QComboBox(); cl->addWidget(m_barMetricCombo);
    cl->addWidget(new QLabel("<b>组合图: 线指标</b>"));
    m_lineMetricCombo = new QComboBox(); cl->addWidget(m_lineMetricCombo);
    comboBox->setVisible(false);
    lp->addWidget(comboBox);

    // Trend line
    m_trendCheck = new QCheckBox("显示趋势线");
    lp->addWidget(m_trendCheck);

    // Entities
    auto* eg = new QGroupBox("实体选择");
    m_entityLayout = new QVBoxLayout(eg); m_entityLayout->setSpacing(1);
    populateEntities();
    auto* es = new QScrollArea(); es->setWidget(eg); es->setWidgetResizable(true); es->setMaximumHeight(140);
    lp->addWidget(es);

    // Metrics
    auto* mg = new QGroupBox("指标选择");
    m_metricLayout = new QVBoxLayout(mg); m_metricLayout->setSpacing(1);
    populateMetrics();
    auto* ms = new QScrollArea(); ms->setWidget(mg); ms->setWidgetResizable(true);
    lp->addWidget(ms, 1);

    m_refreshBtn = new QPushButton("刷新图表");
    m_refreshBtn->setStyleSheet("QPushButton{background:#118DFF;color:white;border:none;padding:8px;border-radius:4px;font-weight:bold;} QPushButton:hover{background:#0B6ED4;}");
    lp->addWidget(m_refreshBtn);
    m_exportBtn = new QPushButton("导出图片"); lp->addWidget(m_exportBtn);
    ml->addWidget(lw);

    // === Right: Chart + Data Table ===
    auto* rightWidget = new QWidget();
    auto* rl = new QVBoxLayout(rightWidget); rl->setContentsMargins(0,0,0,0); rl->setSpacing(0);

    m_chartView = new QChartView(); m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setStyleSheet("background:white;");
    rl->addWidget(m_chartView, 3);

    m_dataTable = new QTableWidget();
    m_dataTable->setMaximumHeight(160);
    m_dataTable->setAlternatingRowColors(true);
    rl->addWidget(m_dataTable, 1);

    ml->addWidget(rightWidget, 1);

    // Connections
    connect(m_refreshBtn, &QPushButton::clicked, this, &ChartDialog::onRefresh);
    connect(m_exportBtn, &QPushButton::clicked, this, &ChartDialog::onExportImage);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChartDialog::onPresetDate);
    connect(m_chartTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChartDialog::onChartTypeChanged);
    connect(m_startDate, &QDateEdit::dateChanged, this, &ChartDialog::onRefresh);
    connect(m_endDate, &QDateEdit::dateChanged, this, &ChartDialog::onRefresh);

    if(!m_entityChecks.isEmpty()) m_entityChecks.first()->setChecked(true);
    if(!m_metricChecks.isEmpty()) m_metricChecks.first()->setChecked(true);
    onPresetDate(0);
}

void ChartDialog::populateEntities() {
    for(auto* cb:m_entityChecks) delete cb; m_entityChecks.clear();
    auto tree = EntityDao::getTree();
    for(const auto& comp:tree) {
        auto* cb = new QCheckBox(comp.entity.name); cb->setProperty("eid",comp.entity.id);
        m_entityChecks<<cb; m_entityLayout->addWidget(cb);
        for(const auto& ctr:comp.children) {
            auto* cb2 = new QCheckBox("  "+ctr.entity.name); cb2->setProperty("eid",ctr.entity.id);
            m_entityChecks<<cb2; m_entityLayout->addWidget(cb2);
        }
    }
}

void ChartDialog::populateMetrics() {
    for(auto* cb:m_metricChecks) delete cb; m_metricChecks.clear();
    m_barMetricCombo->clear(); m_lineMetricCombo->clear();
    for(const auto& col:ColumnDao::getAll(false)) {
        auto* cb = new QCheckBox(col.displayNameWithUnit()); cb->setProperty("cid",col.id);
        m_metricChecks<<cb; m_metricLayout->addWidget(cb);
        m_barMetricCombo->addItem(col.displayNameWithUnit(), col.id);
        m_lineMetricCombo->addItem(col.displayNameWithUnit(), col.id);
    }
    if(m_lineMetricCombo->count()>1) m_lineMetricCombo->setCurrentIndex(1);
}

void ChartDialog::onPresetDate(int idx) {
    QDate today=QDate::currentDate();
    switch(idx) {
    case 0: m_startDate->setDate(today.addDays(-7)); m_endDate->setDate(today); break;
    case 1: m_startDate->setDate(today.addDays(-30)); m_endDate->setDate(today); break;
    case 2: m_startDate->setDate(QDate(today.year(),today.month(),1)); m_endDate->setDate(today); break;
    case 3: m_startDate->setDate(QDate(today.year(),today.month(),1).addMonths(-1));
            m_endDate->setDate(QDate(today.year(),today.month(),1).addDays(-1)); break;
    case 4: { int q=(today.month()-1)/3; m_startDate->setDate(QDate(today.year(),q*3+1,1)); m_endDate->setDate(today); break; }
    case 5: m_startDate->setDate(DailyDao::getEarliestDate()); m_endDate->setDate(DailyDao::getLatestDate()); break;
    }
}

void ChartDialog::onChartTypeChanged(int idx) {
    // Show/hide combo metric selectors
    auto* comboBox = m_barMetricCombo->parentWidget();
    if(comboBox) comboBox->setVisible(idx == 4); // only for combo chart
}

void ChartDialog::onRefresh() { buildChart(); }

static QVector<int> getEids(const QVector<QCheckBox*>& cbs) { QVector<int> r; for(auto* cb:cbs) if(cb->isChecked()) r<<cb->property("eid").toInt(); return r; }
static QVector<int> getCids(const QVector<QCheckBox*>& cbs) { QVector<int> r; for(auto* cb:cbs) if(cb->isChecked()) r<<cb->property("cid").toInt(); return r; }

void ChartDialog::styleChart(QChart* c) {
    c->setBackgroundBrush(Qt::white); c->setBackgroundRoundness(6);
    c->legend()->setAlignment(Qt::AlignBottom); c->legend()->setFont(QFont("Segoe UI",9));
    c->legend()->setLabelColor(QColor("#555"));
    c->setTitleFont(QFont("Segoe UI",13,QFont::Bold)); c->setTitleBrush(QColor("#333"));
    for(auto* a:c->axes()) {
        a->setGridLineColor(QColor("#e8e8e8")); a->setGridLinePen(QPen(QColor("#e8e8e8"),0.5,Qt::DotLine));
        a->setLinePen(QPen(QColor("#ccc"),0.5)); a->setLabelsColor(QColor("#777"));
        a->setLabelsFont(QFont("Segoe UI",9)); a->setTitleFont(QFont("Segoe UI",9,QFont::Bold));
    }
}

void ChartDialog::showDataTable(const QVector<DisplayRow>& rows) {
    if(rows.isEmpty()) { m_dataTable->setRowCount(0); return; }
    auto cols = ColumnDao::getAll(false);
    auto selCids = getCids(m_metricChecks);
    QVector<ColumnDef> showCols;
    for(const auto& col:cols) {
        if(!selCids.contains(col.id)) continue;
        showCols<<col;
    }

    m_dataTable->setColumnCount(2+showCols.size());
    QStringList hdrs; hdrs<<"日期"<<"实体";
    for(const auto& c:showCols) hdrs<<c.displayName;
    m_dataTable->setHorizontalHeaderLabels(hdrs);
    m_dataTable->setRowCount(qMin(20, rows.size()));
    for(int r=0; r<qMin(20,rows.size()); ++r) {
        m_dataTable->setItem(r,0,new QTableWidgetItem(rows[r].date.toString("yyyy-MM-dd")));
        m_dataTable->setItem(r,1,new QTableWidgetItem(rows[r].entityName));
        for(int c=0; c<showCols.size(); ++c) {
            double v=rows[r].values.value(showCols[c].id, 0);
            m_dataTable->setItem(r,2+c,new QTableWidgetItem(FormatUtils::formatValue(v, showCols[c])));
        }
    }
    m_dataTable->resizeColumnsToContents();
}

void ChartDialog::buildChart() {
    int t=m_chartTypeCombo->currentIndex();
    QChart* c=nullptr;
    if(t<=1) c=buildLineChart(t==1);
    else if(t<=3) c=buildBarChart(t==3);
    else if(t==4) c=buildComboChart();
    else if(t==5) c=buildTrendChart();
    else c=buildPieChart();
    if(c){ auto* old=m_chartView->chart(); styleChart(c); m_chartView->setChart(c); delete old; }
}

QChart* ChartDialog::buildLineChart(bool smooth) {
    auto eids=getEids(m_entityChecks), cids=getCids(m_metricChecks);
    auto* c=new QChart();
    if(eids.isEmpty()||cids.isEmpty()){c->setTitle("请选择实体和指标");return c;}
    auto all=ChartService::prepareSeries(m_startDate->date(),m_endDate->date(),eids,cids);
    m_currentRows = DailyDao::queryForChart(m_startDate->date(),m_endDate->date(),eids,cids);
    showDataTable(m_currentRows);
    if(all.isEmpty()){c->setTitle(QString("无数据: 实体%1 指标%2 行%3").arg(eids.size()).arg(cids.size()).arg(m_currentRows.size()));return c;}
    c->setTitle(all.first().columnName+" 趋势");
    auto* ax=new QDateTimeAxis(); ax->setFormat("M/d"); c->addAxis(ax,Qt::AlignBottom);
    auto* ay=new QValueAxis(); c->addAxis(ay,Qt::AlignLeft);
    for(int i=0;i<all.size();++i){
        const auto& s=all[i]; if(s.points.isEmpty()) continue;
        auto* line=smooth?static_cast<QLineSeries*>(new QSplineSeries()):new QLineSeries();
        line->setName(s.entityName); line->setPen(QPen(PBI[i%PBI_N],2.5));
        line->setPointLabelsVisible(true); line->setPointLabelsFormat("@yPoint");
        line->setPointLabelsFont(QFont("Segoe UI",6)); line->setPointLabelsColor(QColor("#999"));
        for(const auto&[d,v]:s.points) line->append(QDateTime(d,QTime(0,0)).toMSecsSinceEpoch(),v);
        c->addSeries(line); line->attachAxis(ax); line->attachAxis(ay);
    }
    return c;
}

QChart* ChartDialog::buildBarChart(bool stacked) {
    auto eids=getEids(m_entityChecks), cids=getCids(m_metricChecks);
    auto* c=new QChart();
    if(eids.isEmpty()||cids.isEmpty()){c->setTitle("请选择实体和指标");return c;}
    auto all=ChartService::prepareSeries(m_startDate->date(),m_endDate->date(),eids,cids);
    m_currentRows = DailyDao::queryForChart(m_startDate->date(),m_endDate->date(),eids,cids);
    showDataTable(m_currentRows);
    if(all.isEmpty()){c->setTitle(QString("无数据: 实体%1 指标%2 行%3").arg(eids.size()).arg(cids.size()).arg(m_currentRows.size()));return c;}
    c->setTitle(all.first().columnName+" 对比");
    auto* bs=stacked?static_cast<QAbstractBarSeries*>(new QStackedBarSeries()):new QBarSeries();
    bs->setLabelsVisible(true); bs->setLabelsFormat("@value");
    QStringList cats;
    for(int i=0;i<all.size();++i){
        auto* set=new QBarSet(all[i].entityName); set->setColor(PBI[i%PBI_N]);
        set->setLabelFont(QFont("Segoe UI",5)); set->setLabelColor(QColor("#999"));
        for(const auto&[d,v]:all[i].points){ *set<<v; QString ct=d.toString("M/d"); if(!cats.contains(ct)) cats<<ct; }
        bs->append(set);
    }
    c->addSeries(bs); auto* ax=new QBarCategoryAxis(); ax->append(cats); c->addAxis(ax,Qt::AlignBottom);
    bs->attachAxis(ax); auto* ay=new QValueAxis(); c->addAxis(ay,Qt::AlignLeft); bs->attachAxis(ay);
    return c;
}

QChart* ChartDialog::buildComboChart() {
    auto eids=getEids(m_entityChecks);
    auto* c=new QChart();
    if(eids.isEmpty()){c->setTitle("请选择实体");return c;}

    int barCid=m_barMetricCombo->currentData().toInt();
    int lineCid=m_lineMetricCombo->currentData().toInt();
    if(barCid<=0||lineCid<=0){c->setTitle("请选择柱指标和线指标");return c;}

    auto barData=ChartService::prepareSeries(m_startDate->date(),m_endDate->date(),eids,{barCid});
    auto lineData=ChartService::prepareSeries(m_startDate->date(),m_endDate->date(),eids,{lineCid});
    m_currentRows = DailyDao::queryForChart(m_startDate->date(),m_endDate->date(),eids,{barCid,lineCid});
    showDataTable(m_currentRows);

    QString barName=barData.isEmpty()?"指标1":barData.first().columnName;
    QString lineName=lineData.isEmpty()?"指标2":lineData.first().columnName;
    c->setTitle(barName+" + "+lineName);

    auto* ayBar=new QValueAxis(); ayBar->setTitleText(barName); c->addAxis(ayBar,Qt::AlignLeft);
    auto* ayLine=new QValueAxis(); ayLine->setTitleText(lineName); c->addAxis(ayLine,Qt::AlignRight);

    QStringList cats; QMap<QString,int> catIdx;
    for(const auto& s:barData) for(const auto&[d,_]:s.points){ QString k=d.toString("M/d"); if(!catIdx.contains(k)){catIdx[k]=cats.size();cats<<k;}}
    for(const auto& s:lineData) for(const auto&[d,_]:s.points){ QString k=d.toString("M/d"); if(!catIdx.contains(k)){catIdx[k]=cats.size();cats<<k;}}
    auto* ax=new QBarCategoryAxis(); ax->append(cats); c->addAxis(ax,Qt::AlignBottom);

    auto* bs=new QBarSeries(); bs->setLabelsVisible(true); bs->setLabelsFormat("@value");
    for(int i=0;i<barData.size();++i){
        auto* set=new QBarSet(barData[i].entityName); set->setColor(PBI[i%PBI_N]);
        set->setLabelFont(QFont("Segoe UI",5)); set->setLabelColor(QColor("#999"));
        for(const auto&[d,v]:barData[i].points) *set<<v;
        bs->append(set);
    }
    c->addSeries(bs); bs->attachAxis(ax); bs->attachAxis(ayBar);

    for(int i=0;i<lineData.size();++i){
        const auto& s=lineData[i]; if(s.points.isEmpty()) continue;
        auto* line=new QSplineSeries(); line->setName(s.entityName+"·"+lineName);
        line->setPen(QPen(PBI[(barData.size()+i)%PBI_N],3));
        line->setPointLabelsVisible(true); line->setPointLabelsFormat("@yPoint");
        line->setPointLabelsFont(QFont("Segoe UI",6)); line->setPointLabelsColor(QColor("#999"));
        for(const auto&[d,v]:s.points) line->append(catIdx.value(d.toString("M/d"),0),v);
        c->addSeries(line); line->attachAxis(ax); line->attachAxis(ayLine);
    }
    return c;
}

QChart* ChartDialog::buildTrendChart() {
    auto eids=getEids(m_entityChecks), cids=getCids(m_metricChecks);
    auto* c=new QChart();
    if(eids.isEmpty()||cids.isEmpty()){c->setTitle("请选择实体和指标");return c;}
    // Use only first metric for trend line clarity
    QVector<int> oneCid={cids.first()};
    auto all=ChartService::prepareSeries(m_startDate->date(),m_endDate->date(),eids,oneCid);
    m_currentRows = DailyDao::queryForChart(m_startDate->date(),m_endDate->date(),eids,oneCid);
    showDataTable(m_currentRows);
    if(all.isEmpty()){c->setTitle(QString("无数据: 实体%1 指标%2 行%3").arg(eids.size()).arg(cids.size()).arg(m_currentRows.size()));return c;}
    c->setTitle(all.first().columnName+" 实体对比趋势");

    auto* ax=new QDateTimeAxis(); ax->setFormat("M/d"); c->addAxis(ax,Qt::AlignBottom);
    auto* ay=new QValueAxis(); c->addAxis(ay,Qt::AlignLeft);

    for(int i=0;i<all.size();++i){
        const auto& s=all[i]; if(s.points.isEmpty()) continue;
        // Actual values as dots
        auto* scatter=new QScatterSeries(); scatter->setName(s.entityName);
        scatter->setColor(PBI[i%PBI_N]); scatter->setMarkerSize(7);
        scatter->setPointLabelsVisible(true); scatter->setPointLabelsFormat("@yPoint");
        scatter->setPointLabelsFont(QFont("Segoe UI",6)); scatter->setPointLabelsColor(QColor("#999"));
        for(const auto&[d,v]:s.points) scatter->append(QDateTime(d,QTime(0,0)).toMSecsSinceEpoch(),v);
        c->addSeries(scatter); scatter->attachAxis(ax); scatter->attachAxis(ay);

        // Trend line (simple linear regression)
        if(m_trendCheck->isChecked() && s.points.size()>=3) {
            double sx=0,sy=0,sxx=0,sxy=0; int n=s.points.size();
            for(int j=0;j<n;++j){ double x=j,y=s.points[j].second; sx+=x; sy+=y; sxx+=x*x; sxy+=x*y; }
            double slope=(n*sxy-sx*sy)/(n*sxx-sx*sx);
            double intercept=(sy-slope*sx)/n;
            auto* trend=new QLineSeries(); trend->setName(s.entityName+" 趋势");
            QPen tp(PBI[i%PBI_N],1.5,Qt::DashLine); trend->setPen(tp);
            trend->append(QDateTime(s.points.first().first,QTime(0,0)).toMSecsSinceEpoch(),intercept);
            trend->append(QDateTime(s.points.last().first,QTime(0,0)).toMSecsSinceEpoch(),intercept+slope*(n-1));
            c->addSeries(trend); trend->attachAxis(ax); trend->attachAxis(ay);
        }
    }
    return c;
}

QChart* ChartDialog::buildPieChart() {
    auto eids=getEids(m_entityChecks), cids=getCids(m_metricChecks);
    auto* c=new QChart();
    if(eids.isEmpty()||cids.isEmpty()){c->setTitle("请选择实体和指标");return c;}
    auto all=ChartService::prepareSeries(m_startDate->date(),m_endDate->date(),eids,cids);
    if(all.isEmpty()){c->setTitle(QString("无数据: 实体%1 指标%2 行%3").arg(eids.size()).arg(cids.size()).arg(m_currentRows.size()));return c;}
    c->setTitle(all.first().columnName+" 占比");
    auto* pie=new QPieSeries(); pie->setLabelsVisible(true);
    for(int i=0;i<all.size();++i){
        if(!all[i].points.isEmpty()){
            auto* sl=pie->append(all[i].entityName,all[i].points.first().second);
            sl->setColor(PBI[i%PBI_N]); sl->setLabelColor(QColor("#555"));
            sl->setLabelFont(QFont("Segoe UI",7));
        }
    }
    pie->setLabelsPosition(QPieSlice::LabelOutside);
    for(auto* sl:pie->slices()) {
        sl->setLabel(QString("%1\n%2").arg(sl->label()).arg(QString::number(sl->value(),'f',0)));
        sl->setLabelFont(QFont("Segoe UI",6));
    }
    c->addSeries(pie); return c;
}

void ChartDialog::onExportImage() {
    QString path=QFileDialog::getSaveFileName(this,"导出图表","chart_"+QDate::currentDate().toString("yyyyMMdd")+".png","PNG (*.png)");
    if(path.isEmpty()) return;
    if(m_chartView->grab().save(path)) QMessageBox::information(this,"成功","图表已保存");
    else QMessageBox::warning(this,"失败","无法保存图片");
}
