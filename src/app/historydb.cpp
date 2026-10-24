#include "historydb.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

const QString kConnection = QStringLiteral("history");

QSqlDatabase db()
{
    return QSqlDatabase::database(kConnection, false);
}

// Attributes whose every change is kept; the others are stored at most
// once an hour so that temperatures and counters don't flood the table.
bool alwaysStore(int id)
{
    switch (id) {
    case 5:
    case 10:
    case 187:
    case 188:
    case 196:
    case 197:
    case 198:
    case 199:
        return true;
    default:
        return false;
    }
}

qint64 startOfToday()
{
    return QDateTime(QDate::currentDate(), QTime(0, 0)).toSecsSinceEpoch();
}

} // namespace

HistoryDb &HistoryDb::instance()
{
    static HistoryDb history;
    return history;
}

bool HistoryDb::exec(const QString &sql)
{
    QSqlQuery query(db());
    if (!query.exec(sql)) {
        qWarning("History database: %s", qPrintable(query.lastError().text()));
        return false;
    }
    return true;
}

bool HistoryDb::open(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnection);
    database.setDatabaseName(path);
    if (!database.open()) {
        qWarning("Can't open the history database %s: %s", qPrintable(path), qPrintable(database.lastError().text()));
        return false;
    }
    m_open = exec(QStringLiteral("PRAGMA journal_mode=WAL"))
        && exec(QStringLiteral("CREATE TABLE IF NOT EXISTS disks (id INTEGER PRIMARY KEY, key TEXT UNIQUE, "
                               "model TEXT, serial TEXT, first_seen INTEGER, comment TEXT DEFAULT '')"))
        && exec(QStringLiteral("CREATE TABLE IF NOT EXISTS temperature (disk INTEGER, ts INTEGER, value REAL)"))
        && exec(QStringLiteral("CREATE INDEX IF NOT EXISTS temperature_disk_ts ON temperature (disk, ts)"))
        && exec(QStringLiteral("CREATE TABLE IF NOT EXISTS health (disk INTEGER, ts INTEGER, health INTEGER, performance INTEGER)"))
        && exec(QStringLiteral("CREATE INDEX IF NOT EXISTS health_disk_ts ON health (disk, ts)"))
        && exec(QStringLiteral("CREATE TABLE IF NOT EXISTS attributes (disk INTEGER, ts INTEGER, attr INTEGER, value INTEGER, raw INTEGER)"))
        && exec(QStringLiteral("CREATE INDEX IF NOT EXISTS attributes_disk_attr_ts ON attributes (disk, attr, ts)"))
        && exec(QStringLiteral("CREATE TABLE IF NOT EXISTS events (id INTEGER PRIMARY KEY, disk INTEGER, kind INTEGER, "
                               "ts INTEGER, level INTEGER, text TEXT)"))
        && exec(QStringLiteral("CREATE INDEX IF NOT EXISTS events_disk_kind_ts ON events (disk, kind, ts)"));
    if (m_open)
        prune();
    return m_open;
}

int HistoryDb::diskId(const QString &key, const QString &model, const QString &serial)
{
    if (!m_open)
        return -1;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT id FROM disks WHERE key = ?"));
    query.addBindValue(key);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    query.prepare(QStringLiteral("INSERT INTO disks (key, model, serial, first_seen) VALUES (?, ?, ?, ?)"));
    query.addBindValue(key);
    query.addBindValue(model);
    query.addBindValue(serial);
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!query.exec())
        return -1;
    return query.lastInsertId().toInt();
}

QDateTime HistoryDb::firstSeen(int diskId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT first_seen FROM disks WHERE id = ?"));
    query.addBindValue(diskId);
    if (query.exec() && query.next())
        return QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
    return {};
}

QString HistoryDb::comment(int diskId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT comment FROM disks WHERE id = ?"));
    query.addBindValue(diskId);
    if (query.exec() && query.next())
        return query.value(0).toString();
    return {};
}

void HistoryDb::setComment(int diskId, const QString &comment)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE disks SET comment = ? WHERE id = ?"));
    query.addBindValue(comment);
    query.addBindValue(diskId);
    query.exec();
}

void HistoryDb::addTemperature(int diskId, const QDateTime &time, double temperature)
{
    if (!m_open || diskId < 0)
        return;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("INSERT INTO temperature (disk, ts, value) VALUES (?, ?, ?)"));
    query.addBindValue(diskId);
    query.addBindValue(time.toSecsSinceEpoch());
    query.addBindValue(temperature);
    query.exec();
}

QList<QPointF> HistoryDb::temperatures(int diskId, const QDateTime &from, const QDateTime &to)
{
    QList<QPointF> points;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT ts, value FROM temperature WHERE disk = ? AND ts >= ? AND ts <= ? ORDER BY ts"));
    query.addBindValue(diskId);
    query.addBindValue(from.toSecsSinceEpoch());
    query.addBindValue(to.toSecsSinceEpoch());
    if (query.exec()) {
        while (query.next())
            points << QPointF(query.value(0).toDouble() * 1000.0, query.value(1).toDouble());
    }
    return points;
}

QList<HistoryDb::DailyTemperature> HistoryDb::dailyTemperatures(int diskId, int days)
{
    QList<DailyTemperature> result;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT date(ts, 'unixepoch', 'localtime') AS d, avg(value), min(value), max(value) "
                                 "FROM temperature WHERE disk = ? AND ts >= ? GROUP BY d ORDER BY d"));
    query.addBindValue(diskId);
    query.addBindValue(startOfToday() - qint64(days) * 86400);
    if (query.exec()) {
        while (query.next()) {
            DailyTemperature d;
            d.date = QDate::fromString(query.value(0).toString(), Qt::ISODate);
            d.average = query.value(1).toDouble();
            d.minimum = query.value(2).toDouble();
            d.maximum = query.value(3).toDouble();
            result << d;
        }
    }
    return result;
}

HistoryDb::TodayTemperature HistoryDb::todayTemperature(int diskId)
{
    TodayTemperature t;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT count(*), avg(value), min(value), max(value) FROM temperature WHERE disk = ? AND ts >= ?"));
    query.addBindValue(diskId);
    query.addBindValue(startOfToday());
    if (query.exec() && query.next() && query.value(0).toInt() > 0) {
        t.valid = true;
        t.average = query.value(1).toDouble();
        t.minimum = query.value(2).toDouble();
        t.maximum = query.value(3).toDouble();
    }
    return t;
}

HistoryDb::Extreme HistoryDb::maximumTemperature(int diskId)
{
    Extreme e;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT value, ts FROM temperature WHERE disk = ? ORDER BY value DESC, ts DESC LIMIT 1"));
    query.addBindValue(diskId);
    if (query.exec() && query.next()) {
        e.valid = true;
        e.value = query.value(0).toDouble();
        e.time = QDateTime::fromSecsSinceEpoch(query.value(1).toLongLong());
    }
    return e;
}

HistoryDb::Extreme HistoryDb::minimumTemperature(int diskId)
{
    Extreme e;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT value, ts FROM temperature WHERE disk = ? ORDER BY value ASC, ts DESC LIMIT 1"));
    query.addBindValue(diskId);
    if (query.exec() && query.next()) {
        e.valid = true;
        e.value = query.value(0).toDouble();
        e.time = QDateTime::fromSecsSinceEpoch(query.value(1).toLongLong());
    }
    return e;
}

int HistoryDb::hotDays(int diskId, double threshold)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT count(*) FROM (SELECT avg(value) AS a FROM temperature WHERE disk = ? "
                                 "GROUP BY date(ts, 'unixepoch', 'localtime')) WHERE a >= ?"));
    query.addBindValue(diskId);
    query.addBindValue(threshold);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    return 0;
}

void HistoryDb::addHealth(int diskId, const QDateTime &time, int health, int performance)
{
    if (!m_open || diskId < 0 || health < 0)
        return;
    const qint64 ts = time.toSecsSinceEpoch();
    if (!m_lastHealth.contains(diskId)) {
        QSqlQuery query(db());
        query.prepare(QStringLiteral("SELECT ts, health, performance FROM health WHERE disk = ? ORDER BY ts DESC LIMIT 1"));
        query.addBindValue(diskId);
        if (query.exec() && query.next())
            m_lastHealth.insert(diskId, {query.value(0).toLongLong(), {query.value(1).toInt(), query.value(2).toInt()}});
    }
    auto last = m_lastHealth.constFind(diskId);
    if (last != m_lastHealth.cend() && last->second == qMakePair(health, performance) && ts - last->first < 3600)
        return;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("INSERT INTO health (disk, ts, health, performance) VALUES (?, ?, ?, ?)"));
    query.addBindValue(diskId);
    query.addBindValue(ts);
    query.addBindValue(health);
    query.addBindValue(performance);
    if (query.exec())
        m_lastHealth.insert(diskId, {ts, {health, performance}});
}

QList<QPointF> HistoryDb::healthSeries(int diskId, bool performance, int days)
{
    QList<QPointF> points;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT max(ts), %1 FROM health WHERE disk = ? AND ts >= ? "
                                 "GROUP BY date(ts, 'unixepoch', 'localtime') ORDER BY 1")
                      .arg(performance ? QStringLiteral("performance") : QStringLiteral("health")));
    query.addBindValue(diskId);
    query.addBindValue(startOfToday() - qint64(days) * 86400);
    if (query.exec()) {
        while (query.next())
            points << QPointF(query.value(0).toDouble() * 1000.0, query.value(1).toDouble());
    }
    return points;
}

int HistoryDb::lastHealth(int diskId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT health FROM health WHERE disk = ? ORDER BY ts DESC LIMIT 1"));
    query.addBindValue(diskId);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    return -1;
}

QHash<int, qint64> HistoryDb::lastRawValues(int diskId)
{
    QHash<int, qint64> values;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT attr, raw, max(ts) FROM attributes WHERE disk = ? GROUP BY attr"));
    query.addBindValue(diskId);
    if (query.exec()) {
        while (query.next())
            values.insert(query.value(0).toInt(), query.value(1).toLongLong());
    }
    return values;
}

void HistoryDb::addAttributes(int diskId, const QDateTime &time, const QList<dw::SmartAttribute> &attributes)
{
    if (!m_open || diskId < 0)
        return;
    const qint64 ts = time.toSecsSinceEpoch();
    QHash<int, QPair<int, qint64>> &cache = m_lastAttributes[diskId];
    if (cache.isEmpty()) {
        QSqlQuery query(db());
        query.prepare(QStringLiteral("SELECT attr, value, raw, max(ts) FROM attributes WHERE disk = ? GROUP BY attr"));
        query.addBindValue(diskId);
        if (query.exec()) {
            while (query.next())
                cache.insert(query.value(0).toInt(), {query.value(1).toInt(), query.value(2).toLongLong()});
        }
    }
    QHash<int, qint64> &stored = m_lastStored[diskId];

    QSqlDatabase database = db();
    database.transaction();
    QSqlQuery query(database);
    query.prepare(QStringLiteral("INSERT INTO attributes (disk, ts, attr, value, raw) VALUES (?, ?, ?, ?, ?)"));
    for (const dw::SmartAttribute &a : attributes) {
        auto previous = cache.constFind(a.id);
        const bool changed = previous == cache.cend() || previous->first != a.value || previous->second != a.raw;
        if (!changed)
            continue;
        const bool valueChanged = previous == cache.cend() || previous->first != a.value;
        if (!valueChanged && !alwaysStore(a.id) && ts - stored.value(a.id, 0) < 3600)
            continue;
        query.addBindValue(diskId);
        query.addBindValue(ts);
        query.addBindValue(a.id);
        query.addBindValue(a.value);
        query.addBindValue(a.raw);
        if (query.exec()) {
            cache.insert(a.id, {a.value, a.raw});
            stored.insert(a.id, ts);
        }
    }
    database.commit();
}

QList<QPointF> HistoryDb::attributeSeries(int diskId, int attributeId, bool normalized)
{
    QList<QPointF> points;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT ts, %1 FROM attributes WHERE disk = ? AND attr = ? ORDER BY ts")
                      .arg(normalized ? QStringLiteral("value") : QStringLiteral("raw")));
    query.addBindValue(diskId);
    query.addBindValue(attributeId);
    if (query.exec()) {
        while (query.next())
            points << QPointF(query.value(0).toDouble() * 1000.0, query.value(1).toDouble());
    }
    return points;
}

qint64 HistoryDb::rawValueAt(int diskId, int attributeId, const QDateTime &time)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT raw FROM attributes WHERE disk = ? AND attr = ? AND ts <= ? ORDER BY ts DESC LIMIT 1"));
    query.addBindValue(diskId);
    query.addBindValue(attributeId);
    query.addBindValue(time.toSecsSinceEpoch());
    if (query.exec() && query.next())
        return query.value(0).toLongLong();
    query.prepare(QStringLiteral("SELECT raw FROM attributes WHERE disk = ? AND attr = ? ORDER BY ts ASC LIMIT 1"));
    query.addBindValue(diskId);
    query.addBindValue(attributeId);
    if (query.exec() && query.next())
        return query.value(0).toLongLong();
    return -1;
}

void HistoryDb::addEvent(int diskId, EventKind kind, const QDateTime &time, int level, const QString &text)
{
    if (!m_open)
        return;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("INSERT INTO events (disk, kind, ts, level, text) VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(diskId);
    query.addBindValue(int(kind));
    query.addBindValue(time.toSecsSinceEpoch());
    query.addBindValue(level);
    query.addBindValue(text);
    query.exec();
}

QList<HistoryDb::Event> HistoryDb::events(int diskId, EventKind kind, int limit)
{
    QList<Event> result;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT ts, level, text FROM events WHERE disk = ? AND kind = ? ORDER BY ts DESC, id DESC LIMIT ?"));
    query.addBindValue(diskId);
    query.addBindValue(int(kind));
    query.addBindValue(limit);
    if (query.exec()) {
        while (query.next()) {
            Event e;
            e.time = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
            e.level = query.value(1).toInt();
            e.text = query.value(2).toString();
            result << e;
        }
    }
    return result;
}

void HistoryDb::clearEvents(int diskId, EventKind kind)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM events WHERE disk = ? AND kind = ?"));
    query.addBindValue(diskId);
    query.addBindValue(int(kind));
    query.exec();
}

void HistoryDb::prune()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM temperature WHERE ts < ?"));
    query.addBindValue(now - 400LL * 86400);
    query.exec();
    query.prepare(QStringLiteral("DELETE FROM events WHERE ts < ?"));
    query.addBindValue(now - 3LL * 365 * 86400);
    query.exec();
}
