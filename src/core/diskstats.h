#pragma once

#include <QHash>
#include <QString>
#include <QtGlobal>

namespace hg {

// One line of /proc/diskstats.
struct DiskStat {
    quint64 readsCompleted = 0;
    quint64 sectorsRead = 0;
    quint64 writesCompleted = 0;
    quint64 sectorsWritten = 0;
    quint64 ioTicksMs = 0; // time spent doing I/O
};

// Parses /proc/diskstats content into statistics keyed by device name.
QHash<QString, DiskStat> parseDiskStats(const QByteArray &content);

// Transfer rates between two samples taken intervalMs apart.
struct DiskRates {
    double readBytesPerSecond = 0;
    double writeBytesPerSecond = 0;
    double utilization = 0; // 0..100
};

DiskRates diskRates(const DiskStat &previous, const DiskStat &current, double intervalMs);

} // namespace hg
