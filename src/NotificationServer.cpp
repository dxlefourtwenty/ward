#include "NotificationServer.h"

NotificationServer::NotificationServer(QObject *parent)
    : QObject(parent) {}

uint NotificationServer::Notify(const QString &appName,
                                uint replacesId,
                                const QString &icon,
                                const QString &summary,
                                const QString &body,
                                const QStringList &,
                                const QVariantMap &hints,
                                int timeout)
{
    const uint notificationId = replacesId > 0 ? replacesId : nextId_++;

    emit newNotification(notificationId,
                         appName,
                         summary,
                         body,
                         icon,
                         timeout,
                         hints);

    return notificationId;
}

QStringList NotificationServer::GetCapabilities() const
{
    return {
        "body",
        "body-markup",
        "icon-static",
        "persistence"
    };
}

void NotificationServer::GetServerInformation(QString &name,
                                              QString &vendor,
                                              QString &version,
                                              QString &specVersion) const
{
    name = "ward";
    vendor = "dxle";
    version = "0.1.0";
    specVersion = "1.3";
}

void NotificationServer::CloseNotification(uint id)
{
    emit closeRequested(id, 3);
}

void NotificationServer::handleNotificationClosed(uint id, uint reason)
{
    emit NotificationClosed(id, reason);
}
