#include "NotificationPopup.h"

#include <memory>

#include <QDBusArgument>
#include <QBoxLayout>
#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>
#include <QUrl>
#include <QWindow>
#include <QXmlStreamReader>

#if WARD_HAS_LAYERSHELLQT
#include <LayerShellQt/window.h>
#endif

namespace {

constexpr int absoluteMaxIconSize = 128;

QEasingCurve::Type easingFromName(const QString &name)
{
    if (name == "linear") {
        return QEasingCurve::Linear;
    }

    if (name == "out-quad") {
        return QEasingCurve::OutQuad;
    }

    if (name == "out-quint") {
        return QEasingCurve::OutQuint;
    }

    return QEasingCurve::OutCubic;
}

QString textDecorationStyle(const QString &value)
{
    const QString lowered = value.trimmed().toLower();
    if (lowered == "single" || lowered == "true" || lowered == "yes") {
        return QStringLiteral("text-decoration: underline;");
    }
    if (lowered == "double") {
        return QStringLiteral("text-decoration: underline double;");
    }
    if (lowered == "error") {
        return QStringLiteral("text-decoration: underline wavy;");
    }
    return {};
}

QString sizeStyle(const QString &value)
{
    const QString trimmed = value.trimmed().toLower();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (trimmed == "xx-small" || trimmed == "x-small" || trimmed == "small" ||
        trimmed == "medium" || trimmed == "large" || trimmed == "x-large" ||
        trimmed == "xx-large" || trimmed == "smaller" || trimmed == "larger") {
        return QStringLiteral("font-size: %1;").arg(trimmed);
    }

    bool ok = false;
    const int numericSize = trimmed.toInt(&ok);
    if (!ok) {
        return {};
    }

    if (numericSize > 1024) {
        return QStringLiteral("font-size: %1pt;").arg(numericSize / 1024);
    }

    return QStringLiteral("font-size: %1pt;").arg(numericSize);
}

QString spanStyle(const QXmlStreamAttributes &attributes)
{
    QStringList styles;

    const auto addColorStyle = [&styles, &attributes](const QString &key, const QString &cssKey) {
        if (!attributes.hasAttribute(key)) {
            return;
        }

        const QColor color(attributes.value(key).toString().trimmed());
        if (color.isValid()) {
            styles.append(QStringLiteral("%1: %2;").arg(cssKey, color.name(QColor::HexArgb)));
        }
    };

    addColorStyle(QStringLiteral("foreground"), QStringLiteral("color"));
    addColorStyle(QStringLiteral("fgcolor"), QStringLiteral("color"));
    addColorStyle(QStringLiteral("color"), QStringLiteral("color"));
    addColorStyle(QStringLiteral("background"), QStringLiteral("background-color"));
    addColorStyle(QStringLiteral("bgcolor"), QStringLiteral("background-color"));

    if (attributes.hasAttribute(QStringLiteral("font_family"))) {
        styles.append(QStringLiteral("font-family: '%1';")
                          .arg(attributes.value(QStringLiteral("font_family")).toString().toHtmlEscaped()));
    } else if (attributes.hasAttribute(QStringLiteral("face"))) {
        styles.append(QStringLiteral("font-family: '%1';")
                          .arg(attributes.value(QStringLiteral("face")).toString().toHtmlEscaped()));
    }

    if (attributes.hasAttribute(QStringLiteral("font_weight"))) {
        styles.append(QStringLiteral("font-weight: %1;")
                          .arg(attributes.value(QStringLiteral("font_weight")).toString().toHtmlEscaped()));
    } else if (attributes.hasAttribute(QStringLiteral("weight"))) {
        styles.append(QStringLiteral("font-weight: %1;")
                          .arg(attributes.value(QStringLiteral("weight")).toString().toHtmlEscaped()));
    }

    if (attributes.hasAttribute(QStringLiteral("font_style"))) {
        styles.append(QStringLiteral("font-style: %1;")
                          .arg(attributes.value(QStringLiteral("font_style")).toString().toHtmlEscaped()));
    } else if (attributes.hasAttribute(QStringLiteral("style"))) {
        styles.append(QStringLiteral("font-style: %1;")
                          .arg(attributes.value(QStringLiteral("style")).toString().toHtmlEscaped()));
    }

    if (attributes.hasAttribute(QStringLiteral("underline"))) {
        const QString underlineStyle = textDecorationStyle(
            attributes.value(QStringLiteral("underline")).toString());
        if (!underlineStyle.isEmpty()) {
            styles.append(underlineStyle);
        }
    }

    if (attributes.hasAttribute(QStringLiteral("strikethrough"))) {
        const QString lowered =
            attributes.value(QStringLiteral("strikethrough")).toString().trimmed().toLower();
        if (lowered == "true" || lowered == "yes" || lowered == "single") {
            styles.append(QStringLiteral("text-decoration: line-through;"));
        }
    }

    if (attributes.hasAttribute(QStringLiteral("size"))) {
        const QString style = sizeStyle(attributes.value(QStringLiteral("size")).toString());
        if (!style.isEmpty()) {
            styles.append(style);
        }
    } else if (attributes.hasAttribute(QStringLiteral("font_size"))) {
        const QString style = sizeStyle(attributes.value(QStringLiteral("font_size")).toString());
        if (!style.isEmpty()) {
            styles.append(style);
        }
    }

    return styles.join(' ');
}

QString richTextFromMarkup(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return {};
    }

    QXmlStreamReader xml(QStringLiteral("<root>%1</root>").arg(text));
    QString html;
    QStringList closingTags;

    while (!xml.atEnd()) {
        switch (xml.readNext()) {
        case QXmlStreamReader::StartElement: {
            const QString name = xml.name().toString().toLower();
            if (name == QStringLiteral("root")) {
                closingTags.prepend({});
                break;
            }

            if (name == QStringLiteral("b") || name == QStringLiteral("big") ||
                name == QStringLiteral("i") || name == QStringLiteral("small") ||
                name == QStringLiteral("sub") || name == QStringLiteral("sup")) {
                html += QStringLiteral("<%1>").arg(name);
                closingTags.prepend(QStringLiteral("</%1>").arg(name));
                break;
            }

            if (name == QStringLiteral("tt")) {
                html += QStringLiteral("<code>");
                closingTags.prepend(QStringLiteral("</code>"));
                break;
            }

            if (name == QStringLiteral("u")) {
                html += QStringLiteral("<span style=\"text-decoration: underline;\">");
                closingTags.prepend(QStringLiteral("</span>"));
                break;
            }

            if (name == QStringLiteral("s") || name == QStringLiteral("strike") ||
                name == QStringLiteral("strikethrough")) {
                html += QStringLiteral("<span style=\"text-decoration: line-through;\">");
                closingTags.prepend(QStringLiteral("</span>"));
                break;
            }

            if (name == QStringLiteral("span")) {
                const QString style = spanStyle(xml.attributes()).toHtmlEscaped();
                if (style.isEmpty()) {
                    html += QStringLiteral("<span>");
                } else {
                    html += QStringLiteral("<span style=\"%1\">").arg(style);
                }
                closingTags.prepend(QStringLiteral("</span>"));
                break;
            }

            if (name == QStringLiteral("a")) {
                const QString href = xml.attributes().value(QStringLiteral("href")).toString().toHtmlEscaped();
                html += QStringLiteral("<a href=\"%1\">").arg(href);
                closingTags.prepend(QStringLiteral("</a>"));
                break;
            }

            if (name == QStringLiteral("br")) {
                html += QStringLiteral("<br/>");
                closingTags.prepend({});
                break;
            }

            closingTags.prepend({});
            break;
        }
        case QXmlStreamReader::EndElement:
            if (!closingTags.isEmpty()) {
                html += closingTags.takeFirst();
            }
            break;
        case QXmlStreamReader::Characters:
            html += xml.text().toString().toHtmlEscaped().replace('\n', QStringLiteral("<br/>"));
            break;
        default:
            break;
        }
    }

    if (xml.hasError()) {
        return text.toHtmlEscaped().replace('\n', QStringLiteral("<br/>"));
    }

    return html;
}

QPixmap loadPixmapFromImageData(const QVariant &value)
{
    if (!value.isValid() || !value.canConvert<QDBusArgument>()) {
        return {};
    }

    const QDBusArgument argument = value.value<QDBusArgument>();
    int imageWidth = 0;
    int imageHeight = 0;
    int rowStride = 0;
    bool hasAlpha = false;
    int bitsPerSample = 0;
    int channels = 0;
    QByteArray imageData;

    argument.beginStructure();
    argument >> imageWidth >> imageHeight >> rowStride >> hasAlpha >> bitsPerSample >> channels >> imageData;
    argument.endStructure();

    if (imageWidth <= 0 || imageHeight <= 0 || rowStride <= 0 || bitsPerSample != 8 || imageData.isEmpty()) {
        return {};
    }

    QImage image;

    if (channels == 4) {
        image = QImage(reinterpret_cast<const uchar *>(imageData.constData()),
                       imageWidth,
                       imageHeight,
                       rowStride,
                       hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGBX8888).copy();
    } else if (channels == 3) {
        image = QImage(reinterpret_cast<const uchar *>(imageData.constData()),
                       imageWidth,
                       imageHeight,
                       rowStride,
                       QImage::Format_RGB888).copy();
    } else if (channels == 1) {
        image = QImage(reinterpret_cast<const uchar *>(imageData.constData()),
                       imageWidth,
                       imageHeight,
                       rowStride,
                       QImage::Format_Grayscale8).copy();
    }

    return image.isNull() ? QPixmap() : QPixmap::fromImage(image);
}

QPixmap pixmapFromPath(const QString &path)
{
    const QUrl url(path);
    if (url.isValid() && url.isLocalFile()) {
        return QPixmap(url.toLocalFile());
    }

    return QPixmap(path);
}

QRect nonTransparentBounds(const QImage &image)
{
    if (image.isNull() || !image.hasAlphaChannel()) {
        return {};
    }

    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) == 0) {
                continue;
            }

            left = qMin(left, x);
            top = qMin(top, y);
            right = qMax(right, x);
            bottom = qMax(bottom, y);
        }
    }

    if (right < left || bottom < top) {
        return {};
    }

    return QRect(QPoint(left, top), QPoint(right, bottom));
}

QPixmap trimmedPixmap(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        return {};
    }

    const QRect bounds = nonTransparentBounds(pixmap.toImage());
    if (bounds.isNull()) {
        return pixmap;
    }

    return pixmap.copy(bounds);
}

QSize pixmapDisplaySize(const QPixmap &pixmap)
{
    return pixmap.isNull() ? QSize() : pixmap.deviceIndependentSize().toSize();
}

} // namespace

NotificationPopup::NotificationPopup(const NotificationRequest &request,
                                     const WardConfig &config,
                                     QWidget *parent)
    : QWidget(parent)
    , request_(request)
    , config_(config)
{
    setObjectName("notificationPopup");
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFocusPolicy(Qt::NoFocus);
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus;

    if (usesLayerShellPlacement()) {
        flags |= Qt::Window;
    } else {
        flags |= Qt::Tool | Qt::WindowStaysOnTopHint;
    }

    setWindowFlags(flags);

    timeoutTimer_.setSingleShot(true);
    connect(&timeoutTimer_, &QTimer::timeout, this, [this]() {
        dismiss(1);
    });

    buildUi();
    refreshContent();
    refreshGeometry();
    restartTimeout();
}

uint NotificationPopup::id() const
{
    return request_.id;
}

void NotificationPopup::updateNotification(const NotificationRequest &request, const WardConfig &config)
{
    stopAnimations();
    request_ = request;
    config_ = config;
    pendingCloseReason_ = 0;
    refreshContent();
    refreshGeometry();
    resetContentState();
    if (supportsOpacityAnimation()) {
        setWindowOpacity(1.0);
    }
    restartTimeout();
}

void NotificationPopup::applyConfig(const WardConfig &config)
{
    config_ = config;
    refreshGeometry();
    restartTimeout();
}

void NotificationPopup::applyStyleSheet(const QString &styleSheet)
{
    setStyleSheet(styleSheet);
    refreshGeometry();
}

void NotificationPopup::showAnimated(const QPoint &targetPosition, int stackOffset)
{
    stopAnimations();
    refreshGeometry();
    currentStackOffset_ = stackOffset;
    timeoutTimer_.stop();
    const bool silentOpen = notificationSilentOpen(request_.hints);

    if (usesLayerShellPlacement()) {
        configureLayerShell(screen());
        applyLayerShellPlacement(stackOffset);

        if (config_.animation.enabled && !silentOpen) {
            setContentOffset(directionalOffset(config_.animation.enterFrom));
            setContentOpacity(0.0);
        } else {
            resetContentState();
        }

        show();
        raise();

        if (!config_.animation.enabled || silentOpen) {
            restartTimeout();
            return;
        }

        startContentAnimation(QPoint(),
                              config_.animation.enterDurationMs,
                              1.0,
                              0,
                              [this]() {
            setContentOffset(QPoint());
            restartTimeout();
        });
        return;
    }

    move(targetPosition);
    show();
    raise();

    if (!config_.animation.enabled || silentOpen) {
        if (supportsOpacityAnimation()) {
            setWindowOpacity(1.0);
        }
        restartTimeout();
        return;
    }

    move(targetPosition + directionalOffset(config_.animation.enterFrom));
    if (supportsOpacityAnimation()) {
        setWindowOpacity(0.0);
    }

    moveAnimation_ = new QPropertyAnimation(this, "pos", this);
    moveAnimation_->setDuration(config_.animation.enterDurationMs);
    moveAnimation_->setStartValue(pos());
    moveAnimation_->setEndValue(targetPosition);
    moveAnimation_->setEasingCurve(animationEasing());
    connect(moveAnimation_, &QPropertyAnimation::finished, this, [this]() {
        restartTimeout();
    });

    moveAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
    if (supportsOpacityAnimation()) {
        fadeAnimation_ = new QPropertyAnimation(this, "windowOpacity", this);
        fadeAnimation_->setDuration(config_.animation.fadeDurationMs);
        fadeAnimation_->setStartValue(0.0);
        fadeAnimation_->setEndValue(1.0);
        fadeAnimation_->setEasingCurve(animationEasing());
        fadeAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void NotificationPopup::moveAnimated(const QPoint &targetPosition, int stackOffset)
{
    currentStackOffset_ = stackOffset;

    if (usesLayerShellPlacement()) {
        configureLayerShell(screen());
        applyLayerShellPlacement(stackOffset);
        return;
    }

    if (!isVisible()) {
        move(targetPosition);
        return;
    }

    if (!config_.animation.enabled) {
        move(targetPosition);
        return;
    }

    if (pos() == targetPosition) {
        return;
    }

    if (moveAnimation_) {
        moveAnimation_->stop();
    }

    moveAnimation_ = new QPropertyAnimation(this, "pos", this);
    moveAnimation_->setDuration(config_.animation.moveDurationMs);
    moveAnimation_->setStartValue(pos());
    moveAnimation_->setEndValue(targetPosition);
    moveAnimation_->setEasingCurve(animationEasing());
    moveAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
}

void NotificationPopup::dismiss(uint reason, const QString &exitDirection, bool forceSilent)
{
    if (pendingCloseReason_ != 0) {
        return;
    }

    pendingCloseReason_ = reason;
    timeoutTimer_.stop();
    stopAnimations();
    const QPoint exitOffset = directionalOffset(effectiveExitDirection(exitDirection));
    const bool silentClose = forceSilent || notificationSilentClose(request_.hints);

    if (usesLayerShellPlacement()) {
        if (!config_.animation.enabled || silentClose) {
            hide();
            resetContentState();
            emit dismissed(request_.id, pendingCloseReason_);
            return;
        }

        startContentAnimation(exitOffset,
                              config_.animation.exitDurationMs,
                              1.0,
                              0,
                              [this]() {
            hide();
            resetContentState();
            emit dismissed(request_.id, pendingCloseReason_);
        });
        return;
    }

    if (!config_.animation.enabled || silentClose) {
        hide();
        emit dismissed(request_.id, pendingCloseReason_);
        return;
    }

    moveAnimation_ = new QPropertyAnimation(this, "pos", this);
    moveAnimation_->setDuration(config_.animation.exitDurationMs);
    moveAnimation_->setStartValue(pos());
    moveAnimation_->setEndValue(pos() + exitOffset);
    moveAnimation_->setEasingCurve(animationEasing());

    auto completionCount = std::make_shared<int>(supportsOpacityAnimation() ? 2 : 1);
    auto finalizeDismissal = [this]() {
        hide();
        resetContentState();
        emit dismissed(request_.id, pendingCloseReason_);
    };
    auto handleAnimationFinished = [completionCount, finalizeDismissal]() {
        *completionCount -= 1;
        if (*completionCount == 0) {
            finalizeDismissal();
        }
    };

    if (supportsOpacityAnimation()) {
        fadeAnimation_ = new QPropertyAnimation(this, "windowOpacity", this);
        fadeAnimation_->setDuration(config_.animation.fadeDurationMs);
        fadeAnimation_->setStartValue(windowOpacity());
        fadeAnimation_->setEndValue(0.0);
        fadeAnimation_->setEasingCurve(animationEasing());
        connect(fadeAnimation_, &QPropertyAnimation::finished, this, handleAnimationFinished);
    } else {
        connect(moveAnimation_, &QPropertyAnimation::finished, this, handleAnimationFinished);
    }
    connect(moveAnimation_, &QPropertyAnimation::finished, this, handleAnimationFinished);

    moveAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
    if (fadeAnimation_) {
        fadeAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

int NotificationPopup::popupHeight() const
{
    return card_ ? card_->height() : height();
}

void NotificationPopup::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    dismiss(2);
}

void NotificationPopup::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncCardGeometry();
}

void NotificationPopup::buildUi()
{
    card_ = new QFrame(this);
    card_->setObjectName("notificationCard");

    auto *cardLayout = new QHBoxLayout(card_);
    cardLayout->setContentsMargins(16, 14, 16, 16);
    cardLayout->setSpacing(12);

    iconLabel_ = new QLabel(card_);
    iconLabel_->setObjectName("iconLabel");
    iconLabel_->setAlignment(Qt::AlignCenter);
    iconLabel_->setFixedSize(QSize(0, 0));
    iconLabel_->setMaximumSize(absoluteMaxIconSize, absoluteMaxIconSize);
    iconLabel_->setScaledContents(false);
    cardLayout->addWidget(iconLabel_, 0, Qt::AlignVCenter);

    auto *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(6);
    cardLayout->addLayout(textLayout, 1);

    summaryLabel_ = new QLabel(card_);
    summaryLabel_->setObjectName("summaryLabel");
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setTextFormat(Qt::RichText);
    summaryLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    textLayout->addWidget(summaryLabel_);

    bodyLabel_ = new QLabel(card_);
    bodyLabel_->setObjectName("bodyLabel");
    bodyLabel_->setWordWrap(true);
    bodyLabel_->setTextFormat(Qt::RichText);
    bodyLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    textLayout->addWidget(bodyLabel_);

    opacityEffect_ = new QGraphicsOpacityEffect(card_);
    opacityEffect_->setOpacity(1.0);
    card_->setGraphicsEffect(opacityEffect_);
    syncCardGeometry();
}

void NotificationPopup::refreshContent()
{
    const QString summaryText = request_.summary.trimmed();
    const QString bodyText = request_.body.trimmed();
    summaryLabel_->setVisible(!summaryText.isEmpty() || bodyText.isEmpty());
    summaryLabel_->setText(formatNotificationText(summaryText.isEmpty()
                               ? QStringLiteral("Notification")
                               : request_.summary));
    bodyLabel_->setVisible(!bodyText.isEmpty());
    bodyLabel_->setText(formatNotificationText(request_.body));

    QPixmap pixmap = trimmedPixmap(notificationPixmap());
    const int maxIconSize = effectiveMaxIconSize();
    if (!pixmap.isNull() &&
        (pixmap.width() > maxIconSize || pixmap.height() > maxIconSize)) {
        pixmap = pixmap.scaled(maxIconSize,
                               maxIconSize,
                               Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    }

    currentIconSize_ = pixmapDisplaySize(pixmap);
    iconLabel_->setMaximumSize(maxIconSize, maxIconSize);
    iconLabel_->setFixedSize(currentIconSize_);
    iconLabel_->setVisible(!pixmap.isNull());
    iconLabel_->setPixmap(pixmap);
    invalidateLayout();
}

void NotificationPopup::refreshGeometry()
{
    invalidateLayout();
    syncTextWidths();
    const QSize popupSize = contentSize();
    const QSize shellSize = surfaceSize();

    card_->setFixedSize(popupSize);
    setFixedSize(shellSize);
    syncCardGeometry();
}

void NotificationPopup::syncCardGeometry()
{
    if (!card_) {
        return;
    }

    card_->move(restingContentPosition() + contentOffset_);
}

void NotificationPopup::invalidateLayout()
{
    if (!card_) {
        return;
    }

    if (QLayout *layout = card_->layout()) {
        layout->invalidate();
        layout->activate();
    }

    iconLabel_->updateGeometry();
    summaryLabel_->updateGeometry();
    bodyLabel_->updateGeometry();
    card_->updateGeometry();
    updateGeometry();
}

void NotificationPopup::syncTextWidths()
{
    if (!card_) {
        return;
    }

    const auto *layout = qobject_cast<QBoxLayout *>(card_->layout());
    if (!layout) {
        return;
    }

    const QMargins margins = layout->contentsMargins();
    int textWidth = config_.layout.width - margins.left() - margins.right();
    if (iconLabel_ && iconLabel_->isVisible()) {
        textWidth -= currentIconSize_.width();
        textWidth -= layout->spacing();
    }

    textWidth = qMax(textWidth, 1);
    summaryLabel_->setFixedWidth(textWidth);
    bodyLabel_->setFixedWidth(textWidth);
}

QSize NotificationPopup::contentSize() const
{
    const int popupWidth = config_.layout.width;
    int measuredHeight = 0;

    if (card_) {
        card_->ensurePolished();
        if (QLayout *layout = card_->layout()) {
            layout->invalidate();
            layout->activate();

            if (layout->hasHeightForWidth()) {
                measuredHeight = qMax(measuredHeight, layout->totalHeightForWidth(popupWidth));
            }

            measuredHeight = qMax(measuredHeight, layout->sizeHint().height());
            measuredHeight = qMax(measuredHeight, layout->minimumSize().height());
        } else {
            measuredHeight = card_->sizeHint().height();
        }
    }

    const int popupHeight = qMax(measuredHeight, effectiveMinimumHeight());
    return QSize(popupWidth, popupHeight);
}

QSize NotificationPopup::surfaceSize() const
{
    const QSize popupSize = contentSize();

    if (!usesLayerShellPlacement()) {
        return popupSize;
    }

    return QSize(popupSize.width() + (anchorAtRight() ? config_.layout.marginRight : config_.layout.marginLeft),
                 popupSize.height() + (anchorAtTop() ? config_.layout.marginTop : config_.layout.marginBottom));
}

QPoint NotificationPopup::restingContentPosition() const
{
    const QSize shellSize = size();
    const QSize popupSize = card_ ? card_->size() : contentSize();
    const int x = anchorAtRight()
        ? shellSize.width() - popupSize.width() - config_.layout.marginRight
        : config_.layout.marginLeft;
    const int y = anchorAtTop()
        ? config_.layout.marginTop
        : shellSize.height() - popupSize.height() - config_.layout.marginBottom;

    return QPoint(x, y);
}

void NotificationPopup::restartTimeout()
{
    timeoutTimer_.stop();

    const int timeoutMs = effectiveTimeoutMs();
    if (timeoutMs <= 0) {
        return;
    }

    timeoutTimer_.start(timeoutMs);
}

int NotificationPopup::effectiveTimeoutMs() const
{
    if (request_.timeoutMs == 0) {
        return 0;
    }

    if (request_.timeoutMs > 0) {
        return request_.timeoutMs;
    }

    if (notificationUrgency(request_.hints) >= 2) {
        return 0;
    }

    return config_.notifications.defaultTimeoutMs;
}

int NotificationPopup::effectiveMinimumHeight() const
{
    int minimumHeight = config_.layout.minimumHeight;
    if (!currentIconSize_.isEmpty()) {
        minimumHeight -= qMax(0, effectiveMaxIconSize() - currentIconSize_.height());
    }

    return qMax(minimumHeight, 1);
}

int NotificationPopup::effectiveMaxIconSize() const
{
    return qBound(0, config_.notifications.maxIconSize, absoluteMaxIconSize);
}

QPoint NotificationPopup::directionalOffset(const QString &direction) const
{
    const int horizontalDistance = width() + config_.animation.slideDistance;
    const int verticalDistance = height() + config_.animation.slideDistance;

    if (direction == "left") {
        return QPoint(-horizontalDistance, 0);
    }

    if (direction == "top" || direction == "up") {
        return QPoint(0, -verticalDistance);
    }

    if (direction == "bottom" || direction == "down") {
        return QPoint(0, verticalDistance);
    }

    return QPoint(horizontalDistance, 0);
}

QString NotificationPopup::effectiveExitDirection(const QString &exitDirectionOverride) const
{
    if (!exitDirectionOverride.trimmed().isEmpty()) {
        return exitDirectionOverride.trimmed().toLower();
    }

    return config_.animation.exitTo;
}

QEasingCurve NotificationPopup::animationEasing() const
{
    return easingFromName(config_.animation.easing);
}

QPixmap NotificationPopup::notificationPixmap() const
{
    const QStringList imageDataKeys = {
        QStringLiteral("image-data"),
        QStringLiteral("image_data"),
        QStringLiteral("icon_data")
    };

    for (const QString &key : imageDataKeys) {
        const QPixmap pixmap = loadPixmapFromImageData(unwrapHintValue(request_.hints.value(key)));
        if (!pixmap.isNull()) {
            return pixmap;
        }
    }

    const QStringList imagePathKeys = {
        QStringLiteral("image-path"),
        QStringLiteral("image_path")
    };

    for (const QString &key : imagePathKeys) {
        const QString path = unwrapHintValue(request_.hints.value(key)).toString().trimmed();
        if (path.isEmpty()) {
            continue;
        }

        const QPixmap pixmap = pixmapFromPath(path);
        if (!pixmap.isNull()) {
            return pixmap;
        }
    }

    if (!request_.iconName.trimmed().isEmpty()) {
        const QPixmap filePixmap = pixmapFromPath(request_.iconName.trimmed());
        if (!filePixmap.isNull()) {
            return filePixmap;
        }

        const QIcon icon = QIcon::fromTheme(request_.iconName.trimmed());
        const int maxIconSize = effectiveMaxIconSize();
        const QSize desiredSize(maxIconSize, maxIconSize);
        const QPixmap themePixmap = icon.pixmap(desiredSize);
        if (!themePixmap.isNull()) {
            return themePixmap;
        }
    }

    return {};
}

QString NotificationPopup::formatNotificationText(const QString &text) const
{
    return richTextFromMarkup(text);
}

void NotificationPopup::setContentOffset(const QPoint &offset)
{
    contentOffset_ = offset;
    syncCardGeometry();
}

void NotificationPopup::setContentOpacity(qreal opacity)
{
    if (!opacityEffect_) {
        return;
    }

    opacityEffect_->setOpacity(opacity);
}

void NotificationPopup::resetContentState()
{
    setContentOffset(QPoint());
    setContentOpacity(1.0);
}

void NotificationPopup::startContentAnimation(const QPoint &endOffset,
                                              int moveDurationMs,
                                              qreal endOpacity,
                                              int fadeDurationMs,
                                              const std::function<void()> &onFinished)
{
    auto completionCount = std::make_shared<int>(0);
    const auto handleAnimationFinished = [completionCount, onFinished]() {
        *completionCount -= 1;
        if (*completionCount == 0 && onFinished) {
            onFinished();
        }
    };

    if (contentOffset_ != endOffset && moveDurationMs > 0) {
        *completionCount += 1;
        moveAnimation_ = new QVariantAnimation(this);
        moveAnimation_->setDuration(moveDurationMs);
        moveAnimation_->setStartValue(contentOffset_);
        moveAnimation_->setEndValue(endOffset);
        moveAnimation_->setEasingCurve(animationEasing());
        connect(moveAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            setContentOffset(value.toPoint());
        });
        connect(moveAnimation_, &QPropertyAnimation::finished, this, handleAnimationFinished);
        moveAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        setContentOffset(endOffset);
    }

    if (opacityEffect_ && fadeDurationMs > 0 && !qFuzzyCompare(opacityEffect_->opacity(), endOpacity)) {
        *completionCount += 1;
        fadeAnimation_ = new QPropertyAnimation(opacityEffect_, "opacity", this);
        fadeAnimation_->setDuration(fadeDurationMs);
        fadeAnimation_->setStartValue(opacityEffect_->opacity());
        fadeAnimation_->setEndValue(endOpacity);
        fadeAnimation_->setEasingCurve(animationEasing());
        connect(fadeAnimation_, &QPropertyAnimation::finished, this, handleAnimationFinished);
        fadeAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        setContentOpacity(endOpacity);
    }

    if (*completionCount == 0 && onFinished) {
        onFinished();
    }
}

bool NotificationPopup::supportsOpacityAnimation() const
{
    return !QGuiApplication::platformName().startsWith("wayland");
}

bool NotificationPopup::usesLayerShellPlacement() const
{
#if WARD_HAS_LAYERSHELLQT
    return QGuiApplication::platformName().startsWith("wayland");
#else
    return false;
#endif
}

#if WARD_HAS_LAYERSHELLQT
void NotificationPopup::configureLayerShell(QScreen *screen)
{
    if (!usesLayerShellPlacement()) {
        return;
    }

    if (!windowHandle()) {
        winId();
    }

    if (!windowHandle()) {
        return;
    }

    if (!layerShellWindow_) {
        layerShellWindow_ = LayerShellQt::Window::get(windowHandle());
    }

    if (!layerShellWindow_) {
        return;
    }

    LayerShellQt::Window::Anchors anchors;
    anchors |= config_.layout.anchor.startsWith("top")
        ? LayerShellQt::Window::AnchorTop
        : LayerShellQt::Window::AnchorBottom;
    anchors |= config_.layout.anchor.endsWith("right")
        ? LayerShellQt::Window::AnchorRight
        : LayerShellQt::Window::AnchorLeft;

    layerShellWindow_->setAnchors(anchors);
    layerShellWindow_->setLayer(LayerShellQt::Window::LayerOverlay);
    layerShellWindow_->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerShellWindow_->setExclusiveZone(0);
    layerShellWindow_->setActivateOnShow(false);
    layerShellWindow_->setScope(QStringLiteral("ward"));
    layerShellWindow_->setDesiredSize(surfaceSize());

    if (screen) {
        layerShellWindow_->setScreen(screen);
    }
}

void NotificationPopup::applyLayerShellPlacement(int stackOffset, const QPoint &offset)
{
    if (!layerShellWindow_) {
        return;
    }

    layerShellWindow_->setDesiredSize(surfaceSize());
    layerShellWindow_->setMargins(layerShellMargins(stackOffset, offset));
}

QMargins NotificationPopup::layerShellMargins(int stackOffset, const QPoint &offset) const
{
    int left = anchorAtRight() ? 0 : offset.x();
    int top = anchorAtTop() ? stackOffset + offset.y() : 0;
    int right = anchorAtRight() ? -offset.x() : 0;
    int bottom = anchorAtTop() ? 0 : stackOffset - offset.y();

    if (!anchorAtRight()) {
        right = 0;
    }

    if (anchorAtRight()) {
        left = 0;
    }

    if (!anchorAtTop()) {
        top = 0;
    }

    if (anchorAtTop()) {
        bottom = 0;
    }

    return QMargins(left, top, right, bottom);
}
#else
void NotificationPopup::configureLayerShell(QScreen *)
{
}

void NotificationPopup::applyLayerShellPlacement(int, const QPoint &)
{
}

QMargins NotificationPopup::layerShellMargins(int, const QPoint &) const
{
    return {};
}
#endif

void NotificationPopup::stopAnimations()
{
    if (moveAnimation_) {
        moveAnimation_->stop();
        moveAnimation_->deleteLater();
        moveAnimation_ = nullptr;
    }

    if (fadeAnimation_) {
        fadeAnimation_->stop();
        fadeAnimation_->deleteLater();
        fadeAnimation_ = nullptr;
    }
}

bool NotificationPopup::anchorAtTop() const
{
    return config_.layout.anchor.startsWith("top");
}

bool NotificationPopup::anchorAtRight() const
{
    return config_.layout.anchor.endsWith("right");
}
