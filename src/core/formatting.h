#pragma once

#include <QString>
#include <QtGlobal>

namespace hg {

// "238.5 GB": binary units with one decimal, as disk sizes are shown.
QString formatSize(qint64 bytes);
// "306.94 GB", "9.95 TB": two decimals, for amounts of transferred data.
QString formatData(qint64 bytes);
// "12.3 MB/s"
QString formatRate(double bytesPerSecond);
// "27 days, 15 hours"
QString formatPowerOnTime(qint64 hours);
// "more than 1000 days", "35 days", "unknown"
QString formatRemainingDays(int days);
// "00:07:46"
QString formatElapsed(qint64 seconds);
// 12-digit hexadecimal raw data, "000000000021"
QString formatRawHex(qint64 raw);

// Words for a health or performance percentage.
QString ratingWord(int percent);

} // namespace hg
