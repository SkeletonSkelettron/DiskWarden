#include "appsettings.h"

#include "smartattribute.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQmlEngine>
#include <QStandardPaths>

namespace {

QString autostartFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/autostart/diskwarden.desktop");
}

} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
}

AppSettings *AppSettings::instance()
{
    static AppSettings *settings = new AppSettings(qApp);
    return settings;
}

AppSettings *AppSettings::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

int AppSettings::intValue(const char *key, int fallback) const
{
    return m_settings.value(QLatin1String(key), fallback).toInt();
}

bool AppSettings::boolValue(const char *key, bool fallback) const
{
    return m_settings.value(QLatin1String(key), fallback).toBool();
}

void AppSettings::setValue(const char *key, const QVariant &value)
{
    if (m_settings.value(QLatin1String(key)) == value)
        return;
    m_settings.setValue(QLatin1String(key), value);
    emit changed();
}

bool AppSettings::autostart() const
{
    return QFile::exists(autostartFile());
}

void AppSettings::setAutostart(bool enabled)
{
    if (enabled == autostart())
        return;
    const QString path = autostartFile();
    if (enabled) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write("[Desktop Entry]\n"
                       "Type=Application\n"
                       "Name=DiskWarden\n"
                       "Comment=Disk health and temperature monitor\n"
                       "Icon=diskwarden\n");
            file.write("Exec=\"" + QCoreApplication::applicationFilePath().toUtf8() + "\" --minimized\n");
            file.write("X-GNOME-Autostart-enabled=true\n");
        }
    } else {
        QFile::remove(path);
    }
    emit changed();
}

int AppSettings::temperatureWarning(int kind) const
{
    switch (dw::DiskKind(kind)) {
    case dw::DiskKind::Ssd:
        return ssdTemperatureWarning();
    case dw::DiskKind::Nvme:
        return nvmeTemperatureWarning();
    default:
        return hddTemperatureWarning();
    }
}

int AppSettings::temperatureCritical(int kind) const
{
    switch (dw::DiskKind(kind)) {
    case dw::DiskKind::Ssd:
        return ssdTemperatureCritical();
    case dw::DiskKind::Nvme:
        return nvmeTemperatureCritical();
    default:
        return hddTemperatureCritical();
    }
}

QString AppSettings::diskGroup(const QString &diskKey) const
{
    QString key = diskKey;
    key.replace(QLatin1Char('/'), QLatin1Char('_'));
    return QStringLiteral("disks/") + key + QLatin1Char('/');
}

QSet<int> AppSettings::disabledAttributes(const QString &diskKey) const
{
    QSet<int> result;
    const QStringList ids = m_settings.value(diskGroup(diskKey) + QStringLiteral("disabled")).toStringList();
    for (const QString &id : ids)
        result.insert(id.toInt());
    return result;
}

void AppSettings::setAttributeEnabled(const QString &diskKey, int id, bool enabled)
{
    QSet<int> ids = disabledAttributes(diskKey);
    if (enabled == !ids.contains(id))
        return;
    if (enabled)
        ids.remove(id);
    else
        ids.insert(id);
    QStringList list;
    for (int i : std::as_const(ids))
        list << QString::number(i);
    m_settings.setValue(diskGroup(diskKey) + QStringLiteral("disabled"), list);
    emit diskOptionsChanged(diskKey);
}

QHash<int, qint64> AppSettings::attributeOffsets(const QString &diskKey) const
{
    QHash<int, qint64> result;
    const QVariantMap map = m_settings.value(diskGroup(diskKey) + QStringLiteral("offsets")).toMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        result.insert(it.key().toInt(), it.value().toLongLong());
    return result;
}

void AppSettings::setAttributeOffset(const QString &diskKey, int id, qint64 offset)
{
    QVariantMap map = m_settings.value(diskGroup(diskKey) + QStringLiteral("offsets")).toMap();
    if (offset == 0)
        map.remove(QString::number(id));
    else
        map.insert(QString::number(id), offset);
    m_settings.setValue(diskGroup(diskKey) + QStringLiteral("offsets"), map);
    emit diskOptionsChanged(diskKey);
}
