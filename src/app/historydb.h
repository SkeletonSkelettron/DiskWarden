#pragma once

#include "smartattribute.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>

// SQLite store of everything HDGushagi remembers about the disks:
// temperature samples, health history, attribute changes, log and alerts.
class HistoryDb
{
public:
    enum EventKind { Log = 0, Alert = 1 };

    struct Event {
        QDateTime time;
        int level = 0; // 0 information, 1 warning, 2 error
        QString text;
    };

    struct DailyTemperature {
        QDate date;
        double average = 0;
        double minimum = 0;
        double maximum = 0;
    };

    struct Extreme {
        bool valid = false;
        double value = 0;
        QDateTime time;
    };

    struct TodayTemperature {
        bool valid = false;
        double average = 0;
        double minimum = 0;
        double maximum = 0;
    };

    static HistoryDb &instance();

    bool open(const QString &path);
    bool isOpen() const { return m_open; }

    // The id of a disk, created on first use.
    int diskId(const QString &key, const QString &model, const QString &serial);
    QDateTime firstSeen(int diskId);
    QString comment(int diskId);
    void setComment(int diskId, const QString &comment);

    void addTemperature(int diskId, const QDateTime &time, double temperature);
    QList<QPointF> temperatures(int diskId, const QDateTime &from, const QDateTime &to);
    QList<DailyTemperature> dailyTemperatures(int diskId, int days);
    TodayTemperature todayTemperature(int diskId);
    Extreme maximumTemperature(int diskId);
    Extreme minimumTemperature(int diskId);
    // Days whose average temperature was at or above the threshold.
    int hotDays(int diskId, double threshold);

    // Stores health and performance when they changed or once an hour.
    void addHealth(int diskId, const QDateTime &time, int health, int performance);
    // The last value of each day, x = msecs since epoch.
    QList<QPointF> healthSeries(int diskId, bool performance, int days = 365);
    int lastHealth(int diskId);

    // Stores the attributes whose value or raw data changed.
    void addAttributes(int diskId, const QDateTime &time, const QList<hg::SmartAttribute> &attributes);
    QHash<int, qint64> lastRawValues(int diskId);
    QList<QPointF> attributeSeries(int diskId, int attributeId, bool normalized);
    // The raw value stored last at or before the given time, or the
    // earliest one when there's none before it; -1 when unknown.
    qint64 rawValueAt(int diskId, int attributeId, const QDateTime &time);

    void addEvent(int diskId, EventKind kind, const QDateTime &time, int level, const QString &text);
    QList<Event> events(int diskId, EventKind kind, int limit = 500);
    void clearEvents(int diskId, EventKind kind);

    void prune();

private:
    HistoryDb() = default;
    bool exec(const QString &sql);

    bool m_open = false;
    QHash<int, QHash<int, QPair<int, qint64>>> m_lastAttributes;
    QHash<int, QHash<int, qint64>> m_lastStored;
    QHash<int, QPair<qint64, QPair<int, int>>> m_lastHealth;
};
