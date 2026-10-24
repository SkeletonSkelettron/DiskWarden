#include "notifier.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QVariantMap>

Notifier *Notifier::instance()
{
    static Notifier *notifier = new Notifier(qApp);
    return notifier;
}

void Notifier::notify(const QString &title, const QString &body, int level)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    QVariantMap hints;
    hints.insert(QStringLiteral("urgency"), QVariant::fromValue<uchar>(uchar(qBound(0, level, 2))));
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("diskwarden"));
    const QString icon = level >= 2 ? QStringLiteral("dialog-error")
        : level == 1                ? QStringLiteral("dialog-warning")
                                    : QStringLiteral("drive-harddisk");
    message << QStringLiteral("DiskWarden") << uint(0) << icon << title << body << QStringList() << hints
            << int(level >= 2 ? 0 : -1);
    QDBusConnection::sessionBus().asyncCall(message);
}
