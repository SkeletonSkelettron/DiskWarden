#include "surfacetest.h"

#include "disk.h"
#include "formatting.h"
#include "historydb.h"
#include "udisks.h"

#include <QCoreApplication>
#include <QDBusPendingCallWatcher>
#include <QDBusUnixFileDescriptor>
#include <QLocale>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace {

const qint64 kChunk = 4 * 1024 * 1024;
const qint64 kQuickBytes = 8 * 1024 * 1024;
const qint64 kBlockAlign = 64 * 1024;
const int kMaxBlocks = 10000;
const int kMaxErrorLogs = 100;

} // namespace

SurfaceTest::SurfaceTest(Disk *disk, QObject *parent)
    : QObject(parent)
    , m_disk(disk)
{
    m_pollTimer.setInterval(250);
    connect(&m_pollTimer, &QTimer::timeout, this, &SurfaceTest::poll);
    m_temperatureTimer.setInterval(10000);
    connect(&m_temperatureTimer, &QTimer::timeout, this, &SurfaceTest::sampleTemperature);
}

void SurfaceTest::sampleTemperature()
{
    if (!m_disk)
        return;
    m_disk->pollTemperature();
    if (m_disk->hasTemperature())
        m_temperatures << QPointF(double(QDateTime::currentMSecsSinceEpoch()), m_disk->temperature());
}

SurfaceTest::~SurfaceTest()
{
    m_stop = true;
    if (m_thread.joinable())
        m_thread.join();
}

QString SurfaceTest::stateText() const
{
    switch (m_state) {
    case Opening:
        return tr("Waiting for authorization");
    case Running:
        return tr("Test running");
    case Finished:
        return tr("Test completed");
    case Stopped:
        return tr("Test stopped");
    case Failed:
        return tr("Test failed");
    default:
        return tr("Ready");
    }
}

void SurfaceTest::setMode(int mode)
{
    if (mode == m_mode || m_state == Running || m_state == Opening)
        return;
    m_mode = mode;
    emit changed();
}

void SurfaceTest::setState(State state)
{
    m_state = state;
    emit changed();
}

qint64 SurfaceTest::totalSectors() const
{
    return m_sectorSize > 0 ? m_totalBytes / m_sectorSize : 0;
}

double SurfaceTest::progress() const
{
    return m_blockCount > 0 ? double(m_good + m_damaged + m_bad) / m_blockCount : 0.0;
}

double SurfaceTest::averageSpeed() const
{
    const qint64 seconds = elapsed();
    return seconds > 0 ? double(m_bytesDone.load()) / seconds : 0.0;
}

qint64 SurfaceTest::elapsed() const
{
    if (!m_started.isValid())
        return 0;
    return m_started.secsTo(m_ended.isValid() ? m_ended : QDateTime::currentDateTime());
}

qint64 SurfaceTest::remaining() const
{
    const double p = progress();
    if (p <= 0.0 || m_state != Running)
        return 0;
    return qint64(elapsed() * (1.0 - p) / p);
}

QList<QPointF> SurfaceTest::speedSeries() const
{
    QMutexLocker lock(&m_mutex);
    return m_speedPoints;
}

quint8 SurfaceTest::blockState(int index) const
{
    QMutexLocker lock(&m_mutex);
    return index >= 0 && index < m_states.size() ? m_states[index] : quint8(Untested);
}

float SurfaceTest::blockSpeed(int index) const
{
    QMutexLocker lock(&m_mutex);
    return index >= 0 && index < m_speeds.size() ? m_speeds[index] : 0.0f;
}

float SurfaceTest::maximumSpeed() const
{
    QMutexLocker lock(&m_mutex);
    return m_maxSpeed;
}

void SurfaceTest::blockStates(QList<quint8> &states, QList<float> &speeds) const
{
    QMutexLocker lock(&m_mutex);
    states = m_states;
    speeds = m_speeds;
}

QString SurfaceTest::blockDescription(int index) const
{
    if (index < 0 || index >= m_blockCount || m_sectorSize <= 0)
        return {};
    const QLocale locale;
    const qint64 first = index * m_blockSize / m_sectorSize;
    const qint64 last = std::min(m_totalBytes, (index + 1) * m_blockSize) / m_sectorSize - 1;
    QString status;
    switch (blockState(index)) {
    case Good:
        status = tr("good");
        break;
    case Damaged:
        status = tr("very slow");
        break;
    case Bad:
        status = tr("unreadable sectors");
        break;
    default:
        status = tr("not tested");
        break;
    }
    QString text = tr("Block: %1, Sector: %2 - %3, MB: %4")
                       .arg(index)
                       .arg(locale.toString(first), locale.toString(last))
                       .arg(locale.toString(index * m_blockSize / (1024 * 1024)));
    if (blockState(index) != Untested)
        text += QStringLiteral(", %1 MB/s").arg(locale.toString(blockSpeed(index), 'f', 2));
    return text + QStringLiteral(" (%1)").arg(status);
}

void SurfaceTest::addLog(int level, const QString &text)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("time"), QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
    entry.insert(QStringLiteral("text"), text);
    entry.insert(QStringLiteral("level"), level);
    m_log.prepend(entry);
    emit logChanged();
}

void SurfaceTest::start()
{
    if (!m_disk || m_state == Opening || m_state == Running)
        return;
    if (m_thread.joinable())
        m_thread.join();
    m_stop = false;
    m_done = false;
    m_bytesDone = 0;
    m_badSectorsAtomic = 0;
    m_failed = 0;
    m_currentBlockAtomic = -1;
    m_currentBlock = -1;
    m_good = m_damaged = m_bad = 0;
    m_badSectors = 0;
    m_currentSpeed = 0;
    m_lastBytes = 0;
    m_started = QDateTime();
    m_ended = QDateTime();
    m_temperatures.clear();
    m_log.clear();
    emit logChanged();
    setState(Opening);
    addLog(0, tr("Opening %1 for reading").arg(m_disk->device()));
    openDevice();
}

void SurfaceTest::stop()
{
    if (m_state == Running) {
        m_stop = true;
        addLog(1, tr("Stopping the test"));
    }
}

void SurfaceTest::openDevice()
{
    QDBusMessage message = udisks::methodCall(m_disk->blockPath(), udisks::BlockIface, QStringLiteral("OpenForBackup"),
                                              {QVariantMap()});
    // Leave time to type the password into the authentication dialog.
    auto *watcher = new QDBusPendingCallWatcher(udisks::bus().asyncCall(message, 300000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            addLog(2, tr("The disk couldn't be opened: %1").arg(reply.errorMessage()));
            setState(Failed);
            return;
        }
        const QDBusUnixFileDescriptor descriptor = reply.arguments().constFirst().value<QDBusUnixFileDescriptor>();
        const int fd = descriptor.isValid() ? ::dup(descriptor.fileDescriptor()) : -1;
        if (fd < 0) {
            addLog(2, tr("The disk couldn't be opened: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
            setState(Failed);
            return;
        }

        quint64 bytes = 0;
        if (::ioctl(fd, BLKGETSIZE64, &bytes) != 0 || bytes == 0)
            bytes = quint64(m_disk->size());
        int sectorSize = 512;
        if (::ioctl(fd, BLKSSZGET, &sectorSize) != 0 || sectorSize <= 0)
            sectorSize = 512;
        m_totalBytes = qint64(bytes);
        m_sectorSize = sectorSize;
        const int blocks = int(std::clamp<qint64>(m_totalBytes / (1024 * 1024), 1, kMaxBlocks));
        m_blockSize = (m_totalBytes + blocks - 1) / blocks;
        m_blockSize = (m_blockSize + kBlockAlign - 1) / kBlockAlign * kBlockAlign;
        m_blockCount = int((m_totalBytes + m_blockSize - 1) / m_blockSize);
        {
            QMutexLocker lock(&m_mutex);
            m_states = QList<quint8>(m_blockCount, quint8(Untested));
            m_speeds = QList<float>(m_blockCount, 0.0f);
            m_speedPoints.clear();
            m_maxSpeed = 0;
            m_pendingLog.clear();
        }

        m_started = QDateTime::currentDateTime();
        m_lastPoll = m_started;
        setState(Running);
        addLog(0, tr("%1 started: %2 blocks of %3")
                      .arg(m_mode == QuickRead ? tr("Quick read test") : tr("Read test"))
                      .arg(QLocale().toString(m_blockCount), hg::formatSize(m_blockSize)));
        m_pollTimer.start();
        m_temperatureTimer.start();
        sampleTemperature();
        m_thread = std::thread([this, fd] {
            run(fd);
            ::close(fd);
            m_done = true;
        });
    });
}

void SurfaceTest::run(int fd)
{
    void *buffer = nullptr;
    if (posix_memalign(&buffer, 4096, size_t(kChunk)) != 0) {
        QMutexLocker lock(&m_mutex);
        m_pendingLog << qMakePair(2, tr("Out of memory"));
        m_failed = 1;
        return;
    }
    const int flags = ::fcntl(fd, F_GETFL);
    bool direct = ::fcntl(fd, F_SETFL, flags | O_DIRECT) == 0;
    const qint64 piece = std::max<qint64>(4096, m_sectorSize);
    std::vector<float> recent;
    int errorLogs = 0;

    for (int b = 0; b < m_blockCount && !m_stop; ++b) {
        m_currentBlockAtomic = b;
        const qint64 start = qint64(b) * m_blockSize;
        const qint64 end = std::min(start + m_blockSize, m_totalBytes);
        const qint64 readEnd = m_mode == QuickRead ? std::min(end, start + kQuickBytes) : end;
        const auto began = std::chrono::steady_clock::now();
        qint64 badInBlock = 0;
        qint64 pos = start;
        while (pos < readEnd && !m_stop) {
            const qint64 n = std::min(kChunk, readEnd - pos);
            const ssize_t r = ::pread(fd, buffer, size_t(n), pos);
            if (r < 0 && errno == EINTR)
                continue;
            if (r < 0 && errno == EINVAL && direct) {
                // The device doesn't take this alignment for direct I/O.
                ::fcntl(fd, F_SETFL, flags);
                direct = false;
                continue;
            }
            if (r > 0) {
                pos += r;
                m_bytesDone += r;
                continue;
            }
            // Find the unreadable sectors of the chunk piece by piece.
            for (qint64 p = pos; p < pos + n && !m_stop; p += piece) {
                const qint64 len = std::min(piece, pos + n - p);
                ssize_t pr = 0;
                do {
                    pr = ::pread(fd, buffer, size_t(len), p);
                } while (pr < 0 && errno == EINTR);
                if (pr > 0) {
                    m_bytesDone += pr;
                    continue;
                }
                const qint64 sectors = len / m_sectorSize;
                badInBlock += sectors;
                m_badSectorsAtomic += sectors;
                if (errorLogs++ < kMaxErrorLogs) {
                    QMutexLocker lock(&m_mutex);
                    m_pendingLog << qMakePair(2, tr("Read error at sector %1: %2")
                                                    .arg(QLocale().toString(p / m_sectorSize),
                                                         QString::fromLocal8Bit(std::strerror(pr < 0 ? errno : EIO))));
                }
            }
            pos += n;
        }
        if (m_stop && pos < readEnd)
            break;

        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        const float speed = seconds > 0 ? float((readEnd - start) / 1048576.0 / seconds) : 0.0f;
        quint8 state = badInBlock > 0 ? Bad : Good;
        if (state == Good && recent.size() >= 8 && seconds > 0.5) {
            std::vector<float> sorted = recent;
            std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
            if (speed < sorted[sorted.size() / 2] * 0.1f)
                state = Damaged;
        }
        if (state == Good) {
            recent.push_back(speed);
            if (recent.size() > 64)
                recent.erase(recent.begin());
        }
        if (!direct)
            ::posix_fadvise(fd, start, readEnd - start, POSIX_FADV_DONTNEED);

        QMutexLocker lock(&m_mutex);
        m_states[b] = state;
        m_speeds[b] = speed;
        m_maxSpeed = std::max(m_maxSpeed, speed);
        m_speedPoints << QPointF(double(start) / (1024.0 * 1024.0 * 1024.0), double(speed));
    }
    std::free(buffer);
}

void SurfaceTest::poll()
{
    QList<QPair<int, QString>> pending;
    {
        QMutexLocker lock(&m_mutex);
        pending.swap(m_pendingLog);
        m_good = m_damaged = m_bad = 0;
        for (quint8 s : std::as_const(m_states)) {
            if (s == Good)
                ++m_good;
            else if (s == Damaged)
                ++m_damaged;
            else if (s == Bad)
                ++m_bad;
        }
    }
    for (const auto &entry : std::as_const(pending))
        addLog(entry.first, entry.second);

    const QDateTime now = QDateTime::currentDateTime();
    const qint64 bytes = m_bytesDone.load();
    const double seconds = m_lastPoll.msecsTo(now) / 1000.0;
    if (seconds > 0) {
        const double speed = (bytes - m_lastBytes) / seconds;
        m_currentSpeed = m_currentSpeed <= 0 ? speed : m_currentSpeed * 0.7 + speed * 0.3;
    }
    m_lastBytes = bytes;
    m_lastPoll = now;
    m_currentBlock = m_currentBlockAtomic.load();
    m_badSectors = m_badSectorsAtomic.load();
    ++m_mapRevision;

    if (m_done) {
        m_pollTimer.stop();
        m_temperatureTimer.stop();
        if (m_thread.joinable())
            m_thread.join();
        m_ended = now;
        m_currentSpeed = 0;
        m_currentBlock = -1;
        const bool stopped = m_stop.load() && progress() < 1.0;
        const QString summary = tr("%1 good, %2 damaged, %3 bad blocks, %4 unreadable sectors")
                                    .arg(m_good)
                                    .arg(m_damaged)
                                    .arg(m_bad)
                                    .arg(m_badSectors);
        const int level = m_bad > 0 ? 2 : m_damaged > 0 ? 1 : 0;
        if (m_failed) {
            m_state = Failed;
            addLog(2, tr("Test failed"));
        } else if (stopped) {
            m_state = Stopped;
            addLog(1, tr("Test stopped: %1").arg(summary));
        } else {
            m_state = Finished;
            addLog(level, tr("Test completed: %1").arg(summary));
        }
        if (m_disk && !m_failed) {
            m_disk->addEvent(HistoryDb::Log, level,
                             stopped ? tr("Surface test stopped after %1 %: %2").arg(qRound(progress() * 100)).arg(summary)
                                     : tr("Surface test completed: %1").arg(summary));
        }
        emit changed();
        emit finished();
        return;
    }
    emit changed();
}
