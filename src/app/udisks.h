#pragma once

#include "healthcalculator.h"
#include "smartattribute.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QMap>
#include <QStringList>
#include <QVariantMap>

using DBusInterfaces = QMap<QString, QVariantMap>;
using DBusManagedObjects = QMap<QDBusObjectPath, DBusInterfaces>;

Q_DECLARE_METATYPE(DBusInterfaces)
Q_DECLARE_METATYPE(DBusManagedObjects)

namespace udisks {

inline const QString Service = QStringLiteral("org.freedesktop.UDisks2");
inline const QString RootPath = QStringLiteral("/org/freedesktop/UDisks2");
inline const QString DriveIface = QStringLiteral("org.freedesktop.UDisks2.Drive");
inline const QString AtaIface = QStringLiteral("org.freedesktop.UDisks2.Drive.Ata");
inline const QString NvmeControllerIface = QStringLiteral("org.freedesktop.UDisks2.NVMe.Controller");
inline const QString NvmeNamespaceIface = QStringLiteral("org.freedesktop.UDisks2.NVMe.Namespace");
inline const QString BlockIface = QStringLiteral("org.freedesktop.UDisks2.Block");
inline const QString PartitionIface = QStringLiteral("org.freedesktop.UDisks2.Partition");
inline const QString PartitionTableIface = QStringLiteral("org.freedesktop.UDisks2.PartitionTable");
inline const QString FilesystemIface = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
inline const QString PropertiesIface = QStringLiteral("org.freedesktop.DBus.Properties");
inline const QString ObjectManagerIface = QStringLiteral("org.freedesktop.DBus.ObjectManager");

void registerTypes();
QDBusConnection bus();

QDBusMessage methodCall(const QString &path, const QString &iface, const QString &method,
                        const QVariantList &args = {});

// Helpers for property values that arrive as QDBusArgument.
QString byteString(const QVariant &value);      // ay
QStringList byteStringList(const QVariant &value); // aay
QStringList stringList(const QVariant &value);  // as
QString objectPath(const QVariant &value);      // o
QList<double> uint16List(const QVariant &value); // aq

// Parses the reply of Drive.Ata.SmartGetAttributes().
QList<dw::SmartAttribute> parseAtaAttributes(const QDBusMessage &reply);
// Parses the reply of NVMe.Controller.SmartGetAttributes() together with
// the controller's properties.
dw::NvmeHealth parseNvmeHealth(const QVariantMap &attributes, const QVariantMap &controller);

} // namespace udisks
