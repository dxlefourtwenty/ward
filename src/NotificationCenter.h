#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include "NotificationPopup.h"
#include "WardConfig.h"

class QScreen;

class NotificationCenter : public QObject {
    Q_OBJECT

public:
    explicit NotificationCenter(QObject *parent = nullptr);

signals:
    void notificationClosed(uint id, uint reason);

public slots:
    void reloadConfiguration();
    void showNotification(uint id,
                          const QString &appName,
                          const QString &summary,
                          const QString &body,
                          const QString &iconName,
                          int timeoutMs,
                          const QVariantMap &hints);
    void closeNotification(uint id, uint reason = 3);

private slots:
    void applyConfig(const WardConfig &config);
    void applyStyle(const QString &styleSheet);
    void handlePopupDismissed(uint id, uint reason);

private:
    QScreen *resolveScreen() const;
    int stackOffsetForIndex(int index) const;
    QPoint targetPosition(int index, const QSize &popupSize, const QScreen *screen) const;
    bool anchorAtTop() const;
    bool anchorAtRight() const;
    void relayout(NotificationPopup *newPopup = nullptr);
    void trimToCapacity();
    int maximumNotifications() const;

    WardConfigLoader configLoader_;
    WardConfig config_;
    QString styleSheet_;
    QList<NotificationPopup *> popups_;
    QHash<uint, NotificationPopup *> popupById_;
};
