#include "sysfs.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace dw {

namespace {

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString deviceDir(const QString &blockName)
{
    return QFileInfo(QStringLiteral("/sys/block/%1/device").arg(blockName)).canonicalFilePath();
}

// The PCI function a sysfs device path hangs off: the last
// "0000:00:17.0" style component.
QString pciDeviceDir(const QString &path)
{
    static const QRegularExpression pciAddress(QStringLiteral("^[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\\.[0-9a-f]$"));
    const QStringList parts = path.split(QLatin1Char('/'));
    int last = -1;
    for (int i = 0; i < parts.size(); ++i) {
        if (pciAddress.match(parts[i]).hasMatch())
            last = i;
    }
    if (last < 0)
        return {};
    return parts.mid(0, last + 1).join(QLatin1Char('/'));
}

void readController(const QString &pciDir, LinkInfo &info)
{
    if (pciDir.isEmpty())
        return;
    info.controllerDriver = QFileInfo(QFileInfo(pciDir + QStringLiteral("/driver")).canonicalFilePath()).fileName();
    bool ok1 = false;
    bool ok2 = false;
    quint16 vendor = readText(pciDir + QStringLiteral("/vendor")).toUShort(&ok1, 16);
    quint16 device = readText(pciDir + QStringLiteral("/device")).toUShort(&ok2, 16);
    if (ok1 && ok2)
        info.controller = pciDeviceName(vendor, device);
}

QString pcieText(double gts, int width)
{
    QString text = QStringLiteral("PCI-Express");
    QString generation = pcieGeneration(gts);
    if (!generation.isEmpty())
        text += QLatin1Char(' ') + generation;
    if (width > 0)
        text += QStringLiteral(" x%1").arg(width);
    return text;
}

} // namespace

double parseLinkSpeed(const QString &text)
{
    static const QRegularExpression number(QStringLiteral("([0-9]+(\\.[0-9]+)?)"));
    QRegularExpressionMatch m = number.match(text);
    return m.hasMatch() ? m.captured(1).toDouble() : 0.0;
}

QString pcieGeneration(double gts)
{
    struct Gen {
        double rate;
        const char *name;
    };
    static const Gen gens[] = {{2.5, "1.0"}, {5.0, "2.0"}, {8.0, "3.0"}, {16.0, "4.0"}, {32.0, "5.0"}, {64.0, "6.0"}};
    for (const Gen &g : gens) {
        if (std::abs(g.rate - gts) < 0.01)
            return QLatin1String(g.name);
    }
    return {};
}

QString pciDeviceName(quint16 vendor, quint16 device)
{
    for (const char *path : {"/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids", "/usr/share/pci.ids"}) {
        QFile file{QString::fromLatin1(path)};
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray vendorId = QByteArray::number(vendor, 16).rightJustified(4, '0');
        const QByteArray deviceId = '\t' + QByteArray::number(device, 16).rightJustified(4, '0');
        bool inVendor = false;
        QByteArray vendorName;
        while (!file.atEnd()) {
            QByteArray line = file.readLine();
            if (line.startsWith('#') || line.trimmed().isEmpty())
                continue;
            if (!line.startsWith('\t')) {
                if (inVendor)
                    break;
                if (line.startsWith(vendorId + ' ')) {
                    inVendor = true;
                    vendorName = line.mid(4).trimmed();
                }
                continue;
            }
            if (inVendor && line.startsWith(deviceId + ' '))
                return QString::fromUtf8(vendorName + ' ' + line.mid(6).trimmed());
        }
        if (inVendor)
            return QString::fromUtf8(vendorName);
    }
    return QStringLiteral("%1:%2").arg(vendor, 4, 16, QLatin1Char('0')).arg(device, 4, 16, QLatin1Char('0'));
}

LinkInfo readLinkInfo(const QString &blockName)
{
    LinkInfo info;
    const QString dir = deviceDir(blockName);
    if (dir.isEmpty())
        return info;

    if (blockName.startsWith(QLatin1String("nvme"))) {
        info.interfaceName = QStringLiteral("NVMe");
        // The namespace's device is the controller; its device is the PCI
        // function with the link attributes.
        QString pci = QFileInfo(dir + QStringLiteral("/device")).canonicalFilePath();
        if (pci.isEmpty())
            pci = pciDeviceDir(dir);
        double cur = parseLinkSpeed(readText(pci + QStringLiteral("/current_link_speed")));
        double max = parseLinkSpeed(readText(pci + QStringLiteral("/max_link_speed")));
        int curWidth = readText(pci + QStringLiteral("/current_link_width")).toInt();
        int maxWidth = readText(pci + QStringLiteral("/max_link_width")).toInt();
        if (cur > 0) {
            info.current = pcieText(cur, curWidth);
            info.maximum = pcieText(max, maxWidth);
            info.maximumKnown = max > 0 && maxWidth > 0;
            if (info.maximumKnown && curWidth > 0)
                info.ratio = std::min(1.0, (cur * curWidth) / (max * maxWidth));
        }
        readController(pci, info);
        return info;
    }

    if (dir.contains(QLatin1String("/usb"))) {
        info.interfaceName = QStringLiteral("USB");
        // Walk up to the USB device that reports its speed.
        QDir d(dir);
        while (d.cdUp() && d.absolutePath() != QLatin1String("/sys/devices")) {
            QString speed = readText(d.absoluteFilePath(QStringLiteral("speed")));
            QString version = readText(d.absoluteFilePath(QStringLiteral("version")));
            if (!speed.isEmpty() && !version.isEmpty()) {
                info.current = QStringLiteral("USB %1 (%2 Mbps)").arg(version, speed);
                break;
            }
        }
        readController(pciDeviceDir(dir), info);
        return info;
    }

    static const QRegularExpression ataPort(QStringLiteral("/ata([0-9]+)/"));
    QRegularExpressionMatch m = ataPort.match(dir);
    if (m.hasMatch()) {
        info.interfaceName = QStringLiteral("S-ATA");
        const QString link = QStringLiteral("/sys/class/ata_link/link%1/").arg(m.captured(1));
        double cur = parseLinkSpeed(readText(link + QStringLiteral("sata_spd")));
        double max = parseLinkSpeed(readText(link + QStringLiteral("hw_sata_spd_limit")));
        if (cur > 0) {
            info.current = QStringLiteral("%1 Gbps").arg(cur, 0, 'f', 1);
            if (max > 0) {
                info.maximum = QStringLiteral("%1 Gbps").arg(max, 0, 'f', 1);
                info.maximumKnown = true;
                info.ratio = std::min(1.0, cur / max);
            }
        }
    } else {
        info.interfaceName = QStringLiteral("SCSI");
    }
    readController(pciDeviceDir(dir), info);
    return info;
}

QueueInfo readQueueInfo(const QString &blockName)
{
    QueueInfo q;
    const QString queue = QStringLiteral("/sys/block/%1/queue/").arg(blockName);
    q.rotational = readText(queue + QStringLiteral("rotational")) == QLatin1String("1");
    q.logicalBlockSize = readText(queue + QStringLiteral("logical_block_size")).toInt();
    q.physicalBlockSize = readText(queue + QStringLiteral("physical_block_size")).toInt();
    q.discardMaxBytes = readText(queue + QStringLiteral("discard_max_bytes")).toLongLong();
    q.writeCache = readText(queue + QStringLiteral("write_cache"));
    static const QRegularExpression active(QStringLiteral("\\[([^\\]]+)\\]"));
    QRegularExpressionMatch m = active.match(readText(queue + QStringLiteral("scheduler")));
    q.scheduler = m.hasMatch() ? m.captured(1) : QString();
    bool ok = false;
    int depth = readText(QStringLiteral("/sys/block/%1/device/queue_depth").arg(blockName)).toInt(&ok);
    if (ok)
        q.queueDepth = depth;
    return q;
}

double readHwmonTemperature(const QString &blockName)
{
    const QString dir = deviceDir(blockName);
    if (dir.isEmpty())
        return std::numeric_limits<double>::quiet_NaN();
    for (const QString &base : {dir, dir + QStringLiteral("/hwmon")}) {
        const QStringList hwmons = QDir(base).entryList({QStringLiteral("hwmon*")}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &hwmon : hwmons) {
            bool ok = false;
            double milli = readText(base + QLatin1Char('/') + hwmon + QStringLiteral("/temp1_input")).toDouble(&ok);
            if (ok)
                return milli / 1000.0;
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace dw
