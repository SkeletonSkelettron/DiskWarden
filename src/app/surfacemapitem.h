#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

class SurfaceTest;

// The block map of a surface test: one cell per block, darker green for
// slower blocks, yellow for very slow and red for unreadable ones.
class SurfaceMapItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(SurfaceTest *test READ test WRITE setTest NOTIFY testChanged)
    Q_PROPERTY(int revision READ revision WRITE setRevision NOTIFY revisionChanged)
    Q_PROPERTY(QColor backgroundColor MEMBER m_background NOTIFY colorsChanged)
    Q_PROPERTY(QColor gapColor MEMBER m_gap NOTIFY colorsChanged)
    Q_PROPERTY(QColor untestedColor MEMBER m_untested NOTIFY colorsChanged)
    Q_PROPERTY(QColor fastColor MEMBER m_fast NOTIFY colorsChanged)
    Q_PROPERTY(QColor slowColor MEMBER m_slow NOTIFY colorsChanged)
    Q_PROPERTY(QColor damagedColor MEMBER m_damaged NOTIFY colorsChanged)
    Q_PROPERTY(QColor badColor MEMBER m_bad NOTIFY colorsChanged)
    Q_PROPERTY(QColor activeColor MEMBER m_active NOTIFY colorsChanged)

public:
    explicit SurfaceMapItem(QQuickItem *parent = nullptr);

    SurfaceTest *test() const { return m_test; }
    void setTest(SurfaceTest *test);
    int revision() const { return m_revision; }
    void setRevision(int revision);

    void paint(QPainter *painter) override;

signals:
    void testChanged();
    void revisionChanged();
    void colorsChanged();
    void blockClicked(int index);
    void blockHovered(int index);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;

private:
    int blockCount() const;
    void layout(int &cell, int &columns) const;
    int blockAt(const QPointF &pos) const;

    QPointer<SurfaceTest> m_test;
    int m_revision = 0;
    QColor m_background = QColor(0xff, 0xff, 0xff);
    QColor m_gap = QColor(0xc8, 0xd8, 0xc8);
    QColor m_untested = QColor(0xf4, 0xf4, 0xf4);
    QColor m_fast = QColor(0xb4, 0xec, 0xb0);
    QColor m_slow = QColor(0x1c, 0x6a, 0x1c);
    QColor m_damaged = QColor(0xf2, 0xd3, 0x5c);
    QColor m_bad = QColor(0xe0, 0x46, 0x46);
    QColor m_active = QColor(0x3c, 0x78, 0xd8);
};
