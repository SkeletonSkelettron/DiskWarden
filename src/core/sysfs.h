#pragma once

#include <QString>
#include <QtGlobal>
#include <limits>

namespace hg {

struct LinkInfo {
    QString interfaceName;   // "S-ATA", "NVMe", "USB"
    QString current;         // "6.0 Gbps", "PCI-Express 4.0 x4"
    QString maximum;
    double ratio = 1.0;      // current speed / maximum speed
    bool maximumKnown = false;
    QString controller;      // PCI name of the disk controller
    QString controllerDriver;
};

struct QueueInfo {
    bool rotational = false;
    int logicalBlockSize = 512;
    int physicalBlockSize = 512;
    qint64 discardMaxBytes = 0;
    QString writeCache;
    QString scheduler;
    int queueDepth = -1;
};

// Reads the interface details of a block device, e.g. "sda" or "nvme0n1".
LinkInfo readLinkInfo(const QString &blockName);
QueueInfo readQueueInfo(const QString &blockName);
// The temperature from a hwmon sensor of the disk (nvme, drivetemp), or NaN.
double readHwmonTemperature(const QString &blockName);

// "6.0 Gbps" -> 6.0, "16.0 GT/s PCIe" -> 16.0, unknown -> 0.
double parseLinkSpeed(const QString &text);
// PCI Express generation for a transfer rate in GT/s: 8.0 -> "3.0".
QString pcieGeneration(double gts);
// Looks up a PCI device name in the hwdata pci.ids database.
QString pciDeviceName(quint16 vendor, quint16 device);

} // namespace hg
