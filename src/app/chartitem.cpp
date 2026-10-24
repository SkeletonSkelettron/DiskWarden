#include "chartitem.h"

#include <QDateTime>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace {

double niceStep(double span, int ticks)
{
    const double raw = span / std::max(1, ticks);
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0}) {
        if (m * magnitude >= raw)
            return m * magnitude;
    }
    return 10.0 * magnitude;
}

} // namespace

ChartItem::ChartItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &ChartItem::changed, this, [this] { update(); });
}

void ChartItem::setPoints(const QList<QPointF> &points)
{
    m_points = points;
    emit changed();
}

void ChartItem::setPoints2(const QList<QPointF> &points)
{
    m_points2 = points;
    emit changed();
}

QString ChartItem::xLabel(double x, double span) const
{
    const QLocale locale;
    switch (m_xMode) {
    case Time: {
        const QDateTime time = QDateTime::fromMSecsSinceEpoch(qint64(x));
        if (span > 2 * 86400000.0)
            return locale.toString(time, QLocale::ShortFormat);
        return time.toString(span < 3600000.0 ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm"));
    }
    case Date:
        return locale.toString(QDateTime::fromMSecsSinceEpoch(qint64(x)).date(), QLocale::ShortFormat);
    default:
        return locale.toString(x, 'f', 0);
    }
}

void ChartItem::paint(QPainter *painter)
{
    const QRectF bounds = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, false);
    QLinearGradient background(0, 0, 0, bounds.height());
    background.setColorAt(0, m_backgroundTop);
    background.setColorAt(1, m_backgroundBottom);
    painter->fillRect(bounds, background);

    QFont font = painter->font();
    font.setPixelSize(10);
    painter->setFont(font);
    const QFontMetricsF fm(font);

    QList<QPointF> all = m_points;
    all += m_points2;
    if (all.isEmpty()) {
        painter->setPen(m_textColor);
        painter->drawText(bounds, Qt::AlignCenter, m_emptyText);
        return;
    }

    // Value range.
    double lo = m_yMinimum;
    double hi = m_yMaximum;
    if (hi <= lo) {
        lo = hi = all.first().y();
        for (const QPointF &p : std::as_const(all)) {
            lo = std::min(lo, p.y());
            hi = std::max(hi, p.y());
        }
        if (hi - lo < m_minimumSpan) {
            const double center = (hi + lo) / 2;
            lo = center - m_minimumSpan / 2;
            hi = center + m_minimumSpan / 2;
            if (lo < 0 && all.first().y() >= 0) {
                hi -= lo;
                lo = 0;
            }
        }
    }
    // The values are counts, degrees and percentages: whole-number ticks.
    const double yStep = std::max(1.0, niceStep(hi - lo, 5));
    lo = std::floor(lo / yStep) * yStep;
    hi = std::ceil(hi / yStep) * yStep;
    if (hi <= lo)
        hi = lo + yStep;

    double xMin = all.first().x();
    double xMax = xMin;
    for (const QPointF &p : std::as_const(all)) {
        xMin = std::min(xMin, p.x());
        xMax = std::max(xMax, p.x());
    }
    if (xMax <= xMin) {
        xMin -= m_xMode == Number ? 1 : 60000;
        xMax += m_xMode == Number ? 1 : 60000;
    }
    const double xSpan = xMax - xMin;

    // Labels decide the margins.
    QList<double> xTicks;
    if (m_xMode == Number) {
        const double step = niceStep(xSpan, std::max(2, int(bounds.width() / 45)));
        for (double x = std::ceil(xMin / step) * step; x <= xMax + step * 1e-6; x += step)
            xTicks << x;
    } else {
        // Every point is a candidate; they're thinned out by their distance
        // once the plot area is known.
        for (const QPointF &p : std::as_const(m_points.isEmpty() ? m_points2 : m_points))
            xTicks << p.x();
    }
    double labelWidth = 0;
    for (double x : std::as_const(xTicks))
        labelWidth = std::max(labelWidth, fm.horizontalAdvance(xLabel(x, xSpan)));
    double yLabelWidth = 0;
    for (double y = lo; y <= hi + yStep * 1e-6; y += yStep)
        yLabelWidth = std::max(yLabelWidth, fm.horizontalAdvance(QLocale().toString(y, 'f', yStep < 1 ? 1 : 0)));

    const double left = 8 + yLabelWidth + (m_yTitle.isEmpty() ? 0 : fm.height() + 6);
    const double right = 14;
    const double top = m_showValues ? 26 : 12;
    const double bottom = 10 + labelWidth + (m_xTitle.isEmpty() ? 0 : fm.height() + 6);
    const QRectF plot(left, top, std::max(10.0, bounds.width() - left - right), std::max(10.0, bounds.height() - top - bottom));

    QLinearGradient plotBackground(0, plot.top(), 0, plot.bottom());
    plotBackground.setColorAt(0, m_plotTop);
    plotBackground.setColorAt(1, m_plotBottom);
    painter->fillRect(plot, plotBackground);

    auto mapX = [&](double x) { return plot.left() + (x - xMin) / xSpan * plot.width(); };
    auto mapY = [&](double y) { return plot.bottom() - (y - lo) / (hi - lo) * plot.height(); };

    if (m_xMode != Number) {
        QList<double> spaced;
        double last = -1e9;
        for (double x : std::as_const(xTicks)) {
            const double px = mapX(x);
            if (px - last >= fm.height() + 6) {
                spaced << x;
                last = px;
            }
        }
        xTicks = spaced;
    }

    // Horizontal grid and value labels.
    QPen dotted(m_gridColor, 1, Qt::DotLine);
    painter->setPen(dotted);
    for (double y = lo; y <= hi + yStep * 1e-6; y += yStep) {
        const double py = std::round(mapY(y)) + 0.5;
        painter->setPen(dotted);
        painter->drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
        painter->setPen(m_textColor);
        painter->drawText(QRectF(left - 6 - yLabelWidth, py - fm.height() / 2, yLabelWidth, fm.height()),
                          Qt::AlignRight | Qt::AlignVCenter, QLocale().toString(y, 'f', yStep < 1 ? 1 : 0));
    }

    // Vertical grid and rotated time labels.
    QPen dashed(m_gridColor, 1, Qt::DashLine);
    for (double x : std::as_const(xTicks)) {
        const double px = std::round(mapX(x)) + 0.5;
        painter->setPen(dashed);
        painter->drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
        painter->save();
        painter->translate(px, plot.bottom() + 5);
        painter->rotate(-90);
        painter->setPen(m_textColor);
        painter->drawText(QRectF(-labelWidth, -fm.height() / 2, labelWidth, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                          xLabel(x, xSpan));
        painter->restore();
    }

    painter->setPen(m_borderColor);
    painter->drawRect(plot.adjusted(0.5, 0.5, -0.5, -0.5));

    if (!m_yTitle.isEmpty()) {
        painter->save();
        painter->translate(4 + fm.height() / 2, plot.center().y());
        painter->rotate(-90);
        painter->setPen(m_textColor);
        painter->drawText(QRectF(-plot.height() / 2, -fm.height() / 2, plot.height(), fm.height()), Qt::AlignCenter, m_yTitle);
        painter->restore();
    }
    if (!m_xTitle.isEmpty()) {
        painter->setPen(m_textColor);
        painter->drawText(QRectF(plot.left(), bounds.height() - fm.height() - 4, plot.width(), fm.height()), Qt::AlignCenter, m_xTitle);
    }

    // Series.
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setClipRect(plot.adjusted(-4, -4, 4, 4));
    auto drawSeries = [&](const QList<QPointF> &series, const QColor &color) {
        if (series.isEmpty())
            return;
        QPolygonF line;
        line.reserve(series.size());
        for (const QPointF &p : series)
            line << QPointF(mapX(p.x()), mapY(p.y()));
        painter->setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (line.size() == 1)
            painter->drawEllipse(line.first(), 2, 2);
        else
            painter->drawPolyline(line);
        if (m_showPoints && line.size() <= 400) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(color.darker(130));
            for (const QPointF &p : std::as_const(line))
                painter->drawRect(QRectF(p.x() - 1.5, p.y() - 1.5, 3, 3));
            painter->setBrush(Qt::NoBrush);
        }
    };
    drawSeries(m_points2, m_lineColor2);
    drawSeries(m_points, m_lineColor);
    painter->setClipping(false);

    if (m_showValues && !m_points.isEmpty()) {
        painter->setRenderHint(QPainter::Antialiasing, false);
        double boxWidth = 0;
        for (const QPointF &p : std::as_const(m_points))
            boxWidth = std::max(boxWidth, fm.horizontalAdvance(QLocale().toString(p.y(), 'f', m_valueDecimals)) + 8);
        const double boxHeight = fm.height() + 2;
        double lastRight = -1e9;
        QPen boxPen(m_boxBorderColor, 1, Qt::DotLine);
        for (int i = 0; i < m_points.size(); ++i) {
            const QPointF p(mapX(m_points[i].x()), mapY(m_points[i].y()));
            QRectF box(std::round(p.x() - boxWidth / 2) + 0.5, std::round(p.y() - boxHeight - 5) + 0.5, boxWidth, boxHeight);
            if (box.left() < lastRight + 2 && i != m_points.size() - 1)
                continue;
            if (box.top() < 1)
                box.moveTop(std::round(p.y() + 5) + 0.5);
            painter->setPen(boxPen);
            painter->setBrush(m_boxColor);
            painter->drawRect(box);
            painter->setPen(m_boxTextColor);
            painter->drawText(box, Qt::AlignCenter, QLocale().toString(m_points[i].y(), 'f', m_valueDecimals));
            lastRight = box.right();
        }
        painter->setBrush(Qt::NoBrush);
    }
}
