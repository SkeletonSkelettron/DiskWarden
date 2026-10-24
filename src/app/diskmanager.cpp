#include "diskmanager.h"

#include "appsettings.h"
#include "disk.h"
#include "surfacetest.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QQmlEngine>
#include <QSet>
#include <algorithm>

DiskManager::DiskManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_detectionTimer, &QTimer::timeout, this, &DiskManager::refresh);
    m_perfTimer.setInterval(1000);
    connect(&m_perfTimer, &QTimer::timeout, this, &DiskManager::samplePerformance);
    m_temperatureTimer.setInterval(10000);
    connect(&m_temperatureTimer, &QTimer::timeout, this, [this] {
        for (Disk *disk : std::as_const(m_disks))
            disk->pollTemperature();
    });
    m_volumeTimer.setInterval(30000);
    connect(&m_volumeTimer, &QTimer::timeout, this, [this] {
        for (Disk *disk : std::as_const(m_disks))
            disk->updateVolumeUsage();
        emit volumesChanged();
    });
    m_enumerateTimer.setSingleShot(true);
    m_enumerateTimer.setInterval(700);
    connect(&m_enumerateTimer, &QTimer::timeout, this, &DiskManager::enumerate);
    connect(AppSettings::instance(), &AppSettings::changed, this, &DiskManager::updateDetectionInterval);
}

DiskManager *DiskManager::instance()
{
    static DiskManager *manager = new DiskManager(qApp);
    return manager;
}

DiskManager *DiskManager::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

QString DiskManager::version() const
{
    return QStringLiteral(DISKWARDEN_VERSION);
}

void DiskManager::start()
{
    udisks::registerTypes();
    QDBusConnection bus = udisks::bus();
    if (!bus.isConnected()) {
        m_available = false;
        m_errorText = tr("Can't connect to the system D-Bus.");
        emit availableChanged();
        return;
    }
    bus.connect(udisks::Service, QString(), udisks::PropertiesIface, QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
    bus.connect(udisks::Service, udisks::RootPath, udisks::ObjectManagerIface, QStringLiteral("InterfacesAdded"), this,
                SLOT(onObjectsChanged()));
    bus.connect(udisks::Service, udisks::RootPath, udisks::ObjectManagerIface, QStringLiteral("InterfacesRemoved"), this,
                SLOT(onObjectsChanged()));
    updateDetectionInterval();
    m_statsClock.start();
    m_perfTimer.start();
    m_temperatureTimer.start();
    m_volumeTimer.start();
    enumerate();
}

void DiskManager::updateDetectionInterval()
{
    const int interval = std::max(1, AppSettings::instance()->detectionMinutes()) * 60000;
    if (m_detectionTimer.interval() != interval || !m_detectionTimer.isActive())
        m_detectionTimer.start(interval);
}

void DiskManager::onObjectsChanged()
{
    m_enumerateTimer.start();
}

void DiskManager::onPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &,
                                      const QDBusMessage &message)
{
    const QString path = message.path();
    if (iface == udisks::FilesystemIface) {
        // Mounts and unmounts change the volume lists.
        m_enumerateTimer.start();
        return;
    }
    for (Disk *disk : std::as_const(m_disks)) {
        if (disk->drivePath() == path) {
            disk->updateProperties(iface, changed);
            emit statusChanged();
            return;
        }
    }
}

void DiskManager::enumerate()
{
    QDBusMessage message = udisks::methodCall(udisks::RootPath, udisks::ObjectManagerIface, QStringLiteral("GetManagedObjects"));
    auto *watcher = new QDBusPendingCallWatcher(udisks::bus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        QDBusPendingReply<DBusManagedObjects> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            m_available = false;
            m_errorText = tr("udisks2 isn't available: %1").arg(reply.error().message());
            emit availableChanged();
            return;
        }
        if (!m_available) {
            m_available = true;
            m_errorText.clear();
            emit availableChanged();
        }
        applyObjects(reply.value());
    });
}

void DiskManager::applyObjects(const DBusManagedObjects &objects)
{
    Disk *current = currentDisk();
    QSet<QString> drivesWithBlocks;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const QVariantMap block = it.value().value(udisks::BlockIface);
        if (!block.isEmpty())
            drivesWithBlocks.insert(udisks::objectPath(block.value(QStringLiteral("Drive"))));
    }
    QList<Disk *> found;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const QVariantMap drive = it.value().value(udisks::DriveIface);
        if (drive.isEmpty() || drive.value(QStringLiteral("Size")).toLongLong() <= 0
            || !drivesWithBlocks.contains(it.key().path()))
            continue;
        if (drive.value(QStringLiteral("Optical")).toBool()
            || drive.value(QStringLiteral("MediaCompatibility")).toStringList().join(QLatin1Char(' ')).contains(QLatin1String("optical")))
            continue;
        const QString path = it.key().path();
        Disk *disk = nullptr;
        for (Disk *d : std::as_const(m_disks)) {
            if (d->drivePath() == path)
                disk = d;
        }
        const bool isNew = !disk;
        if (isNew)
            disk = new Disk(path, this);
        disk->update(objects);
        if (disk->blockPath().isEmpty()) {
            if (isNew)
                delete disk;
            continue;
        }
        if (isNew) {
            connect(disk, &Disk::refreshed, this, [this] {
                if (m_pendingRefreshes > 0 && --m_pendingRefreshes == 0) {
                    m_lastUpdated = QDateTime::currentDateTime();
                    emit lastUpdatedChanged();
                    emit refreshingChanged();
                }
                emit statusChanged();
            });
            connect(disk, &Disk::statusChanged, this, &DiskManager::statusChanged);
            connect(disk, &Disk::temperatureChanged, this, &DiskManager::statusChanged);
            connect(disk, &Disk::volumesChanged, this, &DiskManager::volumesChanged);
        }
        found << disk;
    }

    QCollator collator;
    collator.setNumericMode(true);
    std::sort(found.begin(), found.end(), [&collator](Disk *a, Disk *b) {
        return collator.compare(a->blockName(), b->blockName()) < 0;
    });

    QList<Disk *> removed;
    for (Disk *d : std::as_const(m_disks)) {
        if (!found.contains(d))
            removed << d;
    }
    QList<Disk *> added;
    for (Disk *d : std::as_const(found)) {
        if (!m_disks.contains(d))
            added << d;
    }

    m_disks = found;
    for (int i = 0; i < m_disks.size(); ++i)
        m_disks[i]->setIndex(i);
    emit disksChanged();

    int index = current ? int(m_disks.indexOf(current)) : -1;
    if (index < 0)
        index = std::clamp(m_currentIndex, 0, std::max(0, int(m_disks.size()) - 1));
    m_currentIndex = index;
    emit currentChanged();
    emit volumesChanged();

    for (Disk *d : std::as_const(removed)) {
        if (SurfaceTest *test = m_surfaceTests.take(d))
            test->stop();
        d->deleteLater();
    }
    if (m_firstEnumeration) {
        m_firstEnumeration = false;
        refresh();
    } else {
        for (Disk *d : std::as_const(added))
            d->refresh();
    }
}

void DiskManager::refresh()
{
    if (m_disks.isEmpty()) {
        m_lastUpdated = QDateTime::currentDateTime();
        emit lastUpdatedChanged();
        return;
    }
    const bool wasRefreshing = refreshing();
    m_pendingRefreshes = int(m_disks.size());
    if (!wasRefreshing)
        emit refreshingChanged();
    for (Disk *disk : std::as_const(m_disks)) {
        if (disk->updating())
            --m_pendingRefreshes;
        else
            disk->refresh();
    }
    if (m_pendingRefreshes <= 0) {
        m_pendingRefreshes = 0;
        emit refreshingChanged();
    }
}

void DiskManager::samplePerformance()
{
    QFile file(QStringLiteral("/proc/diskstats"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QHash<QString, dw::DiskStat> stats = dw::parseDiskStats(file.readAll());
    const double interval = double(m_statsClock.restart());
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (Disk *disk : std::as_const(m_disks)) {
        auto current = stats.constFind(disk->blockName());
        if (current == stats.cend())
            continue;
        auto previous = m_lastStats.constFind(disk->blockName());
        if (previous != m_lastStats.cend())
            disk->addPerfSample(dw::diskRates(*previous, *current, interval), now, interval);
    }
    m_lastStats = stats;
}

QVariantList DiskManager::volumes() const
{
    QVariantList list;
    for (Disk *disk : m_disks)
        list += disk->volumes();
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("name")).toString() < b.toMap().value(QStringLiteral("name")).toString();
    });
    return list;
}

void DiskManager::setCurrentIndex(int index)
{
    if (index == m_currentIndex || index < 0 || index >= m_disks.size())
        return;
    m_currentIndex = index;
    emit currentChanged();
}

Disk *DiskManager::currentDisk() const
{
    return m_currentIndex >= 0 && m_currentIndex < m_disks.size() ? m_disks[m_currentIndex] : nullptr;
}

void DiskManager::setTrayAvailable(bool available)
{
    if (m_trayAvailable == available)
        return;
    m_trayAvailable = available;
    emit trayAvailableChanged();
}

int DiskManager::worstLevel() const
{
    int level = -1;
    for (Disk *disk : m_disks)
        level = std::max({level, disk->statusLevel(), disk->temperatureLevel()});
    return level;
}

SurfaceTest *DiskManager::surfaceTest(Disk *disk)
{
    if (!disk)
        return nullptr;
    SurfaceTest *&test = m_surfaceTests[disk];
    if (!test) {
        test = new SurfaceTest(disk, this);
        QQmlEngine::setObjectOwnership(test, QQmlEngine::CppOwnership);
    }
    return test;
}
