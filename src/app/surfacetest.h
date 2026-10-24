#pragma once

#include <QDateTime>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <atomic>
#include <memory>
#include <thread>

#include "disk.h"

// A non-destructive read test of the whole disk surface. The disk is
// opened read-only through udisks2's Block.OpenForBackup, which asks for
// administrator authentication, and read block by block on a thread.
class SurfaceTest : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by DiskManager.surfaceTest()")

    Q_PROPERTY(Disk *disk READ disk CONSTANT)
    Q_PROPERTY(int state READ state NOTIFY changed)
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY changed)
    Q_PROPERTY(int blockCount READ blockCount NOTIFY changed)
    Q_PROPERTY(qint64 blockSize READ blockSize NOTIFY changed)
    Q_PROPERTY(qint64 totalSectors READ totalSectors NOTIFY changed)
    Q_PROPERTY(int sectorSize READ sectorSize NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(int currentBlock READ currentBlock NOTIFY changed)
    Q_PROPERTY(int goodBlocks READ goodBlocks NOTIFY changed)
    Q_PROPERTY(int damagedBlocks READ damagedBlocks NOTIFY changed)
    Q_PROPERTY(int badBlocks READ badBlocks NOTIFY changed)
    Q_PROPERTY(qint64 badSectors READ badSectors NOTIFY changed)
    Q_PROPERTY(double currentSpeed READ currentSpeed NOTIFY changed)
    Q_PROPERTY(double averageSpeed READ averageSpeed NOTIFY changed)
    Q_PROPERTY(qint64 elapsed READ elapsed NOTIFY changed)
    Q_PROPERTY(qint64 remaining READ remaining NOTIFY changed)
    Q_PROPERTY(QList<QPointF> speedSeries READ speedSeries NOTIFY changed)
    Q_PROPERTY(QList<QPointF> temperatureSeries READ temperatureSeries NOTIFY changed)
    Q_PROPERTY(QVariantList log READ log NOTIFY logChanged)
    Q_PROPERTY(int mapRevision READ mapRevision NOTIFY changed)

public:
    enum State { Idle, Opening, Running, Finished, Stopped, Failed };
    Q_ENUM(State)
    enum Mode { FullRead, QuickRead };
    Q_ENUM(Mode)
    enum BlockState : quint8 { Untested = 0, Good, Damaged, Bad };

    SurfaceTest(Disk *disk, QObject *parent = nullptr);
    ~SurfaceTest() override;

    Disk *disk() const { return m_disk; }
    int state() const { return m_state; }
    QString stateText() const;
    int mode() const { return m_mode; }
    void setMode(int mode);
    int blockCount() const { return m_blockCount; }
    qint64 blockSize() const { return m_blockSize; }
    qint64 totalSectors() const;
    int sectorSize() const { return m_sectorSize; }
    double progress() const;
    int currentBlock() const { return m_currentBlock; }
    int goodBlocks() const { return m_good; }
    int damagedBlocks() const { return m_damaged; }
    int badBlocks() const { return m_bad; }
    qint64 badSectors() const { return m_badSectors; }
    double currentSpeed() const { return m_currentSpeed; }
    double averageSpeed() const;
    qint64 elapsed() const;
    qint64 remaining() const;
    QList<QPointF> speedSeries() const;
    QList<QPointF> temperatureSeries() const { return m_temperatures; }
    QVariantList log() const { return m_log; }
    int mapRevision() const { return m_mapRevision; }

    // Thread-safe access for the surface map.
    quint8 blockState(int index) const;
    float blockSpeed(int index) const;
    float maximumSpeed() const;
    void blockStates(QList<quint8> &states, QList<float> &speeds) const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE QString blockDescription(int index) const;

signals:
    void changed();
    void logChanged();
    void finished();

private:
    void openDevice();
    void run(int fd);
    void addLog(int level, const QString &text);
    void poll();
    void sampleTemperature();
    void setState(State state);

    QPointer<Disk> m_disk;
    int m_state = Idle;
    int m_mode = FullRead;
    qint64 m_totalBytes = 0;
    qint64 m_blockSize = 0;
    int m_blockCount = 0;
    int m_sectorSize = 512;
    QDateTime m_started;
    QDateTime m_ended;
    QVariantList m_log;
    QList<QPointF> m_temperatures;
    QTimer m_pollTimer;
    QTimer m_temperatureTimer;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_done{false};

    // Shared with the worker thread.
    mutable QMutex m_mutex;
    QList<quint8> m_states;
    QList<float> m_speeds; // MB/s per block
    float m_maxSpeed = 0;
    QList<QPointF> m_speedPoints;
    QList<QPair<int, QString>> m_pendingLog;
    std::atomic<int> m_currentBlockAtomic{-1};
    std::atomic<qint64> m_bytesDone{0};
    std::atomic<qint64> m_badSectorsAtomic{0};
    std::atomic<int> m_failed{0};

    // Copies for QML, updated by poll().
    int m_currentBlock = -1;
    int m_good = 0;
    int m_damaged = 0;
    int m_bad = 0;
    qint64 m_badSectors = 0;
    double m_currentSpeed = 0;
    qint64 m_lastBytes = 0;
    QDateTime m_lastPoll;
    int m_mapRevision = 0;
};
