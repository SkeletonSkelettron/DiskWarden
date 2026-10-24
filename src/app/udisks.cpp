#include "udisks.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <cmath>

namespace udisks {

namespace {

bool isArgument(const QVariant &value)
{
    return value.userType() == qMetaTypeId<QDBusArgument>();
}

} // namespace

void registerTypes()
{
    qDBusRegisterMetaType<DBusInterfaces>();
    qDBusRegisterMetaType<DBusManagedObjects>();
    qDBusRegisterMetaType<QList<QByteArray>>();
    qDBusRegisterMetaType<QList<quint16>>();
}

QDBusConnection bus()
{
    return QDBusConnection::systemBus();
}

QDBusMessage methodCall(const QString &path, const QString &iface, const QString &method,
                        const QVariantList &args)
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, path, iface, method);
    message.setArguments(args);
    message.setInteractiveAuthorizationAllowed(true);
    return message;
}

QString byteString(const QVariant &value)
{
    QByteArray bytes = isArgument(value) ? qdbus_cast<QByteArray>(value) : value.toByteArray();
    while (bytes.endsWith('\0'))
        bytes.chop(1);
    return QString::fromLocal8Bit(bytes);
}

QStringList byteStringList(const QVariant &value)
{
    QStringList result;
    const QList<QByteArray> list = qdbus_cast<QList<QByteArray>>(value);
    for (QByteArray bytes : list) {
        while (bytes.endsWith('\0'))
            bytes.chop(1);
        result << QString::fromLocal8Bit(bytes);
    }
    return result;
}

QStringList stringList(const QVariant &value)
{
    return isArgument(value) ? qdbus_cast<QStringList>(value) : value.toStringList();
}

QString objectPath(const QVariant &value)
{
    return value.value<QDBusObjectPath>().path();
}

QList<double> uint16List(const QVariant &value)
{
    QList<double> result;
    const QList<quint16> list = isArgument(value) ? qdbus_cast<QList<quint16>>(value)
                                                  : value.value<QList<quint16>>();
    for (quint16 v : list)
        result << double(v);
    return result;
}

QList<hg::SmartAttribute> parseAtaAttributes(const QDBusMessage &reply)
{
    QList<hg::SmartAttribute> attributes;
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return attributes;
    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
    // a(ysqiiixia{sv}): id, name, flags, value, worst, threshold, pretty,
    // pretty unit, expansion
    arg.beginArray();
    while (!arg.atEnd()) {
        uchar id = 0;
        QString name;
        ushort flags = 0;
        int value = 0;
        int worst = 0;
        int threshold = 0;
        qlonglong pretty = 0;
        int unit = 0;
        QVariantMap expansion;
        arg.beginStructure();
        arg >> id >> name >> flags >> value >> worst >> threshold >> pretty >> unit >> expansion;
        arg.endStructure();
        attributes << hg::decodeUdisksAttribute(id, name, flags, value, worst, threshold, pretty, unit);
    }
    arg.endArray();
    return attributes;
}

hg::NvmeHealth parseNvmeHealth(const QVariantMap &attributes, const QVariantMap &controller)
{
    hg::NvmeHealth n;
    n.valid = !attributes.isEmpty();
    auto number = [&](const char *key, qint64 fallback = -1) -> qint64 {
        const QVariant v = attributes.value(QLatin1String(key));
        return v.isValid() ? v.toLongLong() : fallback;
    };
    auto kelvin = [](double k) { return k > 0 ? k - 273.15 : std::numeric_limits<double>::quiet_NaN(); };

    n.availableSpare = int(number("avail_spare"));
    n.spareThreshold = int(number("spare_thresh"));
    n.percentUsed = int(number("percent_used"));
    n.bytesRead = number("total_data_read");
    n.bytesWritten = number("total_data_written");
    n.controllerBusyMinutes = number("ctrl_busy_time");
    n.powerCycles = number("power_cycles");
    n.unsafeShutdowns = number("unsafe_shutdowns");
    n.mediaErrors = number("media_errors");
    n.errorLogEntries = number("num_err_log_entries");
    n.warningTemperatureMinutes = number("warning_temp_time");
    n.criticalTemperatureMinutes = number("critical_temp_time");
    n.warningTemperature = kelvin(number("wctemp", 0));
    n.criticalTemperature = kelvin(number("cctemp", 0));
    for (double k : uint16List(attributes.value(QStringLiteral("temp_sensors")))) {
        if (k > 0)
            n.sensors << kelvin(k);
    }

    n.powerOnHours = controller.value(QStringLiteral("SmartPowerOnHours"), -1).toLongLong();
    n.temperature = kelvin(controller.value(QStringLiteral("SmartTemperature")).toDouble());
    const QStringList warnings = stringList(controller.value(QStringLiteral("SmartCriticalWarning")));
    for (const QString &w : warnings) {
        if (w == QLatin1String("spare"))
            n.criticalWarning |= hg::NvmeSpareLow;
        else if (w == QLatin1String("temperature"))
            n.criticalWarning |= hg::NvmeTemperature;
        else if (w == QLatin1String("degraded"))
            n.criticalWarning |= hg::NvmeReliabilityDegraded;
        else if (w == QLatin1String("readonly") || w == QLatin1String("pmr_readonly"))
            n.criticalWarning |= hg::NvmeReadOnly;
        else if (w == QLatin1String("volatile_mem"))
            n.criticalWarning |= hg::NvmeVolatileBackupFailed;
    }
    return n;
}

} // namespace udisks
