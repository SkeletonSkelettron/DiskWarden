#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Persistent configuration, exposed to QML as the AppSettings singleton.
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int detectionMinutes READ detectionMinutes WRITE setDetectionMinutes NOTIFY changed)
    Q_PROPERTY(int hddTemperatureWarning READ hddTemperatureWarning WRITE setHddTemperatureWarning NOTIFY changed)
    Q_PROPERTY(int hddTemperatureCritical READ hddTemperatureCritical WRITE setHddTemperatureCritical NOTIFY changed)
    Q_PROPERTY(int ssdTemperatureWarning READ ssdTemperatureWarning WRITE setSsdTemperatureWarning NOTIFY changed)
    Q_PROPERTY(int ssdTemperatureCritical READ ssdTemperatureCritical WRITE setSsdTemperatureCritical NOTIFY changed)
    Q_PROPERTY(int nvmeTemperatureWarning READ nvmeTemperatureWarning WRITE setNvmeTemperatureWarning NOTIFY changed)
    Q_PROPERTY(int nvmeTemperatureCritical READ nvmeTemperatureCritical WRITE setNvmeTemperatureCritical NOTIFY changed)
    Q_PROPERTY(int healthWarning READ healthWarning WRITE setHealthWarning NOTIFY changed)
    Q_PROPERTY(int healthCritical READ healthCritical WRITE setHealthCritical NOTIFY changed)
    Q_PROPERTY(bool alertFailure READ alertFailure WRITE setAlertFailure NOTIFY changed)
    Q_PROPERTY(bool alertHealth READ alertHealth WRITE setAlertHealth NOTIFY changed)
    Q_PROPERTY(bool alertTemperature READ alertTemperature WRITE setAlertTemperature NOTIFY changed)
    Q_PROPERTY(bool alertLogEntry READ alertLogEntry WRITE setAlertLogEntry NOTIFY changed)
    Q_PROPERTY(bool alertFreeSpace READ alertFreeSpace WRITE setAlertFreeSpace NOTIFY changed)
    Q_PROPERTY(int freeSpacePercent READ freeSpacePercent WRITE setFreeSpacePercent NOTIFY changed)
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY changed)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized NOTIFY changed)
    Q_PROPERTY(bool hexData READ hexData WRITE setHexData NOTIFY changed)
    Q_PROPERTY(bool wakeSleepingDisks READ wakeSleepingDisks WRITE setWakeSleepingDisks NOTIFY changed)
    Q_PROPERTY(int currentTab READ currentTab WRITE setCurrentTab NOTIFY changed)
    Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY changed)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY changed)

public:
    static AppSettings *instance();
    static AppSettings *create(QQmlEngine *, QJSEngine *);

    int detectionMinutes() const { return intValue("detectionMinutes", 5); }
    void setDetectionMinutes(int v) { setValue("detectionMinutes", v); }
    int hddTemperatureWarning() const { return intValue("hddTemperatureWarning", 50); }
    void setHddTemperatureWarning(int v) { setValue("hddTemperatureWarning", v); }
    int hddTemperatureCritical() const { return intValue("hddTemperatureCritical", 60); }
    void setHddTemperatureCritical(int v) { setValue("hddTemperatureCritical", v); }
    int ssdTemperatureWarning() const { return intValue("ssdTemperatureWarning", 50); }
    void setSsdTemperatureWarning(int v) { setValue("ssdTemperatureWarning", v); }
    int ssdTemperatureCritical() const { return intValue("ssdTemperatureCritical", 60); }
    void setSsdTemperatureCritical(int v) { setValue("ssdTemperatureCritical", v); }
    int nvmeTemperatureWarning() const { return intValue("nvmeTemperatureWarning", 50); }
    void setNvmeTemperatureWarning(int v) { setValue("nvmeTemperatureWarning", v); }
    int nvmeTemperatureCritical() const { return intValue("nvmeTemperatureCritical", 60); }
    void setNvmeTemperatureCritical(int v) { setValue("nvmeTemperatureCritical", v); }
    int healthWarning() const { return intValue("healthWarning", 50); }
    void setHealthWarning(int v) { setValue("healthWarning", v); }
    int healthCritical() const { return intValue("healthCritical", 25); }
    void setHealthCritical(int v) { setValue("healthCritical", v); }
    bool alertFailure() const { return boolValue("alertFailure", true); }
    void setAlertFailure(bool v) { setValue("alertFailure", v); }
    bool alertHealth() const { return boolValue("alertHealth", true); }
    void setAlertHealth(bool v) { setValue("alertHealth", v); }
    bool alertTemperature() const { return boolValue("alertTemperature", true); }
    void setAlertTemperature(bool v) { setValue("alertTemperature", v); }
    bool alertLogEntry() const { return boolValue("alertLogEntry", true); }
    void setAlertLogEntry(bool v) { setValue("alertLogEntry", v); }
    bool alertFreeSpace() const { return boolValue("alertFreeSpace", true); }
    void setAlertFreeSpace(bool v) { setValue("alertFreeSpace", v); }
    int freeSpacePercent() const { return intValue("freeSpacePercent", 10); }
    void setFreeSpacePercent(int v) { setValue("freeSpacePercent", v); }
    bool closeToTray() const { return boolValue("closeToTray", true); }
    void setCloseToTray(bool v) { setValue("closeToTray", v); }
    bool startMinimized() const { return boolValue("startMinimized", false); }
    void setStartMinimized(bool v) { setValue("startMinimized", v); }
    bool hexData() const { return boolValue("hexData", true); }
    void setHexData(bool v) { setValue("hexData", v); }
    bool wakeSleepingDisks() const { return boolValue("wakeSleepingDisks", false); }
    void setWakeSleepingDisks(bool v) { setValue("wakeSleepingDisks", v); }
    int currentTab() const { return intValue("currentTab", 0); }
    void setCurrentTab(int v) { setValue("currentTab", v); }
    // 0 follows the desktop's color scheme, 1 light, 2 dark (Palenight).
    int theme() const { return intValue("theme", 0); }
    void setTheme(int v) { setValue("theme", v); }

    bool autostart() const;
    void setAutostart(bool enabled);

    // Temperature thresholds for a Disk::Kind.
    Q_INVOKABLE int temperatureWarning(int kind) const;
    Q_INVOKABLE int temperatureCritical(int kind) const;

    // Per-disk options of the S.M.A.R.T. page.
    QSet<int> disabledAttributes(const QString &diskKey) const;
    void setAttributeEnabled(const QString &diskKey, int id, bool enabled);
    QHash<int, qint64> attributeOffsets(const QString &diskKey) const;
    void setAttributeOffset(const QString &diskKey, int id, qint64 offset);

signals:
    void changed();
    void diskOptionsChanged(const QString &diskKey);

private:
    explicit AppSettings(QObject *parent = nullptr);
    int intValue(const char *key, int fallback) const;
    bool boolValue(const char *key, bool fallback) const;
    void setValue(const char *key, const QVariant &value);
    QString diskGroup(const QString &diskKey) const;

    mutable QSettings m_settings;
};
