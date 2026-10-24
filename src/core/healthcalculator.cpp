#include "healthcalculator.h"

#include <QCoreApplication>
#include <QLocale>
#include <algorithm>
#include <cmath>

namespace hg {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Health", text);
}

struct Weight {
    int id;
    double weight;
    double limit;
};

// Weights and limits of the attributes that reduce the health, after the
// "analyse data field" method Hard Disk Sentinel documents.
const Weight kWeights[] = {
    {5, 1.0, 70.0},   // Reallocated sectors count
    {7, 0.5, 20.0},   // Seek error rate
    {10, 3.0, 60.0},  // Spin retry count
    {196, 0.6, 30.0}, // Reallocation event count
    {197, 0.6, 48.0}, // Current pending sector count
    {198, 1.0, 70.0}, // Off-line uncorrectable sector count
};

// Attributes whose normalized value is the remaining life of an SSD.
const int kSsdWearAttributes[] = {169, 177, 231, 232, 233, 202, 173};

// Attributes whose normalized value reflects the performance of a disk.
const int kPerformanceAttributes[] = {2, 8};

QString noun(DiskKind kind)
{
    switch (kind) {
    case DiskKind::Hdd:
        return tr("hard disk");
    case DiskKind::Ssd:
        return tr("solid state disk");
    case DiskKind::Nvme:
        return tr("NVMe solid state disk");
    case DiskKind::Unknown:
        break;
    }
    return tr("disk");
}

QString statusWord(int health)
{
    if (health >= 100)
        return tr("PERFECT");
    if (health >= 95)
        return tr("EXCELLENT");
    if (health >= 80)
        return tr("GOOD");
    if (health >= 50)
        return tr("FAIR");
    if (health >= 25)
        return tr("POOR");
    return tr("CRITICAL");
}

bool isSeagate(const QString &model)
{
    return model.startsWith(QLatin1String("ST"), Qt::CaseSensitive)
        || model.startsWith(QLatin1String("Seagate"), Qt::CaseInsensitive);
}

QString temperatureText(double t)
{
    return QStringLiteral("%1 °C").arg(qRound(t));
}

} // namespace

int healthLevel(int health, int warning, int critical)
{
    if (health < 0)
        return 0;
    if (health < critical)
        return 2;
    if (health < warning)
        return 1;
    return 0;
}

double HealthCalculator::attributeDeduction(int id, qint64 count)
{
    if (count <= 0)
        return 0.0;
    for (const Weight &w : kWeights) {
        if (w.id == id)
            return std::min(w.limit, w.weight * double(count));
    }
    return 0.0;
}

qint64 HealthCalculator::healthCount(const HealthInput &input, const SmartAttribute &a)
{
    if (!a.rawKnown)
        return 0;
    qint64 raw = a.raw;
    if (a.id == 7) {
        // Seagate packs the errors (upper 16 bits) and the seeks (lower 32)
        // into a rate whose normalized value already judges it, and large
        // values of other vendors are rates as well: only small counts count.
        if (isSeagate(input.model) || raw > 0xFFFF)
            return 0;
    }
    raw = (raw & 0xFFFF) - input.offsets.value(a.id, 0);
    return std::max<qint64>(raw, 0);
}

int HealthCalculator::remainingLifetimeDays(DiskKind kind, int health, qint64 powerOnHours,
                                            int hotDays, bool wearBased)
{
    if (health < 0)
        return -1;
    if (health == 0)
        return 0;
    if (wearBased && health < 100 && powerOnHours > 0
        && (kind == DiskKind::Ssd || kind == DiskKind::Nvme)) {
        // The wear so far took the power-on time; the rest goes as fast.
        double days = powerOnHours / 24.0 * health / (100.0 - health);
        return int(std::min(days, 1001.0));
    }
    if (health >= 100)
        return 1001;
    // Five years of continuous use for a perfect disk, shortened by the
    // power-on time and hot days, scaled by the square of the health.
    double base = 1825.0 - (powerOnHours > 0 ? powerOnHours / 24.0 : 0.0);
    base = std::max(base, 1825.0 / 4.0);
    double factor = std::max(0.5, 1.0 - hotDays * 0.02);
    double h = health / 100.0;
    return int(std::min(base * h * h * factor, 1001.0));
}

HealthResult HealthCalculator::evaluate(const HealthInput &input)
{
    HealthResult r;
    const QLocale locale;

    if (!input.smartAvailable) {
        r.statusWord = tr("UNKNOWN");
        r.description << tr("S.M.A.R.T. information is not available for this disk, so its health can't be determined.")
                      << tr("USB enclosures, card readers and some RAID controllers don't pass the S.M.A.R.T. data of the disk through.");
        r.recommendation = tr("Connect the disk directly to the computer to examine its status.");
        return r;
    }

    const bool nvme = input.kind == DiskKind::Nvme;
    auto enabled = [&](int id) { return !input.disabledAttributes.contains(id); };
    auto attribute = [&](int id) -> const SmartAttribute * {
        const SmartAttribute *a = findAttribute(input.attributes, id);
        return a && enabled(id) ? a : nullptr;
    };
    auto count = [&](int id) -> qint64 {
        const SmartAttribute *a = attribute(id);
        return a ? healthCount(input, *a) : 0;
    };

    double health = 100.0;
    double cap = 100.0;
    bool wearBased = false;
    QStringList lines;
    int level = 0;

    if (!nvme) {
        for (const Weight &w : kWeights) {
            if (const SmartAttribute *a = attribute(w.id))
                health *= (100.0 - attributeDeduction(w.id, healthCount(input, *a))) / 100.0;
        }
        if (input.kind == DiskKind::Ssd) {
            int wear = -1;
            for (int id : kSsdWearAttributes) {
                const SmartAttribute *a = attribute(id);
                if (!a || a->value < 0 || a->value > 100)
                    continue;
                if (wear < 0 || a->value < wear)
                    wear = a->value;
                r.healthSources << QStringLiteral("#%1 %2").arg(id).arg(a->name);
            }
            if (wear >= 0) {
                health = health * wear / 100.0;
                wearBased = true;
            }
        }
    } else if (input.nvme.valid) {
        const NvmeHealth &n = input.nvme;
        double base = 100.0;
        if (n.percentUsed >= 0) {
            base = 100.0 - std::clamp(n.percentUsed, 0, 100);
            r.healthSources << tr("Percentage Used (%1 %)").arg(n.percentUsed);
        }
        if (n.availableSpare >= 0 && n.availableSpare < 100) {
            int threshold = std::clamp(n.spareThreshold, 0, 99);
            double spare = std::clamp((n.availableSpare - threshold) * 100.0 / (100.0 - threshold), 0.0, 100.0);
            if (spare < base) {
                base = spare;
                r.healthSources << tr("Available Spare (%1 %)").arg(n.availableSpare);
            }
        }
        wearBased = !r.healthSources.isEmpty();
        health = base;
        if (n.mediaErrors > 0)
            health *= (100.0 - std::min(70.0, double(n.mediaErrors))) / 100.0;
        if (n.criticalWarning & NvmeReadOnly)
            cap = std::min(cap, 5.0);
        if (n.criticalWarning & NvmeReliabilityDegraded)
            cap = std::min(cap, 10.0);
        if (n.criticalWarning & (NvmeSpareLow | NvmeVolatileBackupFailed))
            cap = std::min(cap, 30.0);
    }

    if (input.smartFailing)
        cap = std::min(cap, 10.0);
    QStringList failingNow;
    QStringList failedInPast;
    for (const SmartAttribute &a : input.attributes) {
        if (!enabled(a.id) || !a.preFailure())
            continue;
        if (a.failingNow())
            failingNow << QStringLiteral("#%1 %2").arg(a.id).arg(a.name);
        else if (a.failedInPast())
            failedInPast << QStringLiteral("#%1 %2").arg(a.id).arg(a.name);
    }
    if (!failingNow.isEmpty())
        cap = std::min(cap, 10.0);

    r.health = std::clamp(int(std::floor(std::min(health, cap) + 1e-6)), 0, 100);

    // Performance: 25 % performance attributes, 25 % recent transfer
    // errors, 50 % the interface speed.
    double attributePart = 25.0;
    for (int id : kPerformanceAttributes) {
        const SmartAttribute *a = attribute(id);
        if (!a || a->threshold <= 0 || a->value <= 0 || a->value > 100 || a->threshold >= 100)
            continue;
        double f = std::clamp(double(a->value - a->threshold) / (100.0 - a->threshold), 0.0, 1.0);
        attributePart = std::min(attributePart, 25.0 * f);
    }
    double transferPart = 25.0 * std::max(0.0, 1.0 - input.recentCrcErrors / 25.0);
    double linkPart = 50.0 * std::clamp(input.linkRatio, 0.0, 1.0);
    r.performance = std::clamp(qRound(attributePart + transferPart + linkPart), 0, 100);

    r.statusWord = statusWord(r.health);
    lines << tr("The status of the %1 is %2.").arg(noun(input.kind), r.statusWord);

    // Sector problems.
    const qint64 reallocated = count(5);
    const qint64 pending = count(197);
    const qint64 uncorrectable = count(198);
    const qint64 events = count(196);
    const qint64 spinRetries = count(10);
    const qint64 seekErrors = count(7);
    const SmartAttribute *crc = attribute(199);
    const qint64 crcErrors = crc && crc->rawKnown ? (crc->raw & 0xFFFFFFFF) : 0;

    bool sectorProblems = false;
    if (!nvme) {
        if (reallocated > 0) {
            lines << tr("%1 bad sectors were found and reallocated to the spare area of the disk.").arg(locale.toString(reallocated));
            sectorProblems = true;
        }
        if (pending > 0) {
            lines << tr("%1 weak sectors are waiting to be reallocated (pending sectors).").arg(locale.toString(pending));
            sectorProblems = true;
        }
        if (uncorrectable > 0) {
            lines << tr("%1 sectors couldn't be read during the offline scan of the disk.").arg(locale.toString(uncorrectable));
            sectorProblems = true;
        }
        if (events > 0 && reallocated == 0) {
            lines << tr("The disk performed %1 sector reallocation events.").arg(locale.toString(events));
            sectorProblems = true;
        }
        if (!sectorProblems) {
            if (input.kind == DiskKind::Hdd && spinRetries == 0 && crcErrors == 0)
                lines << tr("No problematic or weak sectors were found and there are no spin-up or data transfer errors.");
            else
                lines << tr("No problematic or weak sectors were found.");
        }
        if (spinRetries > 0)
            lines << tr("The disk needed %1 retries to spin up, which can point to a weak power supply or a mechanical problem.").arg(locale.toString(spinRetries));
        if (seekErrors > 0)
            lines << tr("The heads failed to find the requested position %1 times (seek errors).").arg(locale.toString(seekErrors));
        if (crcErrors > 0) {
            QString text = tr("There are %1 errors in the data transfer between the disk and the computer.").arg(locale.toString(crcErrors));
            if (input.recentCrcErrors > 0)
                text += QLatin1Char(' ') + tr("%1 of them occurred in the last 30 days.").arg(locale.toString(input.recentCrcErrors));
            text += QLatin1Char(' ') + tr("Check the data cable, its connectors and the power supply.");
            lines << text;
        }
    } else if (input.nvme.valid) {
        if (input.nvme.mediaErrors > 0) {
            lines << tr("%1 media and data integrity errors were recorded by the controller.").arg(locale.toString(input.nvme.mediaErrors));
            sectorProblems = true;
        } else {
            lines << tr("No media or data integrity errors were found.");
        }
        const int w = input.nvme.criticalWarning;
        if (w & NvmeSpareLow)
            lines << tr("The available spare capacity has fallen below the threshold.");
        if (w & NvmeReliabilityDegraded)
            lines << tr("The controller reports that the reliability of the device is degraded.");
        if (w & NvmeReadOnly)
            lines << tr("The device has been placed into read-only mode.");
        if (w & NvmeVolatileBackupFailed)
            lines << tr("The volatile memory backup device has failed.");
        if (w & NvmeTemperature)
            lines << tr("The controller reports a temperature outside the allowed range.");
    }

    if (input.linkRatio < 0.999 && !input.linkDescription.isEmpty())
        lines << tr("The disk works below the speed of its interface (%1), which lowers the performance.").arg(input.linkDescription);

    if (input.smartFailing)
        lines << tr("The disk reports that it is FAILING: its overall S.M.A.R.T. self-assessment didn't pass.");
    for (const QString &a : std::as_const(failingNow))
        lines << tr("The S.M.A.R.T. attribute %1 has reached its failure threshold.").arg(a);
    for (const QString &a : std::as_const(failedInPast))
        lines << tr("The S.M.A.R.T. attribute %1 reached its failure threshold in the past.").arg(a);

    if (input.kind == DiskKind::Ssd || nvme) {
        if (input.trimSupported)
            lines << tr("The TRIM (discard) feature of the SSD is supported.");
        if (!r.healthSources.isEmpty()) {
            if (nvme)
                lines << tr("The health is determined by the NVMe health log: %1").arg(r.healthSources.join(QLatin1String(", ")));
            else
                lines << tr("The health is determined by SSD specific S.M.A.R.T. attribute(s): %1").arg(r.healthSources.join(QLatin1String(", ")));
        }
    }

    int temperatureLevel = 0;
    if (!std::isnan(input.temperature)) {
        if (input.temperature >= input.temperatureCritical) {
            lines << tr("The temperature is too high (%1). Improve the cooling of the disk.").arg(temperatureText(input.temperature));
            temperatureLevel = 2;
        } else if (input.temperature >= input.temperatureWarning) {
            lines << tr("The temperature is higher than recommended (%1).").arg(temperatureText(input.temperature));
            temperatureLevel = 1;
        }
    }

    if (input.smartFailing || !failingNow.isEmpty() || r.health < 25) {
        r.recommendation = tr("Back up your data immediately and replace the disk.");
        level = 2;
    } else if (r.health < 50) {
        r.recommendation = tr("Back up your data and plan the replacement of the disk.");
        level = 2;
    } else if (sectorProblems) {
        r.recommendation = tr("Back up important data and run a surface test to examine the disk surface.");
        level = 1;
    } else if (r.health < 100) {
        r.recommendation = tr("Monitor the disk regularly.");
        level = 1;
    } else if (input.recentCrcErrors > 0) {
        r.recommendation = tr("Check the data cable connections. No other actions are needed.");
        level = 1;
    } else {
        r.recommendation = tr("No actions needed.");
    }
    r.level = std::max(level, temperatureLevel);
    r.description = lines;

    r.remainingDays = remainingLifetimeDays(input.kind, r.health, input.powerOnHours, input.hotDays, wearBased);
    return r;
}

} // namespace hg
