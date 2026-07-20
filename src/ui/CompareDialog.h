#pragma once
#include <QDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QTableWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QMap>
#include "model/DailyValue.h"
#include "model/ColumnDef.h"

class CompareDialog : public QDialog {
    Q_OBJECT
public:
    explicit CompareDialog(QWidget* parent = nullptr);

private slots:
    void onRefresh();
    void onPeriodChanged(int idx);
    void onExport();

private:
    void setupUI();
    void populateEntities();
    void populateMetrics();
    void compute();
    void buildSummaryCards(const QMap<int,double>& cur, const QMap<int,double>& prev);
    void buildMetricsTable(const QMap<int,double>& cur, const QMap<int,double>& prev);
    void buildRankingTable(const QMap<int,double>& cur, const QMap<int,double>& prev);
    QMap<int,double> queryPeriod(int entTypeFilter, const QDate& start, const QDate& end);

    // Top controls
    QComboBox* m_periodCombo = nullptr;
    QDateEdit* m_startCur = nullptr;
    QDateEdit* m_endCur = nullptr;
    QDateEdit* m_startPrev = nullptr;
    QDateEdit* m_endPrev = nullptr;
    QPushButton* m_refreshBtn = nullptr;

    // Summary cards
    QLabel* m_cardOutbound = nullptr;
    QLabel* m_cardInbound = nullptr;
    QLabel* m_cardSignRate = nullptr;
    QLabel* m_cardWarning = nullptr;
    QLabel* m_periodLabel = nullptr;

    // Tabs
    QTabWidget* m_tabWidget = nullptr;
    QTableWidget* m_metricsTable = nullptr;
    QTableWidget* m_rankingTable = nullptr;

    // Entity & metric selectors
    QVector<QCheckBox*> m_entityChecks;
    QVector<QCheckBox*> m_metricChecks;

    // Cached data for export
    QVector<ColumnDef> m_showCols;
    QMap<int,double> m_curData, m_prevData;
};
