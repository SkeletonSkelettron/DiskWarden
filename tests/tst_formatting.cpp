#include "formatting.h"

#include <QLocale>
#include <QTest>

using namespace hg;

class TestFormatting : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void sizes()
    {
        QCOMPARE(formatSize(256060514304LL), QStringLiteral("238.5 GB"));
        QCOMPARE(formatSize(4000787030016LL), QStringLiteral("3.6 TB"));
        QCOMPARE(formatSize(512), QStringLiteral("512 bytes"));
        QCOMPARE(formatData(10939904638566LL), QStringLiteral("9.95 TB"));
    }

    void times()
    {
        QCOMPARE(formatPowerOnTime(33), QStringLiteral("1 days, 9 hours"));
        QCOMPARE(formatPowerOnTime(-1), QStringLiteral("unknown"));
        QCOMPARE(formatRemainingDays(1001), QStringLiteral("more than 1000 days"));
        QCOMPARE(formatRemainingDays(35), QStringLiteral("35 days"));
        QCOMPARE(formatElapsed(466), QStringLiteral("00:07:46"));
    }

    void raw()
    {
        QCOMPARE(formatRawHex(0x21), QStringLiteral("000000000021"));
        QCOMPARE(formatRawHex(0x100010001LL), QStringLiteral("000100010001"));
    }

    void rating()
    {
        QCOMPARE(ratingWord(100), QStringLiteral("Excellent"));
        QCOMPARE(ratingWord(85), QStringLiteral("Good"));
        QCOMPARE(ratingWord(60), QStringLiteral("Fair"));
        QCOMPARE(ratingWord(30), QStringLiteral("Poor"));
        QCOMPARE(ratingWord(10), QStringLiteral("Critical"));
        QCOMPARE(ratingWord(-1), QStringLiteral("Unknown"));
    }
};

QTEST_GUILESS_MAIN(TestFormatting)
#include "tst_formatting.moc"
