#include "smartattribute.h"

#include <QTest>

using namespace dw;

class TestSmartAttribute : public QObject
{
    Q_OBJECT

private slots:
    void powerOnHours()
    {
        const SmartAttribute a = decodeUdisksAttribute(9, QStringLiteral("power-on-hours"), 50, 52, 52, 0, 152503200000LL, 2);
        QCOMPARE(a.raw, 42362);
        QVERIFY(a.rawKnown);
        QCOMPARE(a.name, QStringLiteral("Power-On Time Count"));
    }

    void spinUpTimeStaysInMilliseconds()
    {
        const SmartAttribute a = decodeUdisksAttribute(3, QStringLiteral("spin-up-time"), 3, 94, 93, 0, 4500, 2);
        QCOMPARE(a.raw, 4500);
    }

    void temperature()
    {
        const SmartAttribute a = decodeUdisksAttribute(194, QStringLiteral("temperature-celsius-2"), 34, 44, 51, 0, 317150, 4);
        QCOMPARE(a.raw, 44);
    }

    void lbasWritten()
    {
        // libatasmart reports LBAs * 65536 * 512 / 10^6 as megabytes.
        const qint64 lbas = 1000000;
        const qint64 pretty = lbas * 65536 * 512 / 1000000;
        const SmartAttribute a = decodeUdisksAttribute(241, QStringLiteral("total-lbas-written"), 0, 100, 253, 0, pretty, 7);
        QCOMPARE(a.raw, lbas);
    }

    void plainCounts()
    {
        const SmartAttribute a = decodeUdisksAttribute(199, QStringLiteral("udma-crc-error-count"), 62, 200, 200, 0, 1650, 1);
        QCOMPARE(a.raw, 1650);
        QVERIFY(!a.preFailure());
        const SmartAttribute b = decodeUdisksAttribute(5, QStringLiteral("reallocated-sector-count"), 51, 100, 100, 10, 3, 3);
        QCOMPARE(b.raw, 3);
        QVERIFY(b.preFailure());
    }

    void unknownUnit()
    {
        const SmartAttribute a = decodeUdisksAttribute(170, QStringLiteral("attribute-170"), 0, 100, 100, 10, 0, 0);
        QVERIFY(!a.rawKnown);
        QCOMPARE(a.name, QStringLiteral("Available Reserved Space"));
    }

    void thresholds()
    {
        SmartAttribute a;
        a.threshold = 10;
        a.value = 10;
        a.worst = 10;
        QVERIFY(a.failingNow());
        QVERIFY(a.failedInPast());
        a.value = 100;
        QVERIFY(!a.failingNow());
        QVERIFY(a.failedInPast());
        a.threshold = 0;
        a.value = 0;
        a.worst = 0;
        QVERIFY(!a.failingNow());
        QVERIFY(!a.failedInPast());
    }

    void flags()
    {
        QCOMPARE(attributeFlagsText(0x33), QStringLiteral("Pre-failure, Online, Event Count, Self Preserving"));
        QCOMPARE(attributeFlagsText(0x00), QStringLiteral("Advisory"));
    }

    void find()
    {
        QList<SmartAttribute> list(2);
        list[0].id = 5;
        list[1].id = 9;
        QCOMPARE(findAttribute(list, 9), &list[1]);
        QCOMPARE(findAttribute(list, 7), nullptr);
    }
};

QTEST_GUILESS_MAIN(TestSmartAttribute)
#include "tst_smartattribute.moc"
