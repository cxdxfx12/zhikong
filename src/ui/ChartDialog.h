#pragma once
#include <QDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QChartView>
#include <QChart>
#include <QTableWidget>
#include "model/DailyValue.h"

class ChartDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChartDialog(QWidget* parent = nullptr);

private slots:
    void onRefresh();
    void onExportImage();
    void onPresetDate(int idx);
    void onChartTypeChanged(int idx);

private:
    void setupUI();
    void buildChart();
    void populateEntities();
    void populateMetrics();
    void styleChart(QChart* c);
    void showDataTable(const QVector<DisplayRow>& rows);

    QChart* buildLineChart(bool smooth);
    QChart* buildBarChart(bool stacked);
    QChart* buildComboChart();
    QChart* buildPieChart();
    QChart* buildTrendChart();

    // Top controls
    QComboBox* m_chartTypeCombo = nullptr;
    QComboBox* m_presetCombo = nullptr;
    QDateEdit* m_startDate = nullptr;
    QDateEdit* m_endDate = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;

    // Entity & metric selectors
    QVBoxLayout* m_entityLayout = nullptr;
    QVBoxLayout* m_metricLayout = nullptr;
    QVector<QCheckBox*> m_entityChecks;
    QVector<QCheckBox*> m_metricChecks;

    // Combo chart: bar vs line metric selectors
    QComboBox* m_barMetricCombo = nullptr;
    QComboBox* m_lineMetricCombo = nullptr;

    // Trend line
    QCheckBox* m_trendCheck = nullptr;

    QChartView* m_chartView = nullptr;
    QTableWidget* m_dataTable = nullptr;

    QVector<DisplayRow> m_currentRows;
};
