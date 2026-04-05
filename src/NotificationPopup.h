#pragma once

#include <functional>

#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QLabel>
#include <QPixmap>
#include <QMargins>
#include <QPointer>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWidget>

#include "NotificationTypes.h"
#include "WardConfig.h"

#if WARD_HAS_LAYERSHELLQT
namespace LayerShellQt
{
class Window;
}
#endif

class NotificationPopup : public QWidget {
    Q_OBJECT

public:
    explicit NotificationPopup(const NotificationRequest &request,
                               const WardConfig &config,
                               QWidget *parent = nullptr);

    uint id() const;
    QString stackTag() const;
    void updateNotification(const NotificationRequest &request, const WardConfig &config);
    void applyConfig(const WardConfig &config);
    void applyStyleSheet(const QString &styleSheet, const QHash<QString, QString> &styleVariables);
    void showAnimated(const QPoint &targetPosition, int stackOffset);
    void moveAnimated(const QPoint &targetPosition, int stackOffset);
    void dismiss(uint reason, const QString &exitDirection = {}, bool forceSilent = false);
    int popupHeight() const;

signals:
    void dismissed(uint id, uint reason);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildUi();
    void applyCardLayoutStyle();
    void refreshContent();
    void refreshGeometry();
    void syncCardGeometry();
    void invalidateLayout();
    void syncTextWidths();
    int effectiveTextGap() const;
    QSize contentSize() const;
    QSize surfaceSize() const;
    QPoint restingContentPosition() const;
    void restartTimeout();
    int effectiveTimeoutMs() const;
    int effectiveMinimumHeight() const;
    int effectiveMaxIconSize() const;
    QPoint directionalOffset(const QString &direction) const;
    QString effectiveExitDirection(const QString &exitDirectionOverride) const;
    QEasingCurve animationEasing() const;
    QPixmap notificationPixmap() const;
    QString formatNotificationText(const QString &text, const QLabel *label) const;
    void setContentOffset(const QPoint &offset);
    void setContentOpacity(qreal opacity);
    void resetContentState();
    void startContentAnimation(const QPoint &endOffset,
                               int moveDurationMs,
                               qreal endOpacity,
                               int fadeDurationMs,
                               const std::function<void()> &onFinished = {});
    bool usesLayerShellPlacement() const;
    bool supportsOpacityAnimation() const;
    void configureLayerShell(QScreen *screen);
    void applyLayerShellPlacement(int stackOffset, const QPoint &offset = QPoint());
    QMargins layerShellMargins(int stackOffset, const QPoint &offset = QPoint()) const;
    void stopAnimations();
    bool anchorAtTop() const;
    bool anchorAtRight() const;

    NotificationRequest request_;
    WardConfig config_;
    QFrame *card_ = nullptr;
    QVBoxLayout *textLayout_ = nullptr;
    QVBoxLayout *textBlockLayout_ = nullptr;
    QLabel *iconLabel_ = nullptr;
    QLabel *summaryLabel_ = nullptr;
    QLabel *bodyLabel_ = nullptr;
    QGraphicsOpacityEffect *opacityEffect_ = nullptr;
    QTimer timeoutTimer_;
    QPointer<QVariantAnimation> moveAnimation_;
    QPointer<QVariantAnimation> fadeAnimation_;
    QHash<QString, QString> styleVariables_;
    uint pendingCloseReason_ = 0;
    int currentStackOffset_ = 0;
    QSize currentIconSize_;
    QPoint contentOffset_;
#if WARD_HAS_LAYERSHELLQT
    LayerShellQt::Window *layerShellWindow_ = nullptr;
#endif
};
