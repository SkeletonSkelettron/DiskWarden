#include "disk.h"

#include "appsettings.h"
#include "formatting.h"
#include "historydb.h"
#include "infomodel.h"
#include "logmodel.h"
#include "notifier.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QFileInfo>
#include <QLocale>
#include <QStorageInfo>
#include <algorithm>
#include <cmath>
#include <functional>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Disk", text);
}

const int kSeriesLength = 180;
const int kAverageWindow = 10;

template <typename F>
void callAsync(QObject *context, const QDBusMessage &message, F callback, int timeout = 30000)
{
    auto *watcher = new QDBusPendingCallWatcher(udisks::bus().asyncCall(message, timeout), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [watcher, callback] {
        callback(watcher->reply());
        watcher->deleteLater();
    });
}

QVariantMap mapFromReply(const QDBusMessage &reply)
{
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};
    return qdbus_cast<QVariantMap>(reply.arguments().constFirst());
}

QString temperatureText(double t)
{
    return QStringLiteral("%1 °C").arg(qRound(t));
}

QString yesNo(bool value)
{
    return value ? tr("Yes") : tr("No");
}

QString supportedEnabled(const QVariantMap &props, const char *supported, const char *enabled)
{
    if (!props.value(QLatin1String(supported)).toBool())
        return tr("Not supported");
    return props.value(QLatin1String(enabled)).toBool() ? tr("Supported, enabled") : tr("Supported, disabled");
}

QString selfTestTypeName(const QString &type)
{
    if (type == QLatin1String("short"))
        return tr("Short");
    if (type == QLatin1String("extended"))
        return tr("Extended");
    if (type == QLatin1String("conveyance"))
        return tr("Conveyance");
    return type;
}

QString selfTestStatusText(const QString &status)
{
    static const QHash<QString, const char *> texts = {
        {QStringLiteral("success"), "Completed without error"},
        {QStringLiteral("aborted"), "Aborted by the host"},
        {QStringLiteral("interrupted"), "Interrupted by a reset"},
        {QStringLiteral("fatal"), "Fatal error"},
        {QStringLiteral("fatal_error"), "Fatal error"},
        {QStringLiteral("error_unknown"), "Failed with an unknown error"},
        {QStringLiteral("error_electrical"), "Electrical failure"},
        {QStringLiteral("error_servo"), "Servo (seek) failure"},
        {QStringLiteral("error_read"), "Read failure"},
        {QStringLiteral("error_handling"), "Handling damage"},
        {QStringLiteral("known_seg_fail"), "A known test segment failed"},
        {QStringLiteral("unknown_seg_fail"), "An unknown test segment failed"},
        {QStringLiteral("inprogress"), "Running, no problems found"},
        {QStringLiteral("ctrl_reset"), "Interrupted by a controller reset"},
        {QStringLiteral("ns_removed"), "Interrupted: namespace removed"},
        {QStringLiteral("aborted_format"), "Aborted by a format command"},
    };
    if (status.isEmpty())
        return tr("Never run");
    if (const char *text = texts.value(status))
        return tr(text);
    return status;
}

struct LbaFormat {
    int dataSize = 0;
    int metadataSize = 0;
    int performance = 0;
};

LbaFormat parseLbaFormat(const QDBusArgument &arg)
{
    LbaFormat f;
    ushort data = 0;
    ushort meta = 0;
    uchar perf = 0;
    arg.beginStructure();
    arg >> data >> meta >> perf;
    arg.endStructure();
    f.dataSize = data;
    f.metadataSize = meta;
    f.performance = perf;
    return f;
}

QString lbaFormatText(const LbaFormat &f)
{
    static const char *const performance[] = {"Best", "Better", "Good", "Degraded"};
    QString text = QString::number(f.dataSize);
    if (f.metadataSize > 0)
        text += QStringLiteral(" + %1").arg(f.metadataSize);
    return text + QStringLiteral(" (%1)").arg(tr(performance[std::clamp(f.performance, 0, 3)]));
}

} // namespace

Disk::Disk(const QString &drivePath, QObject *parent)
    : QObject(parent)
    , m_drivePath(drivePath)
    , m_temperature(std::numeric_limits<double>::quiet_NaN())
    , m_smartModel(new SmartAttributeModel(this))
    , m_infoModel(new InfoModel(this))
    , m_logModel(new LogModel(this))
    , m_alertModel(new LogModel(this))
{
    m_selfTestTimer.setInterval(5000);
    connect(&m_selfTestTimer, &QTimer::timeout, this, &Disk::pollSelfTest);
    connect(AppSettings::instance(), &AppSettings::changed, this, [this] {
        recompute(false);
        emit temperatureChanged();
    });
    connect(AppSettings::instance(), &AppSettings::diskOptionsChanged, this, [this](const QString &key) {
        if (key == m_key)
            recompute(false);
    });
}

Disk::~Disk() = default;

void Disk::setIndex(int index)
{
    if (m_index == index)
        return;
    m_index = index;
    emit identityChanged();
}

QString Disk::smartInterface() const
{
    return m_hasNvme ? udisks::NvmeControllerIface : udisks::AtaIface;
}

void Disk::update(const DBusManagedObjects &objects)
{
    using namespace udisks;
    const DBusInterfaces drive = objects.value(QDBusObjectPath(m_drivePath));
    m_driveProps = drive.value(DriveIface);
    m_hasAta = drive.contains(AtaIface);
    m_hasNvme = drive.contains(NvmeControllerIface);
    if (m_hasAta)
        m_ataProps = drive.value(AtaIface);
    if (m_hasNvme)
        m_nvmeProps = drive.value(NvmeControllerIface);

    m_model = m_driveProps.value(QStringLiteral("Model")).toString().trimmed();
    m_vendor = m_driveProps.value(QStringLiteral("Vendor")).toString().trimmed();
    m_serial = m_driveProps.value(QStringLiteral("Serial")).toString().trimmed();
    m_firmware = m_driveProps.value(QStringLiteral("Revision")).toString().trimmed();
    m_wwn = m_driveProps.value(QStringLiteral("WWN")).toString().trimmed();
    m_size = m_driveProps.value(QStringLiteral("Size")).toLongLong();
    m_rotationRate = m_driveProps.value(QStringLiteral("RotationRate"), -1).toInt();
    if (m_model.isEmpty())
        m_model = m_vendor.isEmpty() ? tr("Unknown disk") : m_vendor;

    // The whole-disk block device and the volumes on it.
    m_blockPath.clear();
    m_volumes.clear();
    m_partitions.clear();
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const DBusInterfaces &ifaces = it.value();
        if (!ifaces.contains(BlockIface))
            continue;
        const QVariantMap &block = ifaces[BlockIface];
        const QString driveOf = objectPath(block.value(QStringLiteral("Drive")));
        const QString backing = objectPath(block.value(QStringLiteral("CryptoBackingDevice")));
        bool encrypted = false;
        QVariantMap partition = ifaces.value(PartitionIface);
        if (driveOf != m_drivePath) {
            if (backing.isEmpty() || backing == QLatin1String("/"))
                continue;
            auto b = objects.constFind(QDBusObjectPath(backing));
            if (b == objects.cend() || objectPath(b->value(BlockIface).value(QStringLiteral("Drive"))) != m_drivePath)
                continue;
            encrypted = true;
            partition = b->value(PartitionIface);
        }

        if (!encrypted && !ifaces.contains(PartitionIface)) {
            // Prefer the first namespace of an NVMe controller.
            if (m_blockPath.isEmpty() || ifaces.contains(NvmeNamespaceIface)) {
                if (m_blockPath.isEmpty() || it.key().path() < m_blockPath) {
                    m_blockPath = it.key().path();
                    m_blockProps = block;
                    m_namespaceProps = ifaces.value(NvmeNamespaceIface);
                }
            }
        }

        VolumeInfo v;
        v.device = byteString(block.value(QStringLiteral("PreferredDevice")));
        if (v.device.isEmpty())
            v.device = byteString(block.value(QStringLiteral("Device")));
        v.label = block.value(QStringLiteral("IdLabel")).toString();
        v.fsType = block.value(QStringLiteral("IdType")).toString();
        v.size = block.value(QStringLiteral("Size")).toLongLong();
        v.encrypted = encrypted;
        v.partitionNumber = partition.value(QStringLiteral("Number")).toInt();
        v.partitionName = partition.value(QStringLiteral("Name")).toString();
        v.partitionType = partition.value(QStringLiteral("Type")).toString();
        if (ifaces.contains(FilesystemIface)) {
            v.mountPoints = byteStringList(ifaces[FilesystemIface].value(QStringLiteral("MountPoints")));
            std::sort(v.mountPoints.begin(), v.mountPoints.end(),
                      [](const QString &a, const QString &b) { return a.size() < b.size(); });
            m_volumes << v;
        }
        if (!encrypted && ifaces.contains(PartitionIface))
            m_partitions << v;
    }
    std::sort(m_volumes.begin(), m_volumes.end(), [](const VolumeInfo &a, const VolumeInfo &b) { return a.device < b.device; });
    std::sort(m_partitions.begin(), m_partitions.end(),
              [](const VolumeInfo &a, const VolumeInfo &b) { return a.partitionNumber < b.partitionNumber; });

    const QString device = byteString(m_blockProps.value(QStringLiteral("Device")));
    m_device = byteString(m_blockProps.value(QStringLiteral("PreferredDevice")));
    if (m_device.isEmpty())
        m_device = device;
    m_blockName = QFileInfo(device).fileName();
    m_link = dw::readLinkInfo(m_blockName);
    m_queue = dw::readQueueInfo(m_blockName);

    if (m_hasNvme)
        m_kind = dw::DiskKind::Nvme;
    else if (m_rotationRate > 0)
        m_kind = dw::DiskKind::Hdd;
    else if (m_rotationRate == 0)
        m_kind = dw::DiskKind::Ssd;
    else
        m_kind = m_queue.rotational ? dw::DiskKind::Hdd : dw::DiskKind::Ssd;

    m_key = m_driveProps.value(QStringLiteral("Id")).toString();
    if (m_key.isEmpty())
        m_key = m_model + QLatin1Char('-') + m_serial;

    if (m_diskId < 0) {
        HistoryDb &db = HistoryDb::instance();
        m_diskId = db.diskId(m_key, m_model, m_serial);
        m_comment = db.comment(m_diskId);
        m_logModel->setEntries(db.events(m_diskId, HistoryDb::Log));
        m_alertModel->setEntries(db.events(m_diskId, HistoryDb::Alert));
        updateTemperatureStats();
        emit commentChanged();
    }

    double t = readTemperature();
    if (!std::isnan(t))
        m_temperature = t;

    updateVolumeUsage();
    emit identityChanged();
    emit temperatureChanged();
    recompute(false);
}

void Disk::updateProperties(const QString &iface, const QVariantMap &changed)
{
    QVariantMap *props = nullptr;
    if (iface == udisks::AtaIface)
        props = &m_ataProps;
    else if (iface == udisks::NvmeControllerIface)
        props = &m_nvmeProps;
    else if (iface == udisks::DriveIface)
        props = &m_driveProps;
    if (!props)
        return;
    for (auto it = changed.cbegin(); it != changed.cend(); ++it)
        props->insert(it.key(), it.value());
    if (changed.contains(QStringLiteral("SmartTemperature"))) {
        double t = readTemperature();
        if (!std::isnan(t) && t != m_temperature) {
            m_temperature = t;
            emit temperatureChanged();
        }
    }
    updateSelfTestState();
}

void Disk::setUpdating(bool updating)
{
    if (m_updating == updating)
        return;
    m_updating = updating;
    emit updatingChanged();
}

void Disk::refresh()
{
    if (m_updating)
        return;
    if (!m_hasAta && !m_hasNvme) {
        finishRefresh();
        return;
    }
    setUpdating(true);
    QVariantMap options;
    if (m_hasAta && !AppSettings::instance()->wakeSleepingDisks())
        options.insert(QStringLiteral("nowakeup"), true);
    callAsync(this, udisks::methodCall(m_drivePath, smartInterface(), QStringLiteral("SmartUpdate"), {options}),
              [this](const QDBusMessage &reply) {
                  if (reply.type() == QDBusMessage::ErrorMessage) {
                      m_standby = reply.errorName().contains(QLatin1String("WouldWakeup"));
                      if (!m_standby)
                          qInfo("SMART update of %s failed: %s", qPrintable(m_device), qPrintable(reply.errorMessage()));
                  } else {
                      m_standby = false;
                  }
                  fetchAttributes();
              },
              120000);
}

void Disk::fetchAttributes()
{
    callAsync(this, udisks::methodCall(m_drivePath, smartInterface(), QStringLiteral("SmartGetAttributes"), {QVariantMap()}),
              [this](const QDBusMessage &reply) {
                  // NVMe attributes are a map, ATA ones an array of structures.
                  QVariantMap attributes;
                  if (reply.type() == QDBusMessage::ReplyMessage) {
                      if (m_hasNvme) {
                          attributes = mapFromReply(reply);
                          m_nvme = udisks::parseNvmeHealth(attributes, m_nvmeProps);
                          m_smartRead = m_nvme.valid;
                      } else {
                          m_attributes = udisks::parseAtaAttributes(reply);
                          m_smartRead = !m_attributes.isEmpty();
                      }
                  } else {
                      qInfo("Reading SMART attributes of %s failed: %s", qPrintable(m_device), qPrintable(reply.errorMessage()));
                  }
                  callAsync(this, udisks::methodCall(m_drivePath, udisks::PropertiesIface, QStringLiteral("GetAll"), {smartInterface()}),
                            [this, attributes](const QDBusMessage &props) {
                                if (props.type() == QDBusMessage::ReplyMessage) {
                                    if (m_hasNvme) {
                                        m_nvmeProps = mapFromReply(props);
                                        if (!attributes.isEmpty())
                                            m_nvme = udisks::parseNvmeHealth(attributes, m_nvmeProps);
                                    } else {
                                        m_ataProps = mapFromReply(props);
                                    }
                                }
                                finishRefresh();
                            });
              },
              60000);
}

void Disk::fetchProperties()
{
    callAsync(this, udisks::methodCall(m_drivePath, udisks::PropertiesIface, QStringLiteral("GetAll"), {smartInterface()}),
              [this](const QDBusMessage &reply) {
                  if (reply.type() == QDBusMessage::ReplyMessage)
                      updateProperties(smartInterface(), mapFromReply(reply));
              });
}

void Disk::finishRefresh()
{
    m_lastUpdated = QDateTime::currentDateTime();
    double t = readTemperature();
    if (!std::isnan(t))
        m_temperature = t;
    setUpdating(false);
    recompute(true);
    updateSelfTestState();
    emit temperatureChanged();
    emit refreshed();
}

bool Disk::smartAvailable() const
{
    if (!m_smartRead)
        return false;
    if (m_hasNvme)
        return true;
    return m_hasAta && m_ataProps.value(QStringLiteral("SmartSupported")).toBool()
        && m_ataProps.value(QStringLiteral("SmartEnabled")).toBool();
}

dw::HealthInput Disk::healthInput() const
{
    AppSettings *settings = AppSettings::instance();
    dw::HealthInput in;
    in.kind = m_kind;
    in.model = m_model;
    in.smartAvailable = smartAvailable();
    in.smartFailing = m_hasAta && m_ataProps.value(QStringLiteral("SmartFailing")).toBool();
    in.attributes = m_attributes;
    in.nvme = m_nvme;
    in.temperature = m_temperature;
    in.temperatureWarning = temperatureWarning();
    in.temperatureCritical = temperatureCritical();
    in.powerOnHours = powerOnHours();
    in.linkRatio = m_link.ratio;
    if (m_link.maximumKnown && m_link.ratio < 0.999)
        in.linkDescription = tr("%1 instead of %2").arg(m_link.current, m_link.maximum);
    in.trimSupported = m_queue.discardMaxBytes > 0;
    in.disabledAttributes = settings->disabledAttributes(m_key);
    in.offsets = settings->attributeOffsets(m_key);

    HistoryDb &db = HistoryDb::instance();
    if (m_diskId >= 0) {
        in.hotDays = db.hotDays(m_diskId, in.temperatureWarning);
        if (const dw::SmartAttribute *crc = dw::findAttribute(m_attributes, 199); crc && crc->rawKnown) {
            qint64 past = db.rawValueAt(m_diskId, 199, QDateTime::currentDateTime().addDays(-30));
            if (past >= 0)
                in.recentCrcErrors = std::max<qint64>(0, (crc->raw & 0xFFFFFFFF) - (past & 0xFFFFFFFF));
        }
    }
    return in;
}

QList<SmartAttributeModel::Row> Disk::smartRows() const
{
    QList<SmartAttributeModel::Row> rows;
    if (m_hasNvme) {
        if (!m_nvme.valid)
            return rows;
        const dw::NvmeHealth &n = m_nvme;
        int number = 0;
        auto add = [&](const QString &name, qint64 raw, const QString &unit, int level = 0, int threshold = -1, int value = -1) {
            SmartAttributeModel::Row row;
            row.attribute.id = ++number;
            row.attribute.name = name;
            row.attribute.raw = raw;
            row.attribute.rawKnown = raw >= 0;
            row.attribute.threshold = threshold;
            row.attribute.value = value;
            row.unit = unit;
            row.level = level;
            row.status = level >= 2 ? tr("Failing") : level == 1 ? tr("Warning") : tr("OK");
            rows << row;
        };
        const int warnings = n.criticalWarning & ~dw::NvmeTemperature;
        add(tr("Critical Warning"), n.criticalWarning, QString(), warnings ? 2 : (n.criticalWarning ? 1 : 0));
        add(tr("Composite Temperature"), std::isnan(n.temperature) ? -1 : qRound64(n.temperature), QStringLiteral("°C"),
            !std::isnan(n.temperature) && !std::isnan(n.warningTemperature) && n.temperature >= n.warningTemperature ? 1 : 0,
            std::isnan(n.warningTemperature) ? -1 : qRound(n.warningTemperature));
        add(tr("Available Spare"), n.availableSpare, QStringLiteral("%"),
            n.availableSpare >= 0 && n.availableSpare <= n.spareThreshold ? 2 : 0, n.spareThreshold, n.availableSpare);
        add(tr("Available Spare Threshold"), n.spareThreshold, QStringLiteral("%"));
        add(tr("Percentage Used"), n.percentUsed, QStringLiteral("%"), n.percentUsed >= 100 ? 2 : n.percentUsed >= 90 ? 1 : 0,
            -1, n.percentUsed >= 0 ? std::max(0, 100 - n.percentUsed) : -1);
        add(tr("Data Units Read"), n.bytesRead >= 0 ? n.bytesRead / 512000 : -1,
            tr("units (%1)").arg(dw::formatData(n.bytesRead)));
        add(tr("Data Units Written"), n.bytesWritten >= 0 ? n.bytesWritten / 512000 : -1,
            tr("units (%1)").arg(dw::formatData(n.bytesWritten)));
        add(tr("Controller Busy Time"), n.controllerBusyMinutes, tr("min"));
        add(tr("Power Cycles"), n.powerCycles, QString());
        add(tr("Power On Hours"), n.powerOnHours, tr("h"));
        add(tr("Unsafe Shutdowns"), n.unsafeShutdowns, QString());
        add(tr("Media and Data Integrity Errors"), n.mediaErrors, QString(), n.mediaErrors > 0 ? 1 : 0);
        add(tr("Number of Error Information Log Entries"), n.errorLogEntries, QString());
        add(tr("Warning Composite Temperature Time"), n.warningTemperatureMinutes, tr("min"));
        add(tr("Critical Composite Temperature Time"), n.criticalTemperatureMinutes, tr("min"),
            n.criticalTemperatureMinutes > 0 ? 1 : 0);
        for (int i = 0; i < n.sensors.size(); ++i)
            add(tr("Temperature Sensor %1").arg(i + 1), qRound64(n.sensors[i]), QStringLiteral("°C"));
        return rows;
    }

    const dw::HealthInput input = healthInput();
    for (const dw::SmartAttribute &a : m_attributes) {
        SmartAttributeModel::Row row;
        row.attribute = a;
        if (a.preFailure() && a.failingNow()) {
            row.level = 2;
            row.status = tr("Failing now");
        } else if (a.preFailure() && a.failedInPast()) {
            row.level = 1;
            row.status = tr("Failed in the past");
        } else {
            if (dw::HealthCalculator::attributeDeduction(a.id, dw::HealthCalculator::healthCount(input, a)) > 0)
                row.level = 1;
            row.status = a.threshold <= 0 ? tr("OK (Always passing)") : tr("OK");
        }
        rows << row;
    }
    return rows;
}

void Disk::updateSmartModel()
{
    AppSettings *settings = AppSettings::instance();
    m_smartModel->setRows(smartRows(), m_hasNvme, settings->disabledAttributes(m_key), settings->attributeOffsets(m_key));
}

void Disk::recompute(bool record)
{
    m_result = dw::HealthCalculator::evaluate(healthInput());
    updateSmartModel();
    if (record && smartAvailable() && m_diskId >= 0) {
        logChanges();
        recordHistory();
        updateTemperatureStats();
        checkAlerts();
    } else if (record && hasTemperature() && m_diskId >= 0) {
        HistoryDb::instance().addTemperature(m_diskId, QDateTime::currentDateTime(), m_temperature);
        updateTemperatureStats();
    }
    rebuildInfo();
    emit statusChanged();
}

void Disk::recordHistory()
{
    HistoryDb &db = HistoryDb::instance();
    const QDateTime now = QDateTime::currentDateTime();
    if (hasTemperature())
        db.addTemperature(m_diskId, now, m_temperature);
    db.addHealth(m_diskId, now, m_result.health, m_result.performance);
    if (m_hasNvme) {
        QList<dw::SmartAttribute> rows;
        for (const SmartAttributeModel::Row &row : smartRows())
            rows << row.attribute;
        db.addAttributes(m_diskId, now, rows);
    } else {
        db.addAttributes(m_diskId, now, m_attributes);
    }
}

void Disk::logChanges()
{
    HistoryDb &db = HistoryDb::instance();
    if (!m_lastRawLoaded) {
        m_lastRaw = db.lastRawValues(m_diskId);
        m_lastLoggedHealth = db.lastHealth(m_diskId);
        m_lastRawLoaded = true;
    }

    QList<dw::SmartAttribute> current;
    QList<int> watched;
    if (m_hasNvme) {
        for (const SmartAttributeModel::Row &row : smartRows())
            current << row.attribute;
        watched = {1, 3, 5, 12};
    } else {
        current = m_attributes;
        watched = {5, 10, 187, 188, 196, 197, 198, 199};
    }
    const QLocale locale;
    for (int id : std::as_const(watched)) {
        const dw::SmartAttribute *a = dw::findAttribute(current, id);
        if (!a || !a->rawKnown)
            continue;
        const qint64 previous = m_lastRaw.value(id, -1);
        if (previous >= 0 && previous != a->raw) {
            // A higher count is worse, except for the NVMe available spare.
            const bool worse = (m_hasNvme && id == 3) ? a->raw < previous : a->raw > previous;
            addEvent(HistoryDb::Log, worse ? 1 : 0,
                     tr("#%1 %2 changed from %3 to %4").arg(id).arg(a->name, locale.toString(previous), locale.toString(a->raw)));
        }
        m_lastRaw.insert(id, a->raw);
    }

    if (m_result.health >= 0) {
        if (m_lastLoggedHealth < 0) {
            addEvent(HistoryDb::Log, 0, tr("Monitoring started. Health: %1 %, performance: %2 %")
                                            .arg(m_result.health).arg(m_result.performance));
        } else if (m_lastLoggedHealth != m_result.health) {
            addEvent(HistoryDb::Log, m_result.health < m_lastLoggedHealth ? 1 : 0,
                     tr("Health changed from %1 % to %2 %").arg(m_lastLoggedHealth).arg(m_result.health));
        }
        m_lastLoggedHealth = m_result.health;
    }
}

void Disk::addEvent(int kind, int level, const QString &text)
{
    const QDateTime now = QDateTime::currentDateTime();
    HistoryDb::instance().addEvent(m_diskId, HistoryDb::EventKind(kind), now, level, text);
    HistoryDb::Event event;
    event.time = now;
    event.level = level;
    event.text = text;
    (kind == HistoryDb::Alert ? m_alertModel : m_logModel)->prepend(event);
    if (kind == HistoryDb::Log && level >= 1 && AppSettings::instance()->alertLogEntry()) {
        addEvent(HistoryDb::Alert, level, tr("New log entry: %1").arg(text));
        Notifier::instance()->notify(displayName(), text, level);
    }
}

void Disk::raiseAlert(const QString &id, int level, const QString &text)
{
    const int previous = m_alertLevels.value(id, 0);
    m_alertLevels.insert(id, level);
    if (level > previous) {
        addEvent(HistoryDb::Alert, level, text);
        Notifier::instance()->notify(displayName(), text, level);
    }
}

void Disk::checkAlerts()
{
    AppSettings *settings = AppSettings::instance();

    bool failure = m_hasAta && m_ataProps.value(QStringLiteral("SmartFailing")).toBool();
    for (const dw::SmartAttribute &a : std::as_const(m_attributes))
        failure = failure || (a.preFailure() && a.failingNow() && !settings->disabledAttributes(m_key).contains(a.id));
    if (m_hasNvme)
        failure = failure || (m_nvme.criticalWarning & ~dw::NvmeTemperature);
    if (settings->alertFailure())
        raiseAlert(QStringLiteral("failure"), failure ? 2 : 0, tr("The disk predicts its own failure. Back up your data now."));

    if (settings->alertHealth()) {
        const int level = dw::healthLevel(m_result.health, settings->healthWarning(), settings->healthCritical());
        raiseAlert(QStringLiteral("health"), level,
                   level >= 2 ? tr("The health of the disk is critical: %1 %").arg(m_result.health)
                              : tr("The health of the disk is low: %1 %").arg(m_result.health));
    }

    if (settings->alertTemperature() && hasTemperature()) {
        // Two degrees of hysteresis so a temperature at the limit doesn't
        // alert on every detection.
        const int previous = m_alertLevels.value(QStringLiteral("temperature"), 0);
        int level = temperatureLevel();
        if (level < previous) {
            const int limit = previous >= 2 ? temperatureCritical() : temperatureWarning();
            if (m_temperature > limit - 2)
                level = previous;
        }
        raiseAlert(QStringLiteral("temperature"), level,
                   tr("The temperature is %1 (limit: %2 °C)")
                       .arg(temperatureText(m_temperature))
                       .arg(level >= 2 ? temperatureCritical() : temperatureWarning()));
    }

    if (settings->alertFreeSpace()) {
        for (const VolumeInfo &v : std::as_const(m_volumes)) {
            if (v.total <= 0 || v.free < 0)
                continue;
            const double percent = 100.0 * v.free / v.total;
            raiseAlert(QStringLiteral("space:") + v.device, percent < settings->freeSpacePercent() ? 1 : 0,
                       tr("Low free space on %1: %2 (%3 %)")
                           .arg(v.mountPoints.value(0, v.device), dw::formatSize(v.free))
                           .arg(qRound(percent)));
        }
    }
}

double Disk::readTemperature() const
{
    double t = dw::readHwmonTemperature(m_blockName);
    if (!std::isnan(t))
        return t;
    double kelvin = 0;
    if (m_hasAta)
        kelvin = m_ataProps.value(QStringLiteral("SmartTemperature")).toDouble();
    else if (m_hasNvme)
        kelvin = m_nvmeProps.value(QStringLiteral("SmartTemperature")).toDouble();
    if (kelvin > 200)
        return kelvin - 273.15;
    return std::numeric_limits<double>::quiet_NaN();
}

void Disk::pollTemperature()
{
    double t = dw::readHwmonTemperature(m_blockName);
    if (std::isnan(t) || std::abs(t - m_temperature) < 0.5)
        return;
    m_temperature = t;
    emit temperatureChanged();
}

bool Disk::hasTemperature() const
{
    return !std::isnan(m_temperature);
}

int Disk::temperatureWarning() const
{
    return AppSettings::instance()->temperatureWarning(int(m_kind));
}

int Disk::temperatureCritical() const
{
    return AppSettings::instance()->temperatureCritical(int(m_kind));
}

int Disk::temperatureLevel() const
{
    if (!hasTemperature())
        return 0;
    if (m_temperature >= temperatureCritical())
        return 2;
    if (m_temperature >= temperatureWarning())
        return 1;
    return 0;
}

void Disk::updateTemperatureStats()
{
    if (m_diskId < 0)
        return;
    HistoryDb &db = HistoryDb::instance();
    const QLocale locale;
    QVariantMap m;
    const HistoryDb::TodayTemperature today = db.todayTemperature(m_diskId);
    m.insert(QStringLiteral("hasToday"), today.valid);
    m.insert(QStringLiteral("average"), today.average);
    m.insert(QStringLiteral("maximumToday"), today.maximum);
    m.insert(QStringLiteral("minimumToday"), today.minimum);
    const HistoryDb::Extreme max = db.maximumTemperature(m_diskId);
    const HistoryDb::Extreme min = db.minimumTemperature(m_diskId);
    m.insert(QStringLiteral("hasExtremes"), max.valid);
    m.insert(QStringLiteral("maximumEver"), max.value);
    m.insert(QStringLiteral("maximumEverTime"), locale.toString(max.time, QLocale::ShortFormat));
    m.insert(QStringLiteral("minimumEver"), min.value);
    m.insert(QStringLiteral("minimumEverTime"), locale.toString(min.time, QLocale::ShortFormat));
    m.insert(QStringLiteral("hotDays"), db.hotDays(m_diskId, temperatureWarning()));
    m_temperatureStats = m;
    emit historyChanged();
}

QList<QPointF> Disk::temperatureSeries(int mode) const
{
    if (m_diskId < 0)
        return {};
    HistoryDb &db = HistoryDb::instance();
    const QDateTime now = QDateTime::currentDateTime();
    if (mode == 0)
        return db.temperatures(m_diskId, now.addSecs(-24 * 3600), now);
    QList<QPointF> points;
    for (const HistoryDb::DailyTemperature &d : db.dailyTemperatures(m_diskId, 30)) {
        const double x = QDateTime(d.date, QTime(12, 0)).toMSecsSinceEpoch();
        points << QPointF(x, mode == 1 ? d.average : mode == 2 ? d.maximum : d.minimum);
    }
    return points;
}

QList<QPointF> Disk::healthSeries(bool performance) const
{
    return m_diskId >= 0 ? HistoryDb::instance().healthSeries(m_diskId, performance) : QList<QPointF>();
}

QList<QPointF> Disk::attributeSeries(int id, bool normalized) const
{
    return m_diskId >= 0 ? HistoryDb::instance().attributeSeries(m_diskId, id, normalized) : QList<QPointF>();
}

void Disk::addPerfSample(const dw::DiskRates &rates, qint64 nowMs, double intervalMs)
{
    m_readRate = rates.readBytesPerSecond;
    m_writeRate = rates.writeBytesPerSecond;
    m_utilization = rates.utilization;
    m_sessionRead += rates.readBytesPerSecond * intervalMs / 1000.0;
    m_sessionWritten += rates.writeBytesPerSecond * intervalMs / 1000.0;
    const double total = (m_readRate + m_writeRate) / (1024.0 * 1024.0);
    // Low utilization makes the estimate unreliable.
    if (m_utilization >= 25.0)
        m_estimatedMaxRate = std::max(m_estimatedMaxRate, (m_readRate + m_writeRate) / (m_utilization / 100.0));

    auto push = [nowMs](QList<QPointF> &series, double value) {
        series << QPointF(double(nowMs), value);
        while (series.size() > kSeriesLength)
            series.removeFirst();
    };
    auto average = [](const QList<QPointF> &series) {
        double sum = 0;
        const qsizetype n = std::min<qsizetype>(series.size(), kAverageWindow);
        for (qsizetype i = series.size() - n; i < series.size(); ++i)
            sum += series[i].y();
        return n > 0 ? sum / n : 0.0;
    };
    push(m_rateSeries, total);
    push(m_rateAverage, average(m_rateSeries));
    push(m_utilSeries, m_utilization);
    push(m_utilAverage, average(m_utilSeries));
    emit perfChanged();
}

void Disk::updateVolumeUsage()
{
    for (VolumeInfo &v : m_volumes) {
        if (v.mountPoints.isEmpty()) {
            v.total = v.size;
            v.free = -1;
            continue;
        }
        QStorageInfo info(v.mountPoints.first());
        if (info.isValid() && info.isReady()) {
            v.total = info.bytesTotal();
            v.free = info.bytesAvailable();
        }
    }
    emit volumesChanged();
}

QVariantList Disk::volumes() const
{
    QVariantList list;
    for (const VolumeInfo &v : m_volumes) {
        if (v.mountPoints.isEmpty())
            continue;
        QVariantMap map;
        map.insert(QStringLiteral("name"), v.mountPoints.first());
        map.insert(QStringLiteral("mountPoints"), v.mountPoints.join(QLatin1String(", ")));
        map.insert(QStringLiteral("label"), v.label);
        map.insert(QStringLiteral("fsType"), v.fsType);
        map.insert(QStringLiteral("device"), v.device);
        map.insert(QStringLiteral("total"), v.total);
        map.insert(QStringLiteral("free"), v.free);
        map.insert(QStringLiteral("sizeText"), dw::formatSize(v.total));
        map.insert(QStringLiteral("freeText"), v.free >= 0 ? dw::formatSize(v.free) : QString());
        map.insert(QStringLiteral("usedFraction"), v.total > 0 && v.free >= 0 ? 1.0 - double(v.free) / v.total : 0.0);
        map.insert(QStringLiteral("diskIndex"), m_index);
        list << map;
    }
    return list;
}

QString Disk::mountSummary() const
{
    QStringList parts;
    for (const VolumeInfo &v : m_volumes) {
        if (v.mountPoints.isEmpty())
            continue;
        QString text = v.mountPoints.first();
        if (!v.label.isEmpty())
            text += QStringLiteral(" [%1]").arg(v.label);
        parts << text;
    }
    return parts.join(QLatin1String(", "));
}

void Disk::setComment(const QString &comment)
{
    if (comment == m_comment)
        return;
    m_comment = comment;
    HistoryDb::instance().setComment(m_diskId, comment);
    emit commentChanged();
}

QString Disk::sizeText() const
{
    return dw::formatSize(m_size);
}

QString Disk::kindText() const
{
    switch (m_kind) {
    case dw::DiskKind::Hdd:
        return tr("Hard disk drive (HDD)");
    case dw::DiskKind::Ssd:
        return tr("Solid state drive (SSD)");
    case dw::DiskKind::Nvme:
        return tr("NVMe solid state drive");
    case dw::DiskKind::Unknown:
        break;
    }
    return tr("Disk");
}

QString Disk::interfaceText() const
{
    if (m_driveProps.value(QStringLiteral("ConnectionBus")).toString() == QLatin1String("usb"))
        return QStringLiteral("USB");
    return m_link.interfaceName.isEmpty() ? tr("Unknown") : m_link.interfaceName;
}

QString Disk::linkText() const
{
    return m_link.current;
}

QString Disk::healthRating() const
{
    return dw::ratingWord(m_result.health);
}

QString Disk::performanceRating() const
{
    return dw::ratingWord(m_result.performance);
}

int Disk::statusLevel() const
{
    if (!smartAvailable())
        return -1;
    AppSettings *settings = AppSettings::instance();
    int level = dw::healthLevel(m_result.health, settings->healthWarning(), settings->healthCritical());
    if (m_hasAta && m_ataProps.value(QStringLiteral("SmartFailing")).toBool())
        level = 2;
    return level;
}

qint64 Disk::powerOnHours() const
{
    if (m_hasNvme)
        return m_nvmeProps.value(QStringLiteral("SmartPowerOnHours"), -1).toLongLong();
    if (m_hasAta) {
        qint64 seconds = m_ataProps.value(QStringLiteral("SmartPowerOnSeconds")).toLongLong();
        if (seconds > 0)
            return seconds / 3600;
        if (const dw::SmartAttribute *a = dw::findAttribute(m_attributes, 9); a && a->rawKnown)
            return a->raw;
    }
    return -1;
}

QString Disk::powerOnText() const
{
    return dw::formatPowerOnTime(powerOnHours());
}

QString Disk::remainingText() const
{
    return dw::formatRemainingDays(m_result.remainingDays);
}

qint64 Disk::startStopCount() const
{
    if (m_kind != dw::DiskKind::Hdd)
        return -1;
    const dw::SmartAttribute *a = dw::findAttribute(m_attributes, 4);
    return a && a->rawKnown ? (a->raw & 0xFFFFFFFF) : -1;
}

qint64 Disk::lifetimeWritten() const
{
    if (m_hasNvme)
        return m_nvme.bytesWritten;
    const dw::SmartAttribute *a = dw::findAttribute(m_attributes, 241);
    if (!a || !a->rawKnown || a->raw <= 0)
        return -1;
    // Intel SSDs count in 32 MiB units, the others in sectors.
    if (m_kind == dw::DiskKind::Ssd && m_model.contains(QLatin1String("INTEL"), Qt::CaseInsensitive))
        return a->raw * 32 * 1024 * 1024;
    return a->raw * std::max(512, m_queue.logicalBlockSize);
}

qint64 Disk::lifetimeRead() const
{
    if (m_hasNvme)
        return m_nvme.bytesRead;
    const dw::SmartAttribute *a = dw::findAttribute(m_attributes, 242);
    if (!a || !a->rawKnown || a->raw <= 0)
        return -1;
    if (m_kind == dw::DiskKind::Ssd && m_model.contains(QLatin1String("INTEL"), Qt::CaseInsensitive))
        return a->raw * 32 * 1024 * 1024;
    return a->raw * std::max(512, m_queue.logicalBlockSize);
}

QString Disk::lifetimeWritesText() const
{
    const qint64 bytes = lifetimeWritten();
    return bytes >= 0 ? dw::formatData(bytes) : QString();
}

QString Disk::lifetimeReadsText() const
{
    const qint64 bytes = lifetimeRead();
    return bytes >= 0 ? dw::formatData(bytes) : QString();
}

QString Disk::displayName() const
{
    return tr("Disk %1: %2").arg(m_index).arg(m_model);
}

bool Disk::selfTestRunning() const
{
    const QVariantMap &props = m_hasNvme ? m_nvmeProps : m_ataProps;
    return props.value(QStringLiteral("SmartSelftestStatus")).toString() == QLatin1String("inprogress");
}

int Disk::selfTestProgress() const
{
    const QVariantMap &props = m_hasNvme ? m_nvmeProps : m_ataProps;
    if (!selfTestRunning())
        return m_selfTestStarted.isValid() && !m_selfTestTimer.isActive() ? 100 : 0;
    const int remaining = props.value(QStringLiteral("SmartSelftestPercentRemaining"), -1).toInt();
    return remaining < 0 ? 0 : std::clamp(100 - remaining, 0, 100);
}

QString Disk::selfTestStatus() const
{
    const QVariantMap &props = m_hasNvme ? m_nvmeProps : m_ataProps;
    return selfTestStatusText(props.value(QStringLiteral("SmartSelftestStatus")).toString());
}

qint64 Disk::selfTestElapsed() const
{
    return m_selfTestStarted.isValid() ? m_selfTestStarted.secsTo(QDateTime::currentDateTime()) : 0;
}

void Disk::startSelfTest(const QString &type)
{
    if (!selfTestSupported())
        return;
    m_selfTestError.clear();
    emit selfTestChanged();
    callAsync(this, udisks::methodCall(m_drivePath, smartInterface(), QStringLiteral("SmartSelftestStart"), {type, QVariantMap()}),
              [this, type](const QDBusMessage &reply) {
                  if (reply.type() == QDBusMessage::ErrorMessage) {
                      m_selfTestError = reply.errorMessage();
                      emit selfTestChanged();
                      return;
                  }
                  m_selfTestType = type;
                  m_selfTestStarted = QDateTime::currentDateTime();
                  m_selfTestWasRunning = false;
                  addEvent(HistoryDb::Log, 0, tr("%1 self-test started").arg(selfTestTypeName(type)));
                  m_selfTestTimer.start();
                  pollSelfTest();
                  emit selfTestChanged();
              },
              120000);
}

void Disk::abortSelfTest()
{
    callAsync(this, udisks::methodCall(m_drivePath, smartInterface(), QStringLiteral("SmartSelftestAbort"), {QVariantMap()}),
              [this](const QDBusMessage &reply) {
                  if (reply.type() == QDBusMessage::ErrorMessage) {
                      m_selfTestError = reply.errorMessage();
                      emit selfTestChanged();
                  }
                  pollSelfTest();
              });
}

void Disk::pollSelfTest()
{
    callAsync(this, udisks::methodCall(m_drivePath, smartInterface(), QStringLiteral("SmartUpdate"), {QVariantMap()}),
              [this](const QDBusMessage &) { fetchProperties(); }, 60000);
}

void Disk::updateSelfTestState()
{
    const bool running = selfTestRunning();
    if (running) {
        m_selfTestWasRunning = true;
        if (!m_selfTestTimer.isActive()) {
            // A test started by another program.
            if (!m_selfTestStarted.isValid())
                m_selfTestStarted = QDateTime::currentDateTime();
            m_selfTestTimer.start();
        }
    } else if (m_selfTestTimer.isActive() && (m_selfTestWasRunning || selfTestElapsed() > 60)) {
        m_selfTestTimer.stop();
        m_selfTestWasRunning = false;
        const QVariantMap &props = m_hasNvme ? m_nvmeProps : m_ataProps;
        const QString status = props.value(QStringLiteral("SmartSelftestStatus")).toString();
        const int level = status == QLatin1String("success") ? 0 : status.startsWith(QLatin1String("abort")) ? 1 : 2;
        addEvent(HistoryDb::Log, level, tr("%1 self-test finished: %2").arg(selfTestTypeName(m_selfTestType), selfTestStatusText(status)));
    }
    emit selfTestChanged();
}

void Disk::setAttributeEnabled(int id, bool enabled)
{
    AppSettings::instance()->setAttributeEnabled(m_key, id, enabled);
}

void Disk::changeAttributeOffset(int id, int delta)
{
    AppSettings *settings = AppSettings::instance();
    settings->setAttributeOffset(m_key, id, settings->attributeOffsets(m_key).value(id, 0) + delta);
}

void Disk::clearLog()
{
    HistoryDb::instance().clearEvents(m_diskId, HistoryDb::Log);
    m_logModel->setEntries({});
}

void Disk::clearAlerts()
{
    HistoryDb::instance().clearEvents(m_diskId, HistoryDb::Alert);
    m_alertModel->setEntries({});
}

void Disk::rebuildInfo()
{
    const QLocale locale;
    QList<InfoModel::Row> rows;
    QString section;
    auto add = [&](const QString &name, const QString &value) {
        if (!value.isEmpty())
            rows << InfoModel::Row{section, name, value};
    };

    section = tr("Physical Disk Information");
    add(tr("Disk Number"), QString::number(m_index));
    add(tr("Device"), m_device);
    add(tr("Model"), m_model);
    add(tr("Vendor"), m_vendor);
    add(tr("Firmware Revision"), m_firmware);
    add(tr("Serial Number"), m_serial);
    add(tr("World Wide Name"), m_wwn);
    add(tr("Total Size"), tr("%1 (%2 bytes)").arg(sizeText(), locale.toString(m_size)));
    add(tr("Disk Type"), kindText());
    if (m_rotationRate > 0)
        add(tr("Rotation Speed"), tr("%1 RPM").arg(m_rotationRate));
    else if (m_rotationRate == 0)
        add(tr("Rotation Speed"), tr("Solid state, no rotation"));
    add(tr("Interface"), interfaceText());
    add(tr("Current Transfer Rate"), m_link.current);
    add(tr("Maximum Transfer Rate"), m_link.maximum);
    add(tr("Disk Controller"), m_link.controller);
    add(tr("Controller Driver"), m_link.controllerDriver);
    add(tr("Logical Sector Size"), tr("%1 bytes").arg(m_queue.logicalBlockSize));
    add(tr("Physical Sector Size"), tr("%1 bytes").arg(m_queue.physicalBlockSize));
    add(tr("Removable"), yesNo(m_driveProps.value(QStringLiteral("Removable")).toBool()));

    section = tr("Health");
    add(tr("Health"), m_result.health >= 0 ? QStringLiteral("%1 % (%2)").arg(m_result.health).arg(healthRating()) : tr("Unknown"));
    add(tr("Performance"), m_result.performance >= 0 ? QStringLiteral("%1 % (%2)").arg(m_result.performance).arg(performanceRating()) : tr("Unknown"));
    add(tr("Temperature"), hasTemperature() ? temperatureText(m_temperature) : tr("Unknown"));
    add(tr("Power On Time"), powerOnText());
    add(tr("Estimated Remaining Lifetime"), remainingText());
    add(tr("Lifetime Writes"), lifetimeWritesText());
    add(tr("Lifetime Reads"), lifetimeReadsText());
    if (m_hasAta)
        add(tr("S.M.A.R.T. Self-Assessment"), m_ataProps.value(QStringLiteral("SmartFailing")).toBool() ? tr("FAILING") : tr("Passed"));
    add(tr("Last Self-Test"), selfTestStatus());
    if (m_lastUpdated.isValid())
        add(tr("Last Updated"), locale.toString(m_lastUpdated, QLocale::LongFormat));

    if (m_hasAta) {
        section = tr("ATA Features");
        add(tr("S.M.A.R.T."), supportedEnabled(m_ataProps, "SmartSupported", "SmartEnabled"));
        add(tr("Power Management"), supportedEnabled(m_ataProps, "PmSupported", "PmEnabled"));
        add(tr("Advanced Power Management"), supportedEnabled(m_ataProps, "ApmSupported", "ApmEnabled"));
        QString aam = supportedEnabled(m_ataProps, "AamSupported", "AamEnabled");
        const int aamValue = m_ataProps.value(QStringLiteral("AamVendorRecommendedValue")).toInt();
        if (m_ataProps.value(QStringLiteral("AamSupported")).toBool() && aamValue > 0)
            aam += tr(", recommended level %1").arg(aamValue);
        add(tr("Automatic Acoustic Management"), aam);
        add(tr("Write Cache"), supportedEnabled(m_ataProps, "WriteCacheSupported", "WriteCacheEnabled"));
        add(tr("Read Look-Ahead"), supportedEnabled(m_ataProps, "ReadLookaheadSupported", "ReadLookaheadEnabled"));
        add(tr("Native Command Queuing"), m_queue.queueDepth > 1 ? tr("Supported, queue depth %1").arg(m_queue.queueDepth) : tr("Not used"));
        add(tr("TRIM (Discard)"), m_queue.discardMaxBytes > 0 ? tr("Supported") : tr("Not supported"));
        const int erase = m_ataProps.value(QStringLiteral("SecurityEraseUnitMinutes")).toInt();
        const int enhanced = m_ataProps.value(QStringLiteral("SecurityEnhancedEraseUnitMinutes")).toInt();
        if (erase > 0)
            add(tr("Security Erase Time"), tr("%1 minutes").arg(erase));
        if (enhanced > 0)
            add(tr("Enhanced Security Erase Time"), tr("%1 minutes").arg(enhanced));
        add(tr("Security Frozen"), yesNo(m_ataProps.value(QStringLiteral("SecurityFrozen")).toBool()));
        add(tr("Bad Sectors Reported"), locale.toString(m_ataProps.value(QStringLiteral("SmartNumBadSectors")).toLongLong()));
        add(tr("Attributes Failing Now"), QString::number(m_ataProps.value(QStringLiteral("SmartNumAttributesFailing")).toInt()));
        add(tr("Attributes Failed in the Past"), QString::number(m_ataProps.value(QStringLiteral("SmartNumAttributesFailedInThePast")).toInt()));
    }

    if (m_hasNvme) {
        section = tr("NVMe Controller");
        add(tr("NVMe Version"), m_nvmeProps.value(QStringLiteral("NVMeRevision")).toString());
        add(tr("Controller ID"), QString::number(m_nvmeProps.value(QStringLiteral("ControllerID")).toInt()));
        add(tr("Subsystem NQN"), udisks::byteString(m_nvmeProps.value(QStringLiteral("SubsystemNQN"))));
        add(tr("FRU GUID"), m_nvmeProps.value(QStringLiteral("FGUID")).toString());
        add(tr("Controller State"), m_nvmeProps.value(QStringLiteral("State")).toString());
        const qint64 unallocated = m_nvmeProps.value(QStringLiteral("UnallocatedCapacity")).toLongLong();
        if (unallocated > 0)
            add(tr("Unallocated Capacity"), dw::formatSize(unallocated));
        if (!std::isnan(m_nvme.warningTemperature))
            add(tr("Warning Composite Temperature Threshold"), temperatureText(m_nvme.warningTemperature));
        if (!std::isnan(m_nvme.criticalTemperature))
            add(tr("Critical Composite Temperature Threshold"), temperatureText(m_nvme.criticalTemperature));
        for (int i = 0; i < m_nvme.sensors.size(); ++i)
            add(tr("Temperature Sensor %1").arg(i + 1), temperatureText(m_nvme.sensors[i]));
        add(tr("TRIM (Deallocate)"), m_queue.discardMaxBytes > 0 ? tr("Supported") : tr("Not supported"));

        if (!m_namespaceProps.isEmpty()) {
            section = tr("NVMe Namespace Information");
            const qint64 nsid = m_namespaceProps.value(QStringLiteral("NSID")).toLongLong();
            add(tr("Namespace ID"), QString::number(nsid));
            int lbaSize = 0;
            const QVariant formatted = m_namespaceProps.value(QStringLiteral("FormattedLBASize"));
            if (formatted.userType() == qMetaTypeId<QDBusArgument>()) {
                const LbaFormat f = parseLbaFormat(formatted.value<QDBusArgument>());
                lbaSize = f.dataSize;
                add(tr("Formatted LBA Size"), lbaFormatText(f));
            }
            const QVariant formats = m_namespaceProps.value(QStringLiteral("LBAFormats"));
            if (formats.userType() == qMetaTypeId<QDBusArgument>()) {
                QStringList list;
                const QDBusArgument arg = formats.value<QDBusArgument>();
                arg.beginArray();
                while (!arg.atEnd())
                    list << lbaFormatText(parseLbaFormat(arg));
                arg.endArray();
                add(tr("Supported LBA Formats"), list.join(QLatin1String(", ")));
            }
            auto blocks = [&](const char *key) {
                const qint64 n = m_namespaceProps.value(QLatin1String(key)).toLongLong();
                return lbaSize > 0 ? tr("%1 sectors (%2)").arg(locale.toString(n), dw::formatSize(n * lbaSize))
                                   : locale.toString(n);
            };
            add(tr("Namespace Size"), blocks("NamespaceSize"));
            add(tr("Namespace Capacity"), blocks("NamespaceCapacity"));
            add(tr("Namespace Utilization"), blocks("NamespaceUtilization"));
            add(tr("EUI-64"), m_namespaceProps.value(QStringLiteral("EUI64")).toString());
            add(tr("NGUID"), m_namespaceProps.value(QStringLiteral("NGUID")).toString());
        }
    }

    section = tr("Linux Block Device");
    add(tr("Kernel Name"), m_blockName);
    add(tr("I/O Scheduler"), m_queue.scheduler);
    add(tr("Write Cache Mode"), m_queue.writeCache);
    if (m_queue.queueDepth > 0)
        add(tr("Queue Depth"), QString::number(m_queue.queueDepth));
    add(tr("Discard Maximum"), m_queue.discardMaxBytes > 0 ? dw::formatSize(m_queue.discardMaxBytes) : tr("Not supported"));

    section = tr("Partitions");
    for (const VolumeInfo &p : std::as_const(m_partitions)) {
        QStringList parts;
        parts << dw::formatSize(p.size);
        if (!p.fsType.isEmpty())
            parts << p.fsType;
        if (!p.label.isEmpty())
            parts << QStringLiteral("[%1]").arg(p.label);
        for (const VolumeInfo &v : std::as_const(m_volumes)) {
            if (v.partitionNumber == p.partitionNumber && !v.mountPoints.isEmpty()) {
                parts << tr("mounted at %1").arg(v.mountPoints.join(QLatin1String(", ")));
                break;
            }
        }
        add(tr("Partition %1 (%2)").arg(p.partitionNumber).arg(p.device), parts.join(QLatin1String(", ")));
    }
    if (m_partitions.isEmpty())
        add(tr("Partition Table"), tr("None"));

    m_infoModel->setRows(rows);
}
