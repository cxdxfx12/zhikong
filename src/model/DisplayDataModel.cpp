#include "DisplayDataModel.h"
#include "db/DailyDao.h"
#include "db/ColumnDao.h"
#include "db/EntityDao.h"
#include "service/AggregationService.h"
#include "utils/FormatUtils.h"
#include <QColor>
#include <QFont>

DisplayDataModel::DisplayDataModel(QObject* parent)
    : QAbstractTableModel(parent) {
    reloadColumns();
}

void DisplayDataModel::reloadColumns() {
    if (!m_categoryFilter.isEmpty()) {
        // Filter by category — show ALL columns of that category
        m_columns = ColumnDao::getAll(false);
        QVector<ColumnDef> filtered;
        for (const auto& col : m_columns) {
            if (col.category == m_categoryFilter)
                filtered.append(col);
        }
        m_columns = filtered;
    } else {
        // "全部" — show ALL columns from all categories
        m_columns = ColumnDao::getAll(false);
    }
}

void DisplayDataModel::setCategoryFilter(const QString& category) {
    m_categoryFilter = category;
    refresh();
}

int DisplayDataModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_rows.size() + (m_showStats ? m_statRows.size() : 0);
}

int DisplayDataModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return FixedColumnCount + m_columns.size();
}

QVariant DisplayDataModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};

    // Check for stat rows
    bool isStatRow = (m_showStats && index.row() >= m_rows.size());
    int statIdx = isStatRow ? (index.row() - m_rows.size()) : -1;

    if (!isStatRow && index.row() >= m_rows.size()) return {};

    const auto& row = isStatRow ? m_statRows[statIdx] : m_rows[index.row()];
    int col = index.column();

    if (role == Qt::DisplayRole) {
        if (col == ColDate) {
            if (isStatRow) return {};
            return FormatUtils::dateToString(row.date);
        }
        if (col == ColType) {
            if (isStatRow) return {};
            return row.entityType;
        }
        if (col == ColName) {
            if (isStatRow) return row.entityName;
            if (row.isContractor()) return "  " + row.entityName;
            return row.entityName;
        }

        // Dynamic column
        int colIdx = col - FixedColumnCount;
        if (colIdx >= 0 && colIdx < m_columns.size()) {
            const auto& colDef = m_columns[colIdx];
            if (row.values.contains(colDef.id)) {
                return FormatUtils::formatValue(row.values[colDef.id], colDef);
            }
        }
        return {};
    }

    // Background color: grey for aggregated company fields
    if (role == Qt::BackgroundRole) {
        if (row.isCompany() && col >= FixedColumnCount) {
            int colIdx = col - FixedColumnCount;
            if (colIdx >= 0 && colIdx < m_columns.size()) {
                const auto& colDef = m_columns[colIdx];
                if (colDef.needsAggregation()) {
                    return QColor(240, 240, 240); // light grey
                }
            }
        }
    }

    // Tooltip for aggregated cells
    if (role == Qt::ToolTipRole) {
        if (row.isCompany() && col >= FixedColumnCount) {
            int colIdx = col - FixedColumnCount;
            if (colIdx >= 0 && colIdx < m_columns.size()) {
                const auto& colDef = m_columns[colIdx];
                if (colDef.needsAggregation()) {
                    return QString("此数据由承包区自动汇总（%1），不可手动编辑")
                        .arg(colDef.aggregateType == "SUM" ? "求和" : "平均");
                }
            }
        }
    }

    // Text alignment: numbers right-aligned
    if (role == Qt::TextAlignmentRole) {
        if (col >= FixedColumnCount) return QVariant::fromValue(int(Qt::AlignRight | Qt::AlignVCenter));
        return QVariant::fromValue(int(Qt::AlignLeft | Qt::AlignVCenter));
    }

    // Bold for company rows + extra bold for stat rows
    if (role == Qt::FontRole) {
        QFont font;
        if (isStatRow) { font.setBold(true); font.setItalic(true); return font; }
        if (row.isCompany()) { font.setBold(true); return font; }
    }

    // Colored text for stat rows
    if (isStatRow && role == Qt::ForegroundRole) {
        return (statIdx == 0) ? QColor("#c0392b") : QColor("#2980b9");  // 合计红, 平均蓝
    }
    if (role == Qt::BackgroundRole && isStatRow) {
        return (statIdx == 0) ? QColor("#fdf0ef") : QColor("#edf5fc");  // 合计浅红底, 平均浅蓝底
    }

    return {};
}

QVariant DisplayDataModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal) return {};

    if (role == Qt::DisplayRole) {
        if (section == ColDate) return "日期";
        if (section == ColType) return "类型";
        if (section == ColName) return "名称";

        int colIdx = section - FixedColumnCount;
        if (colIdx >= 0 && colIdx < m_columns.size()) {
            return m_columns[colIdx].displayNameWithUnit();
        }
    }

    return {};
}

Qt::ItemFlags DisplayDataModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;

    // Stat rows are not editable
    if (m_showStats && index.row() >= m_rows.size())
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // Aggregated company cells are not editable
    if (index.column() >= FixedColumnCount) {
        const auto& row = m_rows[index.row()];
        if (row.isCompany()) {
            int colIdx = index.column() - FixedColumnCount;
            if (colIdx >= 0 && colIdx < m_columns.size()) {
                if (m_columns[colIdx].needsAggregation()) {
                    return Qt::ItemIsSelectable | Qt::ItemIsEnabled; // read-only
                }
            }
        }
    }

    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void DisplayDataModel::load(const QDate& start, const QDate& end, int entityTypeFilter) {
    beginResetModel();
    m_startDate = start;
    m_endDate = end;
    m_entityTypeFilter = entityTypeFilter;

    reloadColumns();

    // Collect column IDs for core columns
    QVector<int> colIds;
    for (const auto& c : m_columns) {
        colIds << c.id;
    }

    // Query contractor + manual company data
    m_rows = DailyDao::queryForDisplay(start, end, entityTypeFilter, colIds);

    // Generate company aggregate rows for companies that don't have manual data yet
    QSet<int> existingCompanyIds;
    QSet<QString> existingKeys; // "companyId|date"
    for (const auto& row : m_rows) {
        if (row.isCompany()) {
            existingCompanyIds.insert(row.entityId);
            existingKeys.insert(QString("%1|%2").arg(row.entityId).arg(FormatUtils::dateToString(row.date)));
        }
    }

    // For each company, generate aggregate rows if not already present
    auto companies = entityTypeFilter == 2 ? QVector<Entity>() :
        EntityDao::getCompanies(); // entityTypeFilter=0 or 1
    for (const auto& company : companies) {
        for (QDate d = start; d <= end; d = d.addDays(1)) {
            QString key = QString("%1|%2").arg(company.id).arg(FormatUtils::dateToString(d));
            if (existingKeys.contains(key)) continue;

            auto aggValues = AggregationService::computeCompanyAggregate(
                company.id, FormatUtils::dateToString(d));

            if (!aggValues.isEmpty()) {
                DisplayRow row;
                row.entityId = company.id;
                row.entityName = company.name;
                row.entityType = "公司";
                row.parentCompanyId = 0;
                row.date = d;
                row.values = aggValues;
                m_rows.append(row);
            }
        }
    }

    // Sort: by date desc, then companies first, then contractors by parent
    std::sort(m_rows.begin(), m_rows.end(), [](const DisplayRow& a, const DisplayRow& b) {
        if (a.date != b.date) return a.date > b.date;
        if (a.isCompany() != b.isCompany()) return a.isCompany();
        if (a.parentCompanyId != b.parentCompanyId) return a.parentCompanyId < b.parentCompanyId;
        return a.entityName < b.entityName;
    });

    // --- compute statistics rows ---
    m_statRows.clear();
    if (!m_rows.isEmpty()) {
        // Only count contractor rows (skip company aggregates to avoid double-count)
        QVector<DisplayRow> baseRows;
        for (const auto& r : m_rows) if (r.isContractor()) baseRows.append(r);
        if (baseRows.isEmpty()) baseRows = m_rows; // fallback if no contractors visible

        DisplayRow sumRow; sumRow.entityName = "合计"; sumRow.entityType = "";
        DisplayRow avgRow; avgRow.entityName = "平均"; avgRow.entityType = "";
        for (const auto& col : m_columns) {
            double sum = 0; int cnt = 0;
            for (const auto& r : baseRows) { if (r.values.contains(col.id)) { sum += r.values[col.id]; cnt++; } }
            sumRow.values[col.id] = sum;
            avgRow.values[col.id] = cnt > 0 ? sum / cnt : 0;
        }
        m_statRows.append(sumRow); m_statRows.append(avgRow);
        m_statRows.append(avgRow);
    }

    endResetModel();
    emit dataLoaded(m_rows.size());
}

void DisplayDataModel::setShowStats(bool show) {
    m_showStats = show;
    refresh();
}

void DisplayDataModel::clear() {
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

void DisplayDataModel::refresh() {
    load(m_startDate, m_endDate, m_entityTypeFilter);
}

DisplayRow DisplayDataModel::rowAt(int row) const {
    if (row >= 0 && row < m_rows.size()) return m_rows[row];
    return DisplayRow();
}

int DisplayDataModel::columnIdForSection(int section) const {
    int colIdx = section - FixedColumnCount;
    if (colIdx >= 0 && colIdx < m_columns.size()) {
        return m_columns[colIdx].id;
    }
    return 0;
}

QMap<int, QString> DisplayDataModel::columnCategoryMap() const {
    QMap<int, QString> map;
    map[ColDate] = "";
    map[ColType] = "";
    map[ColName] = "";
    for (int i = 0; i < m_columns.size(); ++i) {
        map[FixedColumnCount + i] = m_columns[i].category;
    }
    return map;
}
