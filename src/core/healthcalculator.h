#pragma once

#include "smartattribute.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <limits>

namespace hg {

// The NVMe SMART / health information log page.
struct NvmeHealth {
    bool valid = false;
    int criticalWarning = 0;
    int availableSpare = -1;
    int spareThreshold = -1;
    int percentUsed = -1;
    qint64 bytesRead = -1;
    qint64 bytesWritten = -1;
    qint64 controllerBusyMinutes = -1;
    qint64 powerCycles = -1;
    qint64 powerOnHours = -1;
    qint64 unsafeShutdowns = -1;
    qint64 mediaErrors = -1;
    qint64 errorLogEntries = -1;
    double temperature = std::numeric_limits<double>::quiet_NaN();
    QList<double> sensors;
    double warningTemperature = std::numeric_limits<double>::quiet_NaN();
    double criticalTemperature = std::numeric_limits<double>::quiet_NaN();
    qint64 warningTemperatureMinutes = -1;
    qint64 criticalTemperatureMinutes = -1;
};

// NVMe critical warning bits.
enum NvmeWarning {
    NvmeSpareLow = 0x01,
    NvmeTemperature = 0x02,
    NvmeReliabilityDegraded = 0x04,
    NvmeReadOnly = 0x08,
    NvmeVolatileBackupFailed = 0x10,
};

struct HealthInput {
    DiskKind kind = DiskKind::Unknown;
    QString model;
    bool smartAvailable = false;
    bool smartFailing = false;
    QList<SmartAttribute> attributes;
    NvmeHealth nvme;
    double temperature = std::numeric_limits<double>::quiet_NaN();
    int temperatureWarning = 50;
    int temperatureCritical = 60;
    qint64 powerOnHours = -1;
    // Current interface speed divided by the fastest the disk supports.
    double linkRatio = 1.0;
    QString linkDescription;
    // Increase of the UDMA CRC error count during the last 30 days.
    qint64 recentCrcErrors = 0;
    // Days on which the average temperature was above the warning level.
    int hotDays = 0;
    bool trimSupported = false;
    QSet<int> disabledAttributes;
    // Subtracted from the raw data before it counts against the health.
    QHash<int, qint64> offsets;
};

struct HealthResult {
    int health = -1;      // 0..100, -1 when unknown
    int performance = -1; // 0..100, -1 when unknown
    int level = 0;        // 0 good, 1 warning, 2 critical
    int remainingDays = -1;
    QString statusWord;   // PERFECT, GOOD, ...
    QStringList description;
    QString recommendation;
    QStringList healthSources; // attributes the SSD health was taken from
};

// Health levels used for colors and alerts.
int healthLevel(int health, int warning = 50, int critical = 25);

class HealthCalculator
{
public:
    static HealthResult evaluate(const HealthInput &input);

    // The multiplicative health deduction of the "analyse data field"
    // method for the given attribute, in percent (0 = no deduction).
    static double attributeDeduction(int id, qint64 count);

    // Raw data of an attribute as it counts against health: offset applied,
    // lower 16 bits, seek error rates that are rates rather than counts ignored.
    static qint64 healthCount(const HealthInput &input, const SmartAttribute &a);

    static int remainingLifetimeDays(DiskKind kind, int health, qint64 powerOnHours, int hotDays,
                                     bool wearBased);
};

} // namespace hg
