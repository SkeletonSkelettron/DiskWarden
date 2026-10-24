#pragma once

#include "disk.h"
#include "diskstats.h"
#include "surfacetest.h"
#include "udisks.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Finds the disks through udisks2 and keeps them up to date: S.M.A.R.T.
// detection, temperatures, transfer rates and hot-plugging.
class DiskManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QList<Disk *> disks READ disks NOTIFY disksChanged)
    Q_PROPERTY(QVariantList volumes READ volumes NOTIFY volumesChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(Disk *currentDisk READ currentDisk NOTIFY currentChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY availableChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY trayAvailableChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    static DiskManager *instance();
    static DiskManager *create(QQmlEngine *, QJSEngine *);

    void start();

    QList<Disk *> disks() const { return m_disks; }
    QVariantList volumes() const;
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);
    Disk *currentDisk() const;
    QDateTime lastUpdated() const { return m_lastUpdated; }
    bool available() const { return m_available; }
    QString errorText() const { return m_errorText; }
    bool refreshing() const { return m_pendingRefreshes > 0; }
    bool trayAvailable() const { return m_trayAvailable; }
    void setTrayAvailable(bool available);
    QString version() const;
    // The worst status of all disks: -1 unknown, 0 good, 1 warning, 2 bad.
    int worstLevel() const;

    Q_INVOKABLE void refresh();
    // The surface test of a disk, created on first use.
    Q_INVOKABLE SurfaceTest *surfaceTest(Disk *disk);
    Q_INVOKABLE void requestConfiguration() { emit configurationRequested(); }

signals:
    void disksChanged();
    void volumesChanged();
    void currentChanged();
    void lastUpdatedChanged();
    void availableChanged();
    void refreshingChanged();
    void trayAvailableChanged();
    void statusChanged();
    void configurationRequested();

private slots:
    void onPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated,
                             const QDBusMessage &message);
    void onObjectsChanged();

private:
    explicit DiskManager(QObject *parent = nullptr);
    void enumerate();
    void applyObjects(const DBusManagedObjects &objects);
    void samplePerformance();
    void updateDetectionInterval();

    QList<Disk *> m_disks;
    QHash<Disk *, SurfaceTest *> m_surfaceTests;
    int m_currentIndex = 0;
    QDateTime m_lastUpdated;
    bool m_available = true;
    QString m_errorText;
    int m_pendingRefreshes = 0;
    bool m_trayAvailable = false;
    bool m_firstEnumeration = true;

    QTimer m_detectionTimer;
    QTimer m_perfTimer;
    QTimer m_temperatureTimer;
    QTimer m_volumeTimer;
    QTimer m_enumerateTimer;
    QHash<QString, dw::DiskStat> m_lastStats;
    QElapsedTimer m_statsClock;
};
