#pragma once

#include <QObject>
#include <QPointer>

class QMenu;
class QQuickWindow;
class QSystemTrayIcon;

// The tray icon: the highest disk temperature on a background colored by
// the worst disk status, with a tooltip listing every disk.
class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QObject *parent = nullptr);
    ~TrayController() override;

    bool available() const;
    void setWindow(QQuickWindow *window);

private:
    void update();
    void toggleWindow();
    void showWindow();

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QPointer<QQuickWindow> m_window;
};
