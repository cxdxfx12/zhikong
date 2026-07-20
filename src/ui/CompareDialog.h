#pragma once
#include <QDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include "model/DailyValue.h"

class CompareDialog : public QDialog {
    Q_OBJECT
public:
    explicit CompareDialog(QWidget* parent = nullptr);

private slots:
    void onRefresh();
    void onPeriodChanged(int idx);

private:
    void setupUI();
    void populateEntities();
    void populateMetrics();
    void compute();

    QComboBox* m_periodCombo = nullptr;
    QDateEdit* m_startCur = nullptr;
    QDateEdit* m_endCur = nullptr;
    QDateEdit* m_startPrev = nullptr;
    QDateEdit* m_endPrev = nullptr;
    QTableWidget* m_table = nullptr;

    QVBoxLayout* m_entityLayout = nullptr;
    QVBoxLayout* m_metricLayout = nullptr;
    QVector<QCheckBox*> m_entityChecks;
    QVector<QCheckBox*> m_metricChecks;
};
