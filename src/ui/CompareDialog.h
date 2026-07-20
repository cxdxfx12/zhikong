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

    // Helper: is this indicator "bigger is better"?
    bool isPositive(const QString& key) const;
    // Helper: get color for change direction
    QColor changeColor(double diff, const ColumnDef& col) const;

    // Controls
    QComboBox* m_periodCombo = nullptr;
    QDateEdit* m_startCur = nullptr, *m_endCur = nullptr;
    QDateEdit* m_startPrev = nullptr, *m_endPrev = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QLabel* m_periodLabel = nullptr;

    // Summary cards (4 quadrants)
    struct CardWidget { QLabel* title; QLabel* value; QLabel* change; QLabel* status; };
    CardWidget m_cardEff, m_cardQuality, m_cardService, m_cardVolume;

    // Tabs
    QTabWidget* m_tabWidget = nullptr;
    QTableWidget* m_metricsTable = nullptr;
    QTableWidget* m_rankingTable = nullptr;

    // Entity & metric selectors
    QVector<QCheckBox*> m_entityChecks;
    QVector<QCheckBox*> m_metricChecks;
    QVBoxLayout* m_entityLayout2 = nullptr;
    QVBoxLayout* m_metricLayout2 = nullptr;

    // Cached data
    QVector<ColumnDef> m_showCols;
    QMap<int,double> m_curData, m_prevData;

    // Positive indicator keys
    QSet<QString> m_positiveKeys;
};
