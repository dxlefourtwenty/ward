#pragma once
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class NotificationServer : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    explicit NotificationServer(QObject *parent = nullptr);

signals:
    void newNotification(uint id,
                         const QString &appName,
                         const QString &summary,
                         const QString &body,
                         const QString &iconName,
                         int timeoutMs,
                         const QVariantMap &hints);
    void closeRequested(uint id, uint reason);
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &actionKey);

public slots:
    uint Notify(const QString &app_name,
                uint replaces_id,
                const QString &icon,
                const QString &summary,
                const QString &body,
                const QStringList &actions,
                const QVariantMap &hints,
                int timeout);

    QStringList GetCapabilities() const;
    void GetServerInformation(QString &name,
                              QString &vendor,
                              QString &version,
                              QString &specVersion) const;
    void CloseNotification(uint id);
    void handleNotificationClosed(uint id, uint reason);

private:
    uint nextId_ = 1;
};
