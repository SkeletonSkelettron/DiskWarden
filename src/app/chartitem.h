#pragma once

#include <QColor>
#include <QList>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

// A line graph in the style of the classic disk monitoring tools: grey
// gradient background, dashed grid, vertical axis labels and optional
// value boxes above the points.
class ChartItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QList<QPointF> points READ points WRITE setPoints NOTIFY changed)
    Q_PROPERTY(QList<QPointF> points2 READ points2 WRITE setPoints2 NOTIFY changed)
    Q_PROPERTY(QColor lineColor MEMBER m_lineColor NOTIFY changed)
    Q_PROPERTY(QColor lineColor2 MEMBER m_lineColor2 NOTIFY changed)
    Q_PROPERTY(int xMode MEMBER m_xMode NOTIFY changed)
    Q_PROPERTY(bool showValues MEMBER m_showValues NOTIFY changed)
    Q_PROPERTY(bool showPoints MEMBER m_showPoints NOTIFY changed)
    Q_PROPERTY(double yMinimum MEMBER m_yMinimum NOTIFY changed)
    Q_PROPERTY(double yMaximum MEMBER m_yMaximum NOTIFY changed)
    Q_PROPERTY(double minimumSpan MEMBER m_minimumSpan NOTIFY changed)
    Q_PROPERTY(int valueDecimals MEMBER m_valueDecimals NOTIFY changed)
    Q_PROPERTY(QString yTitle MEMBER m_yTitle NOTIFY changed)
    Q_PROPERTY(QString xTitle MEMBER m_xTitle NOTIFY changed)
    Q_PROPERTY(QString emptyText MEMBER m_emptyText NOTIFY changed)
    Q_PROPERTY(QColor backgroundTop MEMBER m_backgroundTop NOTIFY changed)
    Q_PROPERTY(QColor backgroundBottom MEMBER m_backgroundBottom NOTIFY changed)
    Q_PROPERTY(QColor plotTop MEMBER m_plotTop NOTIFY changed)
    Q_PROPERTY(QColor plotBottom MEMBER m_plotBottom NOTIFY changed)
    Q_PROPERTY(QColor gridColor MEMBER m_gridColor NOTIFY changed)
    Q_PROPERTY(QColor textColor MEMBER m_textColor NOTIFY changed)
    Q_PROPERTY(QColor borderColor MEMBER m_borderColor NOTIFY changed)
    Q_PROPERTY(QColor boxColor MEMBER m_boxColor NOTIFY changed)
    Q_PROPERTY(QColor boxBorderColor MEMBER m_boxBorderColor NOTIFY changed)
    Q_PROPERTY(QColor boxTextColor MEMBER m_boxTextColor NOTIFY changed)

public:
    enum XMode { Time, Date, Number };
    Q_ENUM(XMode)

    explicit ChartItem(QQuickItem *parent = nullptr);

    QList<QPointF> points() const { return m_points; }
    void setPoints(const QList<QPointF> &points);
    QList<QPointF> points2() const { return m_points2; }
    void setPoints2(const QList<QPointF> &points);

    void paint(QPainter *painter) override;

signals:
    void changed();

private:
    QString xLabel(double x, double span) const;

    QList<QPointF> m_points;
    QList<QPointF> m_points2;
    QColor m_lineColor = QColor(0x00, 0x00, 0xc0);
    QColor m_lineColor2 = QColor(0xd0, 0x20, 0x20);
    int m_xMode = Time;
    bool m_showValues = false;
    bool m_showPoints = true;
    double m_yMinimum = 0;
    double m_yMaximum = 0;
    double m_minimumSpan = 10;
    int m_valueDecimals = 0;
    QString m_yTitle;
    QString m_xTitle;
    QString m_emptyText;
    QColor m_backgroundTop = QColor(0xfa, 0xfa, 0xfa);
    QColor m_backgroundBottom = QColor(0xd2, 0xd2, 0xd2);
    QColor m_plotTop = QColor(0xff, 0xff, 0xff);
    QColor m_plotBottom = QColor(0xe4, 0xe4, 0xe4);
    QColor m_gridColor = QColor(0xa0, 0xa0, 0xa0);
    QColor m_textColor = QColor(0x30, 0x30, 0x30);
    QColor m_borderColor = QColor(0x80, 0x80, 0x80);
    QColor m_boxColor = QColor(0xff, 0xff, 0xff);
    QColor m_boxBorderColor = QColor(0x60, 0x60, 0x60);
    QColor m_boxTextColor = QColor(0x10, 0x10, 0x10);
};
