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

void NotificationCenter::reloadConfiguration()
{
    configLoader_.reload();
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

    if (NotificationPopup *popup = popupForRequest(request)) {
        const uint replacedId = popup->id();
        const QString previousStackTag = popup->stackTag();
        popup->updateNotification(request, config_);
        updatePopupMappings(popup, replacedId, previousStackTag);
        popups_.removeAll(popup);
        popups_.prepend(popup);

        if (replacedId != id) {
            emit notificationClosed(replacedId, 4);
        }

        relayout();
        return;
    }

    auto *popup = new NotificationPopup(request, config_);
    popup->applyStyleSheet(styleSheet_);
    connect(popup, &NotificationPopup::dismissed, this, &NotificationCenter::handlePopupDismissed);

    popups_.prepend(popup);
    updatePopupMappings(popup);

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

    popupByStackTag_.remove(popup->stackTag());
    popups_.removeAll(popup);
    emit notificationClosed(id, reason);
    popup->deleteLater();
    relayout();
}

void NotificationCenter::updatePopupMappings(NotificationPopup *popup,
                                             uint previousId,
                                             const QString &previousStackTag)
{
    if (previousId > 0) {
        popupById_.remove(previousId);
    }

    if (!previousStackTag.isEmpty()) {
        popupByStackTag_.remove(previousStackTag);
    }

    popupById_.insert(popup->id(), popup);

    const QString stackTag = popup->stackTag();
    if (!stackTag.isEmpty()) {
        popupByStackTag_.insert(stackTag, popup);
    }
}

NotificationPopup *NotificationCenter::popupForRequest(const NotificationRequest &request) const
{
    if (popupById_.contains(request.id)) {
        return popupById_.value(request.id);
    }

    const QString stackTag = notificationStackTag(request.hints);
    if (!stackTag.isEmpty() && popupByStackTag_.contains(stackTag)) {
        return popupByStackTag_.value(stackTag);
    }

    if (notificationReplaceLast(request.hints) && !popups_.isEmpty()) {
        return popups_.first();
    }

    return nullptr;
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
