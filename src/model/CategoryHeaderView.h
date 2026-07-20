#pragma once
#include <QHeaderView>
#include <QPainter>
#include <QMap>
#include <QVector>

class CategoryHeaderView : public QHeaderView {
    Q_OBJECT
public:
    explicit CategoryHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr);

    // Set column → category mapping (column index → category name)
    void setColumnCategories(const QMap<int, QString>& colToCat);

    QSize sizeHint() const override;

protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
    void paintEvent(QPaintEvent* event) override;

private:
    QMap<int, QString> m_colToCat;    // logical index → category
    QMap<QString, QPair<int,int>> m_catRanges;  // category → (firstCol, lastCol)
    int m_topRowHeight = 28;
    int m_bottomRowOffset = 28;
};
