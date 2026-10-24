#include "smartattribute.h"

#include <QCoreApplication>
#include <QHash>
#include <QStringList>
#include <cmath>

namespace dw {

bool SmartAttribute::failingNow() const
{
    return threshold > 0 && value >= 0 && value <= threshold;
}

bool SmartAttribute::failedInPast() const
{
    return threshold > 0 && worst >= 0 && worst <= threshold;
}

QString ataAttributeName(int id)
{
    static const QHash<int, const char *> names = {
        {1, "Raw Read Error Rate"},
        {2, "Throughput Performance"},
        {3, "Spin Up Time"},
        {4, "Start/Stop Count"},
        {5, "Reallocated Sectors Count"},
        {6, "Read Channel Margin"},
        {7, "Seek Error Rate"},
        {8, "Seek Time Performance"},
        {9, "Power-On Time Count"},
        {10, "Spin Retry Count"},
        {11, "Drive Calibration Retry Count"},
        {12, "Drive Power Cycle Count"},
        {13, "Soft Read Error Rate"},
        {22, "Current Helium Level"},
        {23, "Helium Condition Lower"},
        {24, "Helium Condition Upper"},
        {160, "Uncorrectable Sector Count"},
        {161, "Valid Spare Blocks"},
        {163, "Initial Invalid Blocks"},
        {164, "Total Erase Count"},
        {165, "Maximum Erase Count"},
        {166, "Minimum Erase Count"},
        {167, "Average Erase Count"},
        {168, "SATA PHY Error Count"},
        {169, "Remaining Lifetime Percentage"},
        {170, "Available Reserved Space"},
        {171, "Program Fail Count"},
        {172, "Erase Fail Count"},
        {173, "Wear Leveling Count"},
        {174, "Unexpected Power Loss Count"},
        {175, "Power Loss Protection Failure"},
        {176, "Erase Fail Count (Chip)"},
        {177, "Wear Leveling Count"},
        {178, "Used Reserved Block Count (Chip)"},
        {179, "Used Reserved Block Count (Total)"},
        {180, "Unused Reserved Block Count (Total)"},
        {181, "Program Fail Count (Total)"},
        {182, "Erase Fail Count (Total)"},
        {183, "Runtime Bad Block Count"},
        {184, "End-to-End Error Count"},
        {185, "Head Stability"},
        {187, "Reported Uncorrectable Errors"},
        {188, "Command Timeout"},
        {189, "High Fly Writes"},
        {190, "Airflow Temperature"},
        {191, "G-Sense Error Rate"},
        {192, "Power-Off Retract Count"},
        {193, "Load/Unload Cycle Count"},
        {194, "Disk Temperature"},
        {195, "Hardware ECC Recovered"},
        {196, "Reallocation Event Count"},
        {197, "Current Pending Sector Count"},
        {198, "Off-Line Uncorrectable Sector Count"},
        {199, "Ultra ATA CRC Error Count"},
        {200, "Write Error Rate"},
        {201, "Soft Read Error Rate"},
        {202, "Data Address Mark Errors"},
        {203, "Run Out Cancel"},
        {204, "Soft ECC Correction"},
        {205, "Thermal Asperity Rate"},
        {206, "Flying Height"},
        {207, "Spin High Current"},
        {208, "Spin Buzz"},
        {209, "Offline Seek Performance"},
        {210, "Vibration During Write"},
        {211, "Vibration During Write"},
        {212, "Shock During Write"},
        {220, "Disk Shift"},
        {221, "G-Sense Error Rate"},
        {222, "Loaded Hours"},
        {223, "Load/Unload Retry Count"},
        {224, "Load Friction"},
        {225, "Load/Unload Cycle Count"},
        {226, "Load-in Time"},
        {227, "Torque Amplification Count"},
        {228, "Power-Off Retract Cycle"},
        {230, "Drive Life Protection Status"},
        {231, "SSD Life Left"},
        {232, "Endurance Remaining"},
        {233, "Media Wearout Indicator"},
        {234, "Average Erase Count"},
        {235, "Good Block Count"},
        {240, "Head Flying Hours"},
        {241, "Total LBAs Written"},
        {242, "Total LBAs Read"},
        {243, "Total LBAs Written Expanded"},
        {244, "Total LBAs Read Expanded"},
        {249, "NAND Writes (GiB)"},
        {250, "Read Error Retry Rate"},
        {251, "Minimum Spares Remaining"},
        {252, "Newly Added Bad Flash Block"},
        {254, "Free Fall Protection"},
    };
    if (const char *name = names.value(id))
        return QCoreApplication::translate("SmartAttribute", name);
    return QCoreApplication::translate("SmartAttribute", "Vendor Specific");
}

SmartAttribute decodeUdisksAttribute(int id, const QString &key, int flags, int value,
                                     int worst, int threshold, qint64 pretty, int prettyUnit)
{
    SmartAttribute a;
    a.id = id;
    a.key = key;
    a.name = ataAttributeName(id);
    a.flags = flags;
    a.value = value;
    a.worst = worst;
    a.threshold = threshold;
    a.rawKnown = true;

    switch (PrettyUnit(prettyUnit)) {
    case PrettyUnit::Milliseconds:
        // Power-on style counters are converted to hours, other times
        // (spin up time) stay in milliseconds as the drive reports them.
        if (key.startsWith(QLatin1String("power-on")) || key == QLatin1String("loaded-hours")
            || key == QLatin1String("head-flying-hours"))
            a.raw = pretty / 3600000;
        else
            a.raw = pretty;
        break;
    case PrettyUnit::MilliKelvin:
        a.raw = qRound64((pretty - 273150) / 1000.0);
        break;
    case PrettyUnit::Megabytes:
        // libatasmart multiplies the LBA count by 65536 * 512 / 10^6.
        a.raw = qRound64(double(pretty) * 1000000.0 / (65536.0 * 512.0));
        break;
    case PrettyUnit::None:
    case PrettyUnit::Sectors:
    case PrettyUnit::SmallPercent:
    case PrettyUnit::Percent:
        a.raw = pretty;
        break;
    case PrettyUnit::Unknown:
    default:
        a.raw = 0;
        a.rawKnown = false;
        break;
    }
    return a;
}

QString attributeFlagsText(int flags)
{
    QStringList parts;
    parts << (flags & FlagPreFailure ? QCoreApplication::translate("SmartAttribute", "Pre-failure")
                                      : QCoreApplication::translate("SmartAttribute", "Advisory"));
    if (flags & FlagOnline)
        parts << QCoreApplication::translate("SmartAttribute", "Online");
    if (flags & FlagPerformance)
        parts << QCoreApplication::translate("SmartAttribute", "Performance");
    if (flags & FlagErrorRate)
        parts << QCoreApplication::translate("SmartAttribute", "Error Rate");
    if (flags & FlagEventCount)
        parts << QCoreApplication::translate("SmartAttribute", "Event Count");
    if (flags & FlagSelfPreserving)
        parts << QCoreApplication::translate("SmartAttribute", "Self Preserving");
    return parts.join(QLatin1String(", "));
}

const SmartAttribute *findAttribute(const QList<SmartAttribute> &attributes, int id)
{
    for (const SmartAttribute &a : attributes) {
        if (a.id == id)
            return &a;
    }
    return nullptr;
}

} // namespace dw
