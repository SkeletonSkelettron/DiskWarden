#include "diskstats.h"

#include <QTest>

using namespace hg;

class TestDiskStats : public QObject
{
    Q_OBJECT

private slots:
    void parse()
    {
        const QByteArray content = "   8       0 sda 152 0 5824 323 10 2 80 5 0 313 323 0 0 0 0 0 0\n"
                                   " 259       7 nvme0n1 1654 2491 40698 418 6 0 24 1 0 66 421 0 0 0 0 3 1\n"
                                   "bad line\n";
        const QHash<QString, DiskStat> stats = parseDiskStats(content);
        QCOMPARE(stats.size(), 2);
        const DiskStat sda = stats.value(QStringLiteral("sda"));
        QCOMPARE(sda.readsCompleted, 152u);
        QCOMPARE(sda.sectorsRead, 5824u);
        QCOMPARE(sda.writesCompleted, 10u);
        QCOMPARE(sda.sectorsWritten, 80u);
        QCOMPARE(sda.ioTicksMs, 313u);
        QCOMPARE(stats.value(QStringLiteral("nvme0n1")).sectorsRead, 40698u);
    }

    void rates()
    {
        DiskStat a;
        a.sectorsRead = 1000;
        a.sectorsWritten = 0;
        a.ioTicksMs = 100;
        DiskStat b = a;
        b.sectorsRead += 2048; // 1 MiB
        b.sectorsWritten += 4096;
        b.ioTicksMs += 250;
        const DiskRates r = diskRates(a, b, 500);
        QCOMPARE(r.readBytesPerSecond, 2.0 * 1024 * 1024);
        QCOMPARE(r.writeBytesPerSecond, 4.0 * 1024 * 1024);
        QCOMPARE(r.utilization, 50.0);
    }

    void countersGoingBackwards()
    {
        DiskStat a;
        a.sectorsRead = 5000;
        DiskStat b;
        const DiskRates r = diskRates(a, b, 1000);
        QCOMPARE(r.readBytesPerSecond, 0.0);
        QCOMPARE(diskRates(a, b, 0).utilization, 0.0);
    }
};

QTEST_GUILESS_MAIN(TestDiskStats)
#include "tst_diskstats.moc"
