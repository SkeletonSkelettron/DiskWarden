#pragma once

#include "diskstats.h"
#include "healthcalculator.h"
#include "infomodel.h"
#include "logmodel.h"
#include "smartattribute.h"
#include "smartattributemodel.h"
#include "sysfs.h"
#include "udisks.h"

#include <QDateTime>
#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

struct VolumeInfo {
    QString device;
    QString label;
    QString fsType;
    QStringList mountPoints;
    qint64 size = 0;
    qint64 total = 0;
    qint64 free = -1;
    int partitionNumber = 0;
    QString partitionName;
    QString partitionType;
    bool encrypted = false;
};

// One physical disk: its identity from udisks2, S.M.A.R.T. data, the
// evaluated health, temperature, performance counters and history.
class Disk : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Disks come from DiskManager")

    Q_PROPERTY(int index READ index NOTIFY identityChanged)
    Q_PROPERTY(QString key READ key NOTIFY identityChanged)
    Q_PROPERTY(QString device READ device NOTIFY identityChanged)
    Q_PROPERTY(QString model READ model NOTIFY identityChanged)
    Q_PROPERTY(QString vendor READ vendor NOTIFY identityChanged)
    Q_PROPERTY(QString serial READ serial NOTIFY identityChanged)
    Q_PROPERTY(QString firmware READ firmware NOTIFY identityChanged)
    Q_PROPERTY(qint64 size READ size NOTIFY identityChanged)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY identityChanged)
    Q_PROPERTY(int kind READ kind NOTIFY identityChanged)
    Q_PROPERTY(QString kindText READ kindText NOTIFY identityChanged)
    Q_PROPERTY(QString interfaceText READ interfaceText NOTIFY identityChanged)
    Q_PROPERTY(QString linkText READ linkText NOTIFY identityChanged)
    Q_PROPERTY(bool selfTestSupported READ selfTestSupported NOTIFY identityChanged)
    Q_PROPERTY(bool conveyanceSupported READ conveyanceSupported NOTIFY identityChanged)

    Q_PROPERTY(bool smartAvailable READ smartAvailable NOTIFY statusChanged)
    Q_PROPERTY(int health READ health NOTIFY statusChanged)
    Q_PROPERTY(int performance READ performance NOTIFY statusChanged)
    Q_PROPERTY(QString healthRating READ healthRating NOTIFY statusChanged)
    Q_PROPERTY(QString performanceRating READ performanceRating NOTIFY statusChanged)
    Q_PROPERTY(int statusLevel READ statusLevel NOTIFY statusChanged)
    Q_PROPERTY(QString statusWord READ statusWord NOTIFY statusChanged)
    Q_PROPERTY(QStringList description READ description NOTIFY statusChanged)
    Q_PROPERTY(QString recommendation READ recommendation NOTIFY statusChanged)
    Q_PROPERTY(int descriptionLevel READ descriptionLevel NOTIFY statusChanged)
    Q_PROPERTY(qint64 powerOnHours READ powerOnHours NOTIFY statusChanged)
    Q_PROPERTY(QString powerOnText READ powerOnText NOTIFY statusChanged)
    Q_PROPERTY(QString remainingText READ remainingText NOTIFY statusChanged)
    Q_PROPERTY(qint64 startStopCount READ startStopCount NOTIFY statusChanged)
    Q_PROPERTY(qint64 lifetimeWritten READ lifetimeWritten NOTIFY statusChanged)
    Q_PROPERTY(qint64 lifetimeRead READ lifetimeRead NOTIFY statusChanged)
    Q_PROPERTY(QString lifetimeWritesText READ lifetimeWritesText NOTIFY statusChanged)
    Q_PROPERTY(QString lifetimeReadsText READ lifetimeReadsText NOTIFY statusChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY statusChanged)
    Q_PROPERTY(bool standby READ standby NOTIFY statusChanged)
    Q_PROPERTY(bool updating READ updating NOTIFY updatingChanged)

    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(bool hasTemperature READ hasTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(int temperatureLevel READ temperatureLevel NOTIFY temperatureChanged)
    Q_PROPERTY(int temperatureWarning READ temperatureWarning NOTIFY temperatureChanged)
    Q_PROPERTY(int temperatureCritical READ temperatureCritical NOTIFY temperatureChanged)
    Q_PROPERTY(QVariantMap temperatureStats READ temperatureStats NOTIFY historyChanged)

    Q_PROPERTY(SmartAttributeModel *smartModel READ smartModel CONSTANT)
    Q_PROPERTY(InfoModel *infoModel READ infoModel CONSTANT)
    Q_PROPERTY(LogModel *logModel READ logModel CONSTANT)
    Q_PROPERTY(LogModel *alertModel READ alertModel CONSTANT)

    Q_PROPERTY(QVariantList volumes READ volumes NOTIFY volumesChanged)
    Q_PROPERTY(QString mountSummary READ mountSummary NOTIFY volumesChanged)
    Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY commentChanged)

    Q_PROPERTY(bool selfTestRunning READ selfTestRunning NOTIFY selfTestChanged)
    Q_PROPERTY(int selfTestProgress READ selfTestProgress NOTIFY selfTestChanged)
    Q_PROPERTY(QString selfTestStatus READ selfTestStatus NOTIFY selfTestChanged)
    Q_PROPERTY(QString selfTestType READ selfTestType NOTIFY selfTestChanged)
    Q_PROPERTY(qint64 selfTestElapsed READ selfTestElapsed NOTIFY selfTestChanged)
    Q_PROPERTY(QString selfTestError READ selfTestError NOTIFY selfTestChanged)

    Q_PROPERTY(double readRate READ readRate NOTIFY perfChanged)
    Q_PROPERTY(double writeRate READ writeRate NOTIFY perfChanged)
    Q_PROPERTY(double utilization READ utilization NOTIFY perfChanged)
    Q_PROPERTY(double estimatedMaxRate READ estimatedMaxRate NOTIFY perfChanged)
    Q_PROPERTY(qint64 sessionRead READ sessionRead NOTIFY perfChanged)
    Q_PROPERTY(qint64 sessionWritten READ sessionWritten NOTIFY perfChanged)
    Q_PROPERTY(QList<QPointF> rateSeries READ rateSeries NOTIFY perfChanged)
    Q_PROPERTY(QList<QPointF> rateAverage READ rateAverage NOTIFY perfChanged)
    Q_PROPERTY(QList<QPointF> utilizationSeries READ utilizationSeries NOTIFY perfChanged)
    Q_PROPERTY(QList<QPointF> utilizationAverage READ utilizationAverage NOTIFY perfChanged)

public:
    enum Kind { Unknown = int(dw::DiskKind::Unknown), Hdd = int(dw::DiskKind::Hdd),
                Ssd = int(dw::DiskKind::Ssd), Nvme = int(dw::DiskKind::Nvme) };
    Q_ENUM(Kind)

    explicit Disk(const QString &drivePath, QObject *parent = nullptr);
    ~Disk() override;

    QString drivePath() const { return m_drivePath; }
    QString blockPath() const { return m_blockPath; }
    QString blockName() const { return m_blockName; }
    void setIndex(int index);
    // Takes the drive's properties, block device and volumes from the
    // udisks2 object tree.
    void update(const DBusManagedObjects &objects);
    void updateProperties(const QString &iface, const QVariantMap &changed);
    void addPerfSample(const dw::DiskRates &rates, qint64 nowMs, double intervalMs);
    void pollTemperature();
    void updateVolumeUsage();
    const QList<VolumeInfo> &volumeInfos() const { return m_volumes; }
    const QList<dw::SmartAttribute> &attributes() const { return m_attributes; }
    const dw::NvmeHealth &nvmeHealth() const { return m_nvme; }

    int index() const { return m_index; }
    QString key() const { return m_key; }
    QString device() const { return m_device; }
    QString model() const { return m_model; }
    QString vendor() const { return m_vendor; }
    QString serial() const { return m_serial; }
    QString firmware() const { return m_firmware; }
    qint64 size() const { return m_size; }
    QString sizeText() const;
    int kind() const { return int(m_kind); }
    QString kindText() const;
    QString interfaceText() const;
    QString linkText() const;
    bool selfTestSupported() const { return m_hasAta || m_hasNvme; }
    bool conveyanceSupported() const { return m_hasAta; }

    bool smartAvailable() const;
    int health() const { return m_result.health; }
    int performance() const { return m_result.performance; }
    QString healthRating() const;
    QString performanceRating() const;
    int statusLevel() const;
    QString statusWord() const { return m_result.statusWord; }
    QStringList description() const { return m_result.description; }
    QString recommendation() const { return m_result.recommendation; }
    int descriptionLevel() const { return m_result.level; }
    qint64 powerOnHours() const;
    QString powerOnText() const;
    QString remainingText() const;
    qint64 startStopCount() const;
    qint64 lifetimeWritten() const;
    qint64 lifetimeRead() const;
    QString lifetimeWritesText() const;
    QString lifetimeReadsText() const;
    QDateTime lastUpdated() const { return m_lastUpdated; }
    bool standby() const { return m_standby; }
    bool updating() const { return m_updating; }

    double temperature() const { return m_temperature; }
    bool hasTemperature() const;
    int temperatureLevel() const;
    int temperatureWarning() const;
    int temperatureCritical() const;
    QVariantMap temperatureStats() const { return m_temperatureStats; }

    SmartAttributeModel *smartModel() const { return m_smartModel; }
    InfoModel *infoModel() const { return m_infoModel; }
    LogModel *logModel() const { return m_logModel; }
    LogModel *alertModel() const { return m_alertModel; }

    QVariantList volumes() const;
    QString mountSummary() const;
    QString comment() const { return m_comment; }
    void setComment(const QString &comment);

    bool selfTestRunning() const;
    int selfTestProgress() const;
    QString selfTestStatus() const;
    QString selfTestType() const { return m_selfTestType; }
    qint64 selfTestElapsed() const;
    QString selfTestError() const { return m_selfTestError; }

    double readRate() const { return m_readRate; }
    double writeRate() const { return m_writeRate; }
    double utilization() const { return m_utilization; }
    double estimatedMaxRate() const { return m_estimatedMaxRate; }
    qint64 sessionRead() const { return qint64(m_sessionRead); }
    qint64 sessionWritten() const { return qint64(m_sessionWritten); }
    QList<QPointF> rateSeries() const { return m_rateSeries; }
    QList<QPointF> rateAverage() const { return m_rateAverage; }
    QList<QPointF> utilizationSeries() const { return m_utilSeries; }
    QList<QPointF> utilizationAverage() const { return m_utilAverage; }

    // mode: 0 samples of the last 24 hours, 1 daily average, 2 daily
    // maximum, 3 daily minimum. x is msecs since the epoch.
    Q_INVOKABLE QList<QPointF> temperatureSeries(int mode) const;
    Q_INVOKABLE QList<QPointF> healthSeries(bool performance) const;
    Q_INVOKABLE QList<QPointF> attributeSeries(int id, bool normalized) const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startSelfTest(const QString &type);
    Q_INVOKABLE void abortSelfTest();
    Q_INVOKABLE void setAttributeEnabled(int id, bool enabled);
    Q_INVOKABLE void changeAttributeOffset(int id, int delta);
    Q_INVOKABLE void clearLog();
    Q_INVOKABLE void clearAlerts();
    Q_INVOKABLE QString displayName() const;

    // Adds an entry to the disk's log (kind 0) or alerts (kind 1).
    void addEvent(int kind, int level, const QString &text);

signals:
    void identityChanged();
    void statusChanged();
    void temperatureChanged();
    void volumesChanged();
    void commentChanged();
    void updatingChanged();
    void selfTestChanged();
    void historyChanged();
    void perfChanged();
    void refreshed();

private:
    void fetchAttributes();
    void fetchProperties();
    void finishRefresh();
    void recompute(bool record);
    dw::HealthInput healthInput() const;
    QList<SmartAttributeModel::Row> smartRows() const;
    void updateSmartModel();
    void recordHistory();
    void logChanges();
    void checkAlerts();
    void raiseAlert(const QString &id, int level, const QString &text);
    void rebuildInfo();
    void updateTemperatureStats();
    double readTemperature() const;
    void setUpdating(bool updating);
    void pollSelfTest();
    void updateSelfTestState();
    QString smartInterface() const;

    QString m_drivePath;
    QString m_blockPath;
    QString m_blockName;
    QString m_key;
    int m_index = 0;
    int m_diskId = -1;
    QVariantMap m_driveProps;
    QVariantMap m_ataProps;
    QVariantMap m_nvmeProps;
    QVariantMap m_namespaceProps;
    QVariantMap m_blockProps;
    bool m_hasAta = false;
    bool m_hasNvme = false;
    QString m_device;
    QString m_model;
    QString m_vendor;
    QString m_serial;
    QString m_firmware;
    QString m_wwn;
    qint64 m_size = 0;
    int m_rotationRate = -1;
    dw::DiskKind m_kind = dw::DiskKind::Unknown;
    dw::LinkInfo m_link;
    dw::QueueInfo m_queue;
    QList<VolumeInfo> m_volumes;
    QList<VolumeInfo> m_partitions;

    QList<dw::SmartAttribute> m_attributes;
    dw::NvmeHealth m_nvme;
    dw::HealthResult m_result;
    bool m_smartRead = false;
    bool m_standby = false;
    bool m_updating = false;
    QDateTime m_lastUpdated;
    double m_temperature;
    QVariantMap m_temperatureStats;
    QString m_comment;

    QHash<int, qint64> m_lastRaw;
    bool m_lastRawLoaded = false;
    int m_lastLoggedHealth = -2;
    QHash<QString, int> m_alertLevels;

    QString m_selfTestType;
    QString m_selfTestError;
    QDateTime m_selfTestStarted;
    bool m_selfTestWasRunning = false;
    QTimer m_selfTestTimer;

    double m_readRate = 0;
    double m_writeRate = 0;
    double m_utilization = 0;
    double m_estimatedMaxRate = 0;
    double m_sessionRead = 0;
    double m_sessionWritten = 0;
    QList<QPointF> m_rateSeries;
    QList<QPointF> m_rateAverage;
    QList<QPointF> m_utilSeries;
    QList<QPointF> m_utilAverage;

    SmartAttributeModel *m_smartModel;
    InfoModel *m_infoModel;
    LogModel *m_logModel;
    LogModel *m_alertModel;
};
