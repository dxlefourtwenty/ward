#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>

#include "NotificationCenter.h"
#include "NotificationServer.h"
#include "WardControl.h"

namespace {

constexpr auto kNotificationService = "org.freedesktop.Notifications";
constexpr auto kNotificationPath = "/org/freedesktop/Notifications";
constexpr auto kControlService = "org.dxle.ward";
constexpr auto kControlPath = "/org/dxle/ward";
constexpr auto kControlInterface = "org.dxle.ward.Control";

QString instanceLockPath()
{
    const QString runtimePath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!runtimePath.isEmpty()) {
        return runtimePath + "/ward.lock";
    }

    return QDir::tempPath() + "/ward.lock";
}

int reloadRunningWard(const QDBusConnection &bus)
{
    QDBusInterface interface(kControlService, kControlPath, kControlInterface, bus);
    if (!interface.isValid()) {
        qCritical() << "No running ward instance found.";
        return 1;
    }

    const QDBusReply<void> reply = interface.call("Reload");
    if (!reply.isValid()) {
        qCritical() << "Failed to reload running ward instance:" << reply.error().message();
        return 1;
    }

    return 0;
}

bool registerService(const QDBusConnection &bus,
                     const QString &serviceName,
                     const QString &failureMessage)
{
    const auto registration = bus.interface()->registerService(
        serviceName,
        QDBusConnectionInterface::DontQueueService,
        QDBusConnectionInterface::DontAllowReplacement);

    if (!registration.isValid()) {
        qCritical() << failureMessage << registration.error().message();
        return false;
    }

    if (registration.value() != QDBusConnectionInterface::ServiceRegistered) {
        qCritical() << failureMessage << serviceName;
        return false;
    }

    return true;
}

bool registerObject(QDBusConnection &bus, const QString &path, QObject *object)
{
    if (bus.registerObject(path,
                           object,
                           QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        return true;
    }

    qCritical() << "Failed to register D-Bus object at" << path << ":" << bus.lastError().message();
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription("ward notification daemon");
    parser.addHelpOption();
    QCommandLineOption reloadOption(QStringList() << "reload",
                                    "Reload the running ward instance configuration.");
    parser.addOption(reloadOption);
    parser.process(app);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "Failed to connect to the session D-Bus.";
        return 1;
    }

    if (parser.isSet(reloadOption)) {
        return reloadRunningWard(bus);
    }

    QLockFile instanceLock(instanceLockPath());
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock()) {
        qCritical() << "ward is already running.";
        return 1;
    }

    NotificationCenter center;
    NotificationServer server;
    WardControl control;

    if (!registerService(bus, kControlService, "Failed to acquire ward control service:")) {
        return 1;
    }

    if (!registerService(bus,
                         kNotificationService,
                         "Failed to acquire org.freedesktop.Notifications:")) {
        return 1;
    }

    if (!registerObject(bus, kControlPath, &control)) {
        return 1;
    }

    if (!registerObject(bus, kNotificationPath, &server)) {
        return 1;
    }

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
        &control,
        &WardControl::reloadRequested,
        &center,
        &NotificationCenter::reloadConfiguration);
    QObject::connect(
        &center,
        &NotificationCenter::notificationClosed,
        &server,
        &NotificationServer::handleNotificationClosed);

    return app.exec();
}
