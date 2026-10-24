#include "healthcalculator.h"

#include <QTest>

using namespace dw;

namespace {

SmartAttribute attribute(int id, qint64 raw, int value = 100, int worst = 100, int threshold = 10, int flags = 0x33)
{
    SmartAttribute a;
    a.id = id;
    a.name = ataAttributeName(id);
    a.raw = raw;
    a.rawKnown = true;
    a.value = value;
    a.worst = worst;
    a.threshold = threshold;
    a.flags = flags;
    return a;
}

HealthInput healthyHdd()
{
    HealthInput in;
    in.kind = DiskKind::Hdd;
    in.model = QStringLiteral("WDC WD30EFRX-68EUZN0");
    in.smartAvailable = true;
    in.powerOnHours = 1000;
    in.attributes = {attribute(1, 0), attribute(5, 0), attribute(9, 1000, 99, 99, 0), attribute(197, 0), attribute(198, 0)};
    return in;
}

} // namespace

class TestHealthCalculator : public QObject
{
    Q_OBJECT

private slots:
    void perfectDisk()
    {
        const HealthResult r = HealthCalculator::evaluate(healthyHdd());
        QCOMPARE(r.health, 100);
        QCOMPARE(r.performance, 100);
        QCOMPARE(r.level, 0);
        QCOMPARE(r.statusWord, QStringLiteral("PERFECT"));
        QCOMPARE(r.recommendation, QStringLiteral("No actions needed."));
        QCOMPARE(r.remainingDays, 1001);
    }

    void reallocatedAndPendingSectors()
    {
        // The worked example of the "analyse data field" method.
        HealthInput in = healthyHdd();
        in.attributes[1].raw = 2;
        QCOMPARE(HealthCalculator::evaluate(in).health, 98);
        in.attributes[3].raw = 5;
        const HealthResult r = HealthCalculator::evaluate(in);
        QCOMPARE(r.health, 95);
        QCOMPARE(r.level, 1);
        QVERIFY(r.description.join(QLatin1Char(' ')).contains(QLatin1String("2 bad sectors")));
    }

    void deductionIsLimited()
    {
        HealthInput in = healthyHdd();
        in.attributes[4].raw = 512; // off-line uncorrectable, limit 70 %
        QCOMPARE(HealthCalculator::evaluate(in).health, 30);
    }

    void onlyLower16BitsCount()
    {
        HealthInput in = healthyHdd();
        in.attributes[1].raw = 0x10002;
        QCOMPARE(HealthCalculator::evaluate(in).health, 98);
    }

    void offsetsAndDisabledAttributes()
    {
        HealthInput in = healthyHdd();
        in.attributes[1].raw = 10;
        in.offsets.insert(5, 8);
        QCOMPARE(HealthCalculator::evaluate(in).health, 98);
        in.disabledAttributes.insert(5);
        QCOMPARE(HealthCalculator::evaluate(in).health, 100);
    }

    void seagateSeekErrorRate()
    {
        HealthInput in = healthyHdd();
        in.model = QStringLiteral("ST4000VX000-2AG166");
        // 40 errors in the upper word, half a billion seeks below: a rate
        // that the normalized value judges.
        in.attributes << attribute(7, 0x002822144A2DLL, 71, 60, 30);
        QCOMPARE(HealthCalculator::evaluate(in).health, 100);
    }

    void seekErrorCounts()
    {
        HealthInput in = healthyHdd();
        in.attributes << attribute(7, 123456789);
        QCOMPARE(HealthCalculator::evaluate(in).health, 100);
        in.attributes.last().raw = 4;
        const HealthResult r = HealthCalculator::evaluate(in);
        QCOMPARE(r.health, 98);
        QVERIFY(r.description.join(QLatin1Char(' ')).contains(QLatin1String("4 times")));
    }

    void failingDisk()
    {
        HealthInput in = healthyHdd();
        in.smartFailing = true;
        const HealthResult r = HealthCalculator::evaluate(in);
        QCOMPARE(r.health, 10);
        QCOMPARE(r.level, 2);
    }

    void attributeAtThreshold()
    {
        HealthInput in = healthyHdd();
        in.attributes[1].value = 10; // threshold 10, pre-failure
        QCOMPARE(HealthCalculator::evaluate(in).health, 10);
    }

    void ssdWear()
    {
        HealthInput in;
        in.kind = DiskKind::Ssd;
        in.smartAvailable = true;
        in.attributes = {attribute(5, 0), attribute(177, 120, 95, 95, 0, 0x13)};
        const HealthResult r = HealthCalculator::evaluate(in);
        QCOMPARE(r.health, 95);
        QCOMPARE(r.healthSources.size(), 1);
    }

    void nvme()
    {
        HealthInput in;
        in.kind = DiskKind::Nvme;
        in.smartAvailable = true;
        in.powerOnHours = 13545;
        in.nvme.valid = true;
        in.nvme.percentUsed = 7;
        in.nvme.availableSpare = 100;
        in.nvme.spareThreshold = 10;
        in.nvme.mediaErrors = 0;
        HealthResult r = HealthCalculator::evaluate(in);
        QCOMPARE(r.health, 93);
        QCOMPARE(r.remainingDays, 1001);

        in.nvme.availableSpare = 55; // half of the usable spare is gone
        r = HealthCalculator::evaluate(in);
        QCOMPARE(r.health, 50);

        in.nvme.availableSpare = 100;
        in.nvme.criticalWarning = NvmeReliabilityDegraded;
        QCOMPARE(HealthCalculator::evaluate(in).health, 10);
    }

    void performance()
    {
        HealthInput in = healthyHdd();
        in.linkRatio = 0.5;
        in.linkDescription = QStringLiteral("3.0 Gbps instead of 6.0 Gbps");
        QCOMPARE(HealthCalculator::evaluate(in).performance, 75);
        in.linkRatio = 1.0;
        in.recentCrcErrors = 10;
        QCOMPARE(HealthCalculator::evaluate(in).performance, 90);
        in.recentCrcErrors = 0;
        in.attributes << attribute(8, 0, 52, 52, 4); // seek time performance
        QCOMPARE(HealthCalculator::evaluate(in).performance, 88);
    }

    void remainingLifetime()
    {
        // 600 days of use and 70 % health: 1225 days * 0.7 * 0.7.
        QCOMPARE(HealthCalculator::remainingLifetimeDays(DiskKind::Hdd, 70, 600 * 24, 0, false), 600);
        QCOMPARE(HealthCalculator::remainingLifetimeDays(DiskKind::Hdd, 100, 90000, 0, false), 1001);
        // Old disks keep a quarter of the five years as the base.
        QCOMPARE(HealthCalculator::remainingLifetimeDays(DiskKind::Hdd, 50, 90000, 0, false), 114);
        // 10 % of the endurance in 100 days: 900 more days at that rate.
        QCOMPARE(HealthCalculator::remainingLifetimeDays(DiskKind::Ssd, 90, 2400, 0, true), 900);
        QCOMPARE(HealthCalculator::remainingLifetimeDays(DiskKind::Hdd, -1, 100, 0, false), -1);
    }

    void smartUnavailable()
    {
        HealthInput in;
        in.kind = DiskKind::Hdd;
        const HealthResult r = HealthCalculator::evaluate(in);
        QCOMPARE(r.health, -1);
        QCOMPARE(r.performance, -1);
    }

    void temperatureWarning()
    {
        HealthInput in = healthyHdd();
        in.temperature = 52;
        in.temperatureWarning = 45;
        in.temperatureCritical = 55;
        QCOMPARE(HealthCalculator::evaluate(in).level, 1);
        in.temperature = 58;
        QCOMPARE(HealthCalculator::evaluate(in).level, 2);
    }
};

QTEST_GUILESS_MAIN(TestHealthCalculator)
#include "tst_healthcalculator.moc"
