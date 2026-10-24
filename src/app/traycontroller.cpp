#include "traycontroller.h"

#include "disk.h"
#include "diskmanager.h"

#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QQuickWindow>
#include <QSystemTrayIcon>
#include <cmath>

TrayController::TrayController(QObject *parent)
    : QObject(parent)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_tray = new QSystemTrayIcon(this);
    m_menu = new QMenu();
    m_menu->addAction(tr("Show DiskWarden"), this, &TrayController::showWindow);
    m_menu->addAction(tr("Refresh"), DiskManager::instance(), &DiskManager::refresh);
    m_menu->addSeparator();
    m_menu->addAction(tr("Configuration..."), this, [this] {
        showWindow();
        DiskManager::instance()->requestConfiguration();
    });
    m_menu->addSeparator();
    m_menu->addAction(tr("Exit"), qApp, &QCoreApplication::quit);
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            toggleWindow();
    });
    DiskManager *manager = DiskManager::instance();
    connect(manager, &DiskManager::statusChanged, this, &TrayController::update);
    connect(manager, &DiskManager::disksChanged, this, &TrayController::update);
    update();
    m_tray->show();
}

TrayController::~TrayController()
{
    delete m_menu;
}

bool TrayController::available() const
{
    return m_tray != nullptr;
}

void TrayController::setWindow(QQuickWindow *window)
{
    m_window = window;
}

void TrayController::showWindow()
{
    if (!m_window)
        return;
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void TrayController::toggleWindow()
{
    if (!m_window)
        return;
    if (m_window->isVisible() && m_window->isActive())
        m_window->hide();
    else
        showWindow();
}

void TrayController::update()
{
    if (!m_tray)
        return;
    DiskManager *manager = DiskManager::instance();
    double hottest = std::numeric_limits<double>::quiet_NaN();
    QStringList lines;
    lines << QStringLiteral("DiskWarden");
    for (Disk *disk : manager->disks()) {
        if (disk->hasTemperature() && (std::isnan(hottest) || disk->temperature() > hottest))
            hottest = disk->temperature();
        QString line = disk->displayName() + QStringLiteral(": ");
        line += disk->hasTemperature() ? QStringLiteral("%1 °C").arg(qRound(disk->temperature())) : QStringLiteral("? °C");
        if (disk->health() >= 0)
            line += QStringLiteral(", %1 %").arg(disk->health());
        lines << line;
    }
    m_tray->setToolTip(lines.join(QLatin1Char('\n')));

    const int level = manager->worstLevel();
    const QColor background = level >= 2 ? QColor(0xd0, 0x30, 0x30)
        : level == 1                     ? QColor(0xe0, 0xa8, 0x00)
        : level == 0                     ? QColor(0x2e, 0x8b, 0x2e)
                                         : QColor(0x80, 0x80, 0x80);
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(background);
    painter.setPen(QPen(background.darker(150), 2));
    painter.drawRoundedRect(QRectF(2, 2, 60, 60), 10, 10);
    QFont font = QApplication::font();
    font.setBold(true);
    font.setPixelSize(std::isnan(hottest) || hottest < 100 ? 38 : 28);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, 64, 64), Qt::AlignCenter,
                     std::isnan(hottest) ? QStringLiteral("?") : QString::number(qRound(hottest)));
    painter.end();
    m_tray->setIcon(QIcon(pixmap));
}
