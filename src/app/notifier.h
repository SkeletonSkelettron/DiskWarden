#pragma once

#include <QObject>

// Desktop notifications through org.freedesktop.Notifications.
class Notifier : public QObject
{
    Q_OBJECT

public:
    static Notifier *instance();

    // level: 0 information, 1 warning, 2 critical
    void notify(const QString &title, const QString &body, int level);

private:
    using QObject::QObject;
};
