#include "formatting.h"

#include <QCoreApplication>
#include <QLocale>

namespace hg {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Formatting", text);
}

QString scaled(double value, int decimals, const char *const *units, int unitCount)
{
    int unit = 0;
    while (value >= 1024.0 && unit < unitCount - 1) {
        value /= 1024.0;
        ++unit;
    }
    if (unit == 0)
        decimals = 0;
    return QStringLiteral("%1 %2").arg(QLocale().toString(value, 'f', decimals), QLatin1String(units[unit]));
}

} // namespace

QString formatSize(qint64 bytes)
{
    static const char *const units[] = {"bytes", "KB", "MB", "GB", "TB", "PB"};
    return scaled(double(qMax<qint64>(bytes, 0)), 1, units, 6);
}

QString formatData(qint64 bytes)
{
    static const char *const units[] = {"bytes", "KB", "MB", "GB", "TB", "PB"};
    return scaled(double(qMax<qint64>(bytes, 0)), 2, units, 6);
}

QString formatRate(double bytesPerSecond)
{
    double mb = bytesPerSecond / (1024.0 * 1024.0);
    return QStringLiteral("%1 MB/s").arg(QLocale().toString(mb, 'f', mb < 10 ? 2 : 1));
}

QString formatPowerOnTime(qint64 hours)
{
    if (hours < 0)
        return tr("unknown");
    return tr("%1 days, %2 hours").arg(hours / 24).arg(hours % 24);
}

QString formatRemainingDays(int days)
{
    if (days < 0)
        return tr("unknown");
    if (days > 1000)
        return tr("more than 1000 days");
    if (days == 1)
        return tr("1 day");
    return tr("%1 days").arg(days);
}

QString formatElapsed(qint64 seconds)
{
    seconds = qMax<qint64>(seconds, 0);
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString formatRawHex(qint64 raw)
{
    return QStringLiteral("%1").arg(quint64(raw) & 0xFFFFFFFFFFFFull, 12, 16, QLatin1Char('0')).toUpper();
}

QString ratingWord(int percent)
{
    if (percent < 0)
        return tr("Unknown");
    if (percent >= 95)
        return tr("Excellent");
    if (percent >= 80)
        return tr("Good");
    if (percent >= 50)
        return tr("Fair");
    if (percent >= 25)
        return tr("Poor");
    return tr("Critical");
}

} // namespace hg
