#pragma once
#include <QAbstractTableModel>
#include <QDate>
#include <QVector>
#include "model/DailyValue.h"
#include "model/ColumnDef.h"
#include "model/Entity.h"

class DisplayDataModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit DisplayDataModel(QObject* parent = nullptr);

    // QAbstractTableModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Custom methods
    void load(const QDate& start, const QDate& end, int entityTypeFilter = 0);
    void setCategoryFilter(const QString& category);
    void setShowStats(bool show);
    bool showStats() const { return m_showStats; }
    QString categoryFilter() const { return m_categoryFilter; }
    void clear();
    void refresh();

    DisplayRow rowAt(int row) const;
    QVector<ColumnDef> displayedColumns() const { return m_columns; }
    int columnIdForSection(int section) const;

    // For category header: returns column index → category name
    QMap<int, QString> columnCategoryMap() const;

    // Extra columns before the dynamic columns
    enum FixedColumn {
        ColDate = 0,
        ColType,
        ColName,
        FixedColumnCount
    };

signals:
    void dataLoaded(int rowCount);

private:
    void reloadColumns();

    QVector<DisplayRow> m_rows;
    QVector<DisplayRow> m_statRows;    // summary statistic rows
    QVector<ColumnDef> m_columns;
    bool m_showStats = true;
    QDate m_startDate, m_endDate;
    int m_entityTypeFilter = 0;
    QString m_categoryFilter;
};
