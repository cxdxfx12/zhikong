#include "CategoryHeaderView.h"
#include <QPainter>
#include <QFontMetrics>

CategoryHeaderView::CategoryHeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView(orientation, parent) {
    setSectionsClickable(true);
    setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

QSize CategoryHeaderView::sizeHint() const {
    QSize base = QHeaderView::sizeHint();
    base.setHeight(base.height() + m_topRowHeight);
    return base;
}

void CategoryHeaderView::setColumnCategories(const QMap<int, QString>& colToCat) {
    m_colToCat = colToCat;
    m_catRanges.clear();

    // Build category → (firstCol, lastCol) ranges
    QString lastCat;
    int rangeStart = -1;
    for (int i = 0; i <= count(); ++i) {
        QString cat = m_colToCat.value(i, "");
        if (i == count() || cat != lastCat) {
            if (rangeStart >= 0 && !lastCat.isEmpty()) {
                m_catRanges[lastCat] = {rangeStart, i - 1};
            }
            rangeStart = i;
            lastCat = cat;
        }
    }

    // Reserve extra vertical space
    setMinimumHeight(m_bottomRowOffset + 20);
}

void CategoryHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const {
    // Draw the individual column name in the bottom portion
    QRect bottomRect = rect;
    bottomRect.setTop(rect.top() + m_topRowHeight);

    painter->save();

    // Background
    painter->fillRect(rect, QColor("#e8ecf1"));

    // Bottom divider
    painter->setPen(QColor("#c0c7cf"));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    // Section borders
    painter->drawLine(rect.topRight(), rect.bottomRight());

    // Column name text
    QString colName = model() ? model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString() : "";
    painter->setPen(QColor("#2d3436"));
    QFont font = painter->font();
    font.setPointSize(9);
    font.setBold(false);
    painter->setFont(font);
    painter->drawText(bottomRect.adjusted(8, 2, -4, -2), Qt::AlignLeft | Qt::AlignVCenter, colName);

    painter->restore();
}

void CategoryHeaderView::paintEvent(QPaintEvent* event) {
    QHeaderView::paintEvent(event);

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Draw category header row
    QFont catFont = painter.font();
    catFont.setPointSize(10);
    catFont.setBold(true);
    painter.setFont(catFont);

    // Colors for each category
    QMap<QString, QColor> catColors;
    catColors["业务"] = QColor("#3498db");
    catColors["客服"] = QColor("#e74c3c");
    catColors["操作"] = QColor("#27ae60");
    catColors["小件员取派签质量"] = QColor("#8e44ad");
    catColors["运营"] = QColor("#e67e22");

    int defaultColorIdx = 0;

    for (auto it = m_catRanges.begin(); it != m_catRanges.end(); ++it) {
        const QString& cat = it.key();
        int firstCol = it.value().first;
        int lastCol = it.value().second;

        int x1 = sectionViewportPosition(firstCol);
        int x2 = sectionViewportPosition(lastCol) + sectionSize(lastCol);
        QRect catRect(x1, 0, x2 - x1, m_topRowHeight);

        // Category background
        QColor bg;
        if (catColors.contains(cat)) { bg = catColors[cat]; }
        else { bg = QColor::fromHsl((defaultColorIdx * 60) % 360, 180, 200); defaultColorIdx++; }
        painter.fillRect(catRect, bg);
        painter.setPen(Qt::white);
        painter.drawText(catRect.adjusted(12, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, "▎" + cat);

        // Category separator line
        painter.setPen(bg.darker(130));
        painter.drawLine(catRect.bottomLeft(), catRect.bottomRight());
    }

    // Horizontal divider between category row and column row
    painter.setPen(QColor("#bdc3c7"));
    painter.drawLine(0, m_topRowHeight, viewport()->width(), m_topRowHeight);
}
