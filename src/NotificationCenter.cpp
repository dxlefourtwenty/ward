#include "NotificationCenter.h"

#include <QApplication>
#include <QCursor>
#include <QScreen>

namespace {

QScreen *screenByName(const QString &name)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return nullptr;
    }

    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen && screen->name().compare(trimmedName, Qt::CaseInsensitive) == 0) {
            return screen;
        }
    }

    return nullptr;
}

} // namespace

NotificationCenter::NotificationCenter(QObject *parent)
    : QObject(parent)
    , config_(configLoader_.config())
    , styleSheet_(configLoader_.styleSheet())
{
    connect(&configLoader_, &WardConfigLoader::configChanged, this, &NotificationCenter::applyConfig);
    connect(&configLoader_, &WardConfigLoader::styleChanged, this, &NotificationCenter::applyStyle);
}

void NotificationCenter::showNotification(uint id,
                                          const QString &appName,
                                          const QString &summary,
                                          const QString &body,
                                          const QString &iconName,
                                          int timeoutMs,
                                          const QVariantMap &hints)
{
    NotificationRequest request;
    request.id = id;
    request.appName = appName;
    request.iconName = iconName;
    request.summary = summary;
    request.body = body;
    request.timeoutMs = timeoutMs;
    request.hints = hints;

    if (notificationReplaceLast(request.hints) && !popups_.isEmpty()) {
        NotificationPopup *popup = popups_.first();
        const uint replacedId = popup->id();

        if (replacedId != id) {
            popupById_.remove(replacedId);
            emit notificationClosed(replacedId, 4);
        }

        popup->updateNotification(request, config_);
        popupById_.insert(id, popup);
        popups_.removeAll(popup);
        popups_.prepend(popup);
        relayout();
        return;
    }

    if (popupById_.contains(id)) {
        NotificationPopup *popup = popupById_.value(id);
        popup->updateNotification(request, config_);
        popups_.removeAll(popup);
        popups_.prepend(popup);
        relayout();
        return;
    }

    auto *popup = new NotificationPopup(request, config_);
    popup->applyStyleSheet(styleSheet_);
    connect(popup, &NotificationPopup::dismissed, this, &NotificationCenter::handlePopupDismissed);

    popups_.prepend(popup);
    popupById_.insert(id, popup);

    trimToCapacity();
    relayout(popup);
}

void NotificationCenter::closeNotification(uint id, uint reason)
{
    if (!popupById_.contains(id)) {
        return;
    }

    popupById_.value(id)->dismiss(reason);
}

void NotificationCenter::applyConfig(const WardConfig &config)
{
    config_ = config;

    for (NotificationPopup *popup : popups_) {
        popup->applyConfig(config_);
    }

    trimToCapacity();
    relayout();
}

void NotificationCenter::applyStyle(const QString &styleSheet)
{
    styleSheet_ = styleSheet;

    for (NotificationPopup *popup : popups_) {
        popup->applyStyleSheet(styleSheet_);
    }

    relayout();
}

void NotificationCenter::handlePopupDismissed(uint id, uint reason)
{
    NotificationPopup *popup = popupById_.take(id);
    if (!popup) {
        return;
    }

    popups_.removeAll(popup);
    emit notificationClosed(id, reason);
    popup->deleteLater();
    relayout();
}

QScreen *NotificationCenter::resolveScreen() const
{
    const QString requestedScreen = config_.layout.screen.trimmed();

    if (requestedScreen.compare("primary", Qt::CaseInsensitive) == 0) {
        return QApplication::primaryScreen();
    }

    if (requestedScreen.compare("cursor", Qt::CaseInsensitive) == 0) {
        if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
            return screen;
        }

        return QApplication::primaryScreen();
    }

    if (QScreen *screen = screenByName(requestedScreen)) {
        return screen;
    }

    return QApplication::primaryScreen();
}

QPoint NotificationCenter::targetPosition(int index,
                                          const QSize &popupSize,
                                          const QScreen *screen) const
{
    const QRect available = screen->availableGeometry();
    int x = anchorAtRight()
        ? available.right() - config_.layout.marginRight - popupSize.width() + 1
        : available.left() + config_.layout.marginLeft;

    int y = anchorAtTop()
        ? available.top() + config_.layout.marginTop
        : available.bottom() - config_.layout.marginBottom - popupSize.height() + 1;

    const int stackOffset = stackOffsetForIndex(index);
    y += anchorAtTop() ? stackOffset : -stackOffset;

    return QPoint(x, y);
}

int NotificationCenter::stackOffsetForIndex(int index) const
{
    int offset = 0;

    for (int current = 0; current < index; ++current) {
        const NotificationPopup *popup = popups_.at(current);
        offset += popup->popupHeight() + config_.layout.outerMargin;
    }

    return offset;
}

bool NotificationCenter::anchorAtTop() const
{
    return config_.layout.anchor.startsWith("top");
}

bool NotificationCenter::anchorAtRight() const
{
    return config_.layout.anchor.endsWith("right");
}

void NotificationCenter::relayout(NotificationPopup *newPopup)
{
    QScreen *screen = resolveScreen();
    if (!screen) {
        return;
    }

    for (int index = 0; index < popups_.size(); ++index) {
        NotificationPopup *popup = popups_.at(index);
        popup->setScreen(screen);

        const int stackOffset = stackOffsetForIndex(index);
        const QPoint position = targetPosition(index, popup->size(), screen);
        if (popup == newPopup) {
            popup->showAnimated(position, stackOffset);
            continue;
        }

        popup->moveAnimated(position, stackOffset);
    }
}

void NotificationCenter::trimToCapacity()
{
    while (popups_.size() > maximumNotifications()) {
        NotificationPopup *popup = popups_.last();
        if (!popup) {
            break;
        }

        popups_.removeLast();
        popup->dismiss(4, anchorAtTop() ? QStringLiteral("down") : QStringLiteral("up"));
    }
}

int NotificationCenter::maximumNotifications() const
{
    return qMax(config_.notifications.maxNotifications, 1);
}
