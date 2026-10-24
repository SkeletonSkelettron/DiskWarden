#include "surfacemapitem.h"

#include "surfacetest.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace {

QColor mix(const QColor &a, const QColor &b, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t), float(a.greenF() + (b.greenF() - a.greenF()) * t),
                            float(a.blueF() + (b.blueF() - a.blueF()) * t));
}

} // namespace

SurfaceMapItem::SurfaceMapItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    connect(this, &SurfaceMapItem::colorsChanged, this, [this] { update(); });
}

void SurfaceMapItem::setTest(SurfaceTest *test)
{
    if (m_test == test)
        return;
    m_test = test;
    emit testChanged();
    update();
}

void SurfaceMapItem::setRevision(int revision)
{
    if (m_revision == revision)
        return;
    m_revision = revision;
    emit revisionChanged();
    update();
}

int SurfaceMapItem::blockCount() const
{
    return m_test && m_test->blockCount() > 0 ? m_test->blockCount() : 10000;
}

void SurfaceMapItem::layout(int &cell, int &columns) const
{
    const int n = blockCount();
    const int w = int(width());
    const int h = int(height());
    for (cell = 16; cell > 2; --cell) {
        columns = std::max(1, w / cell);
        const int rows = (n + columns - 1) / columns;
        if (rows * cell <= h)
            return;
    }
    cell = 2;
    columns = std::max(1, w / cell);
}

int SurfaceMapItem::blockAt(const QPointF &pos) const
{
    int cell = 0;
    int columns = 0;
    layout(cell, columns);
    const int column = int(pos.x()) / cell;
    const int row = int(pos.y()) / cell;
    if (column < 0 || column >= columns || row < 0)
        return -1;
    const int index = row * columns + column;
    return index < blockCount() ? index : -1;
}

void SurfaceMapItem::paint(QPainter *painter)
{
    int cell = 0;
    int columns = 0;
    layout(cell, columns);
    const int n = blockCount();
    const int rows = (n + columns - 1) / columns;

    painter->fillRect(boundingRect(), m_background);
    painter->fillRect(QRect(0, 0, columns * cell + 1, rows * cell + 1), m_gap);

    QList<quint8> states;
    QList<float> speeds;
    float maxSpeed = 0;
    int current = -1;
    if (m_test) {
        m_test->blockStates(states, speeds);
        maxSpeed = m_test->maximumSpeed();
        current = m_test->currentBlock();
    }

    const int inner = std::max(1, cell - 1);
    for (int i = 0; i < n; ++i) {
        const int x = (i % columns) * cell + 1;
        const int y = (i / columns) * cell + 1;
        QColor color = m_untested;
        const quint8 state = i < states.size() ? states[i] : quint8(SurfaceTest::Untested);
        if (i == current) {
            color = m_active;
        } else if (state == SurfaceTest::Good) {
            const double ratio = maxSpeed > 0 ? speeds[i] / maxSpeed : 1.0;
            color = mix(m_fast, m_slow, std::pow(1.0 - ratio, 0.8) * 1.4);
        } else if (state == SurfaceTest::Damaged) {
            color = m_damaged;
        } else if (state == SurfaceTest::Bad) {
            color = m_bad;
        }
        painter->fillRect(x, y, inner, inner, color);
    }
}

void SurfaceMapItem::mousePressEvent(QMouseEvent *event)
{
    const int index = blockAt(event->position());
    if (index >= 0)
        emit blockClicked(index);
}

void SurfaceMapItem::hoverMoveEvent(QHoverEvent *event)
{
    emit blockHovered(blockAt(event->position()));
}
