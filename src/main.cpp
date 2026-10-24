#include "appsettings.h"
#include "diskmanager.h"
#include "historydb.h"
#include "traycontroller.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <functional>

namespace {

// The interface follows the classic light Windows look whatever the
// desktop's color scheme is.
QPalette lightPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(0xf0, 0xf0, 0xf0));
    p.setColor(QPalette::WindowText, QColor(0x10, 0x10, 0x10));
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::AlternateBase, QColor(0xf5, 0xf5, 0xf5));
    p.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xe1));
    p.setColor(QPalette::ToolTipText, Qt::black);
    p.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Text, QColor(0x10, 0x10, 0x10));
    p.setColor(QPalette::Button, QColor(0xe9, 0xe9, 0xe9));
    p.setColor(QPalette::ButtonText, QColor(0x10, 0x10, 0x10));
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Light, Qt::white);
    p.setColor(QPalette::Midlight, QColor(0xe3, 0xe3, 0xe3));
    p.setColor(QPalette::Mid, QColor(0xa0, 0xa0, 0xa0));
    p.setColor(QPalette::Dark, QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Shadow, QColor(0x50, 0x50, 0x50));
    p.setColor(QPalette::Highlight, QColor(0x00, 0x78, 0xd7));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(0x00, 0x66, 0xcc));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x90, 0x90, 0x90));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x90, 0x90, 0x90));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x90, 0x90, 0x90));
    return p;
}

// --screenshot: renders the pages of the main window, the configuration
// and the surface test window into PNG files.
void takeScreenshots(QQuickWindow *window, const QString &directory, int disk)
{
    QDir().mkpath(directory);
    if (disk >= 0)
        DiskManager::instance()->setCurrentIndex(disk);
    struct Shot {
        QString name;
        std::function<void()> prepare;
    };
    auto *shots = new QList<Shot>;
    for (int tab = 0; tab < 7; ++tab)
        *shots << Shot{QStringLiteral("page%1").arg(tab), [=] { window->setProperty("currentTab", tab); }};
    *shots << Shot{QStringLiteral("configuration"), [=] { QMetaObject::invokeMethod(window, "openConfiguration", QVariant(2)); }};
    *shots << Shot{QStringLiteral("surface"), [=] {
                       QMetaObject::invokeMethod(window, "closeConfiguration");
                       QMetaObject::invokeMethod(window, "openSurfaceTestOfCurrent");
                   }};
    auto *index = new int(-1);
    auto *timer = new QTimer(window);
    timer->setInterval(800);
    QObject::connect(timer, &QTimer::timeout, window, [=] {
        if (*index >= 0) {
            // The surface test is a window of its own.
            QQuickWindow *target = window;
            if (shots->at(*index).name == QLatin1String("surface")) {
                for (QWindow *w : QGuiApplication::topLevelWindows()) {
                    if (w != window && w->isVisible() && qobject_cast<QQuickWindow *>(w))
                        target = qobject_cast<QQuickWindow *>(w);
                }
            }
            target->grabWindow().save(QStringLiteral("%1/%2.png").arg(directory, shots->at(*index).name));
        }
        if (++*index >= shots->size()) {
            timer->stop();
            qApp->quit();
            return;
        }
        shots->at(*index).prepare();
    });
    timer->start();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("DiskWarden"));
    app.setApplicationName(QStringLiteral("DiskWarden"));
    app.setApplicationVersion(QStringLiteral(DISKWARDEN_VERSION));
    app.setDesktopFileName(QStringLiteral("diskwarden"));
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/DiskWarden/icons/diskwarden.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Disk health and temperature monitor"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption minimized(QStringLiteral("minimized"), QStringLiteral("Start in the system tray."));
    QCommandLineOption screenshot(QStringLiteral("screenshot"), QStringLiteral("Save the pages as images and quit."),
                                  QStringLiteral("directory"));
    QCommandLineOption screenshotDisk(QStringLiteral("disk"), QStringLiteral("Disk shown in screenshots."), QStringLiteral("index"));
    parser.addOptions({minimized, screenshot, screenshotDisk});
    parser.process(app);

    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    app.setPalette(lightPalette());

    HistoryDb::instance().open(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                               + QStringLiteral("/DiskWarden/history.sqlite"));

    DiskManager *manager = DiskManager::instance();
    manager->start();

    TrayController tray;
    manager->setTrayAvailable(tray.available());
    app.setQuitOnLastWindowClosed(!tray.available());

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("DiskWarden", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    tray.setWindow(window);

    if (parser.isSet(screenshot)) {
        window->show();
        const QString directory = parser.value(screenshot);
        const int disk = parser.isSet(screenshotDisk) ? parser.value(screenshotDisk).toInt() : -1;
        // Start once the first detection finished.
        QObject::connect(manager, &DiskManager::lastUpdatedChanged, window,
                         [=] { takeScreenshots(window, directory, disk); }, Qt::SingleShotConnection);
    } else {
        const bool hidden = tray.available() && (parser.isSet(minimized) || AppSettings::instance()->startMinimized());
        if (!hidden)
            window->show();
    }
    return app.exec();
}
