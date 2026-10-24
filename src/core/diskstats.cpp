#include "diskstats.h"

#include <QByteArrayList>
#include <algorithm>

namespace dw {

QHash<QString, DiskStat> parseDiskStats(const QByteArray &content)
{
    QHash<QString, DiskStat> stats;
    const QByteArrayList lines = content.split('\n');
    for (const QByteArray &line : lines) {
        const QByteArrayList f = line.simplified().split(' ');
        // major minor name reads merged sectors ms writes merged sectors ms
        // in-flight io-ticks ...
        if (f.size() < 13)
            continue;
        DiskStat s;
        s.readsCompleted = f[3].toULongLong();
        s.sectorsRead = f[5].toULongLong();
        s.writesCompleted = f[7].toULongLong();
        s.sectorsWritten = f[9].toULongLong();
        s.ioTicksMs = f[12].toULongLong();
        stats.insert(QString::fromLatin1(f[2]), s);
    }
    return stats;
}

DiskRates diskRates(const DiskStat &previous, const DiskStat &current, double intervalMs)
{
    DiskRates r;
    if (intervalMs <= 0)
        return r;
    auto delta = [](quint64 a, quint64 b) { return b >= a ? double(b - a) : 0.0; };
    const double seconds = intervalMs / 1000.0;
    // /proc/diskstats counts 512-byte sectors regardless of the sector size.
    r.readBytesPerSecond = delta(previous.sectorsRead, current.sectorsRead) * 512.0 / seconds;
    r.writeBytesPerSecond = delta(previous.sectorsWritten, current.sectorsWritten) * 512.0 / seconds;
    r.utilization = std::clamp(delta(previous.ioTicksMs, current.ioTicksMs) * 100.0 / intervalMs, 0.0, 100.0);
    return r;
}

} // namespace dw
