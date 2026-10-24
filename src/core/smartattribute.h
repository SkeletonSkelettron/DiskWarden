#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

namespace hg {

enum class DiskKind { Unknown, Hdd, Ssd, Nvme };

// The pretty_unit values udisks2 (libblockdev/libatasmart) reports with
// each ATA attribute.
enum class PrettyUnit {
    Unknown = 0,
    None = 1,
    Milliseconds = 2,
    Sectors = 3,
    MilliKelvin = 4,
    SmallPercent = 5,
    Percent = 6,
    Megabytes = 7,
};

// ATA attribute flag bits.
enum AttributeFlag {
    FlagPreFailure = 0x01,
    FlagOnline = 0x02,
    FlagPerformance = 0x04,
    FlagErrorRate = 0x08,
    FlagEventCount = 0x10,
    FlagSelfPreserving = 0x20,
};

struct SmartAttribute {
    int id = 0;
    QString key;   // udisks name, e.g. "reallocated-sector-count"
    QString name;  // display name, e.g. "Reallocated Sectors Count"
    int flags = 0;
    int value = -1;
    int worst = -1;
    int threshold = -1;
    qint64 raw = 0;        // decoded raw data (count, hours, °C, LBAs)
    bool rawKnown = false; // false when udisks couldn't decode the raw data

    bool preFailure() const { return flags & FlagPreFailure; }
    // The normalized value has reached the threshold now.
    bool failingNow() const;
    // The normalized value reached the threshold at some point.
    bool failedInPast() const;
};

QString ataAttributeName(int id);

// Converts one entry of Drive.Ata.SmartGetAttributes() to an attribute,
// undoing libatasmart's unit conversion where that's possible.
SmartAttribute decodeUdisksAttribute(int id, const QString &key, int flags, int value,
                                     int worst, int threshold, qint64 pretty, int prettyUnit);

QString attributeFlagsText(int flags);

// Looks up an attribute by id; returns nullptr when it isn't there.
const SmartAttribute *findAttribute(const QList<SmartAttribute> &attributes, int id);

} // namespace hg
