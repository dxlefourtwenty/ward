#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QApplication>
#include <QDebug>

#include "NotificationCenter.h"
#include "NotificationServer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    NotificationCenter center;
    NotificationServer server;

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "Failed to connect to the session D-Bus.";
        return 1;
    }

    auto registration = bus.interface()->registerService(
        "org.freedesktop.Notifications",
        QDBusConnectionInterface::ReplaceExistingService,
        QDBusConnectionInterface::AllowReplacement);

    if (!registration.isValid() ||
        registration.value() != QDBusConnectionInterface::ServiceRegistered) {
        qCritical() << "Failed to acquire org.freedesktop.Notifications."
                    << bus.interface()->lastError().message();
        return 1;
    }

    bus.registerObject("/org/freedesktop/Notifications",
                       &server,
                       QDBusConnection::ExportAllSlots);

    QObject::connect(
        &server,
        &NotificationServer::newNotification,
        &center,
        &NotificationCenter::showNotification);
    QObject::connect(
        &server,
        &NotificationServer::closeRequested,
        &center,
        &NotificationCenter::closeNotification);
    QObject::connect(
        &center,
        &NotificationCenter::notificationClosed,
        &server,
        &NotificationServer::handleNotificationClosed);

    return app.exec();
}
