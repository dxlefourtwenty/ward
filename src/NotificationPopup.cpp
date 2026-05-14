#include "NotificationPopup.h"

#include <memory>

#include <QDBusArgument>
#include <QBoxLayout>
#include <QColor>
#include <QDir>
#include <QFontInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QRegularExpression>
#include <QRegion>
#include <QScreen>
#include <QUrl>
#include <QWindow>
#include <QXmlStreamReader>

#if WARD_HAS_KWINDOWSYSTEM
#include <KWindowEffects>
#endif

#if WARD_HAS_LAYERSHELLQT
#include <LayerShellQt/window.h>
#endif

class TimeoutProgressRing : public QWidget {
public:
    explicit TimeoutProgressRing(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(false);
        setFixedSize(18, 18);
    }

    void setProgress(qreal progress)
    {
        const qreal boundedProgress = qBound<qreal>(0.0, progress, 1.0);
        if (qFuzzyCompare(progress_ + 1.0, boundedProgress + 1.0)) {
            return;
        }

        progress_ = boundedProgress;
        update();
    }

    void setColors(const QColor &activeColor, const QColor &troughColor)
    {
        activeColor_ = activeColor.isValid() ? activeColor : QColor(Qt::white);
        troughColor_ = troughColor.isValid() ? troughColor : QColor(255, 255, 255, 42);
        update();
    }

    void setRingSize(int size)
    {
        const int boundedSize = qBound(8, size, 64);
        if (width() == boundedSize && height() == boundedSize) {
            return;
        }

        setFixedSize(boundedSize, boundedSize);
    }

    void setStrokeWidth(int strokeWidth)
    {
        strokeWidth_ = qBound(1, strokeWidth, qMax(1, qMin(width(), height()) / 3));
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const qreal strokeWidth = static_cast<qreal>(strokeWidth_);
        const QRectF arcRect = rect().adjusted(strokeWidth, strokeWidth, -strokeWidth, -strokeWidth);
        if (arcRect.isEmpty()) {
            return;
        }

        QPen troughPen(troughColor_, strokeWidth, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(troughPen);
        painter.drawEllipse(arcRect);

        if (progress_ <= 0.0) {
            return;
        }

        QPen activePen(activeColor_, strokeWidth, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(activePen);
        painter.drawArc(arcRect, 90 * 16, -static_cast<int>(360 * 16 * progress_));
    }

private:
    qreal progress_ = 1.0;
    int strokeWidth_ = 2;
    QColor activeColor_ = QColor(Qt::white);
    QColor troughColor_ = QColor(255, 255, 255, 42);
};

namespace {

constexpr int absoluteMaxIconSize = 128;
constexpr int defaultCardPaddingTop = 14;
constexpr int defaultCardPaddingRight = 16;
constexpr int defaultCardPaddingBottom = 16;
constexpr int defaultCardPaddingLeft = 16;
constexpr int defaultCardSpacing = 12;
constexpr int textHeightSafetyPadding = 8;
constexpr int textRightCushion = 6;

QString resolveStyleValue(const QString &value,
                          const QHash<QString, QString> &styleVariables,
                          int depth = 0);

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

QString stripEnclosingQuotes(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.size() < 2) {
        return trimmed;
    }

    const QChar first = trimmed.front();
    const QChar last = trimmed.back();
    if ((first == '\'' && last == '\'') || (first == '"' && last == '"')) {
        return trimmed.mid(1, trimmed.size() - 2);
    }

    return trimmed;
}

int matchingParenthesisIndex(const QString &value, int openIndex)
{
    int depth = 0;
    QChar quote;

    for (int index = openIndex; index < value.size(); ++index) {
        const QChar character = value.at(index);

        if (!quote.isNull()) {
            if (character == quote && (index == 0 || value.at(index - 1) != '\\')) {
                quote = QChar();
            }
            continue;
        }

        if (character == '\'' || character == '"') {
            quote = character;
            continue;
        }

        if (character == '(') {
            ++depth;
            continue;
        }

        if (character != ')') {
            continue;
        }

        --depth;
        if (depth == 0) {
            return index;
        }
    }

    return -1;
}

int topLevelCommaIndex(const QString &value)
{
    int depth = 0;
    QChar quote;

    for (int index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);

        if (!quote.isNull()) {
            if (character == quote && (index == 0 || value.at(index - 1) != '\\')) {
                quote = QChar();
            }
            continue;
        }

        if (character == '\'' || character == '"') {
            quote = character;
            continue;
        }

        if (character == '(') {
            ++depth;
            continue;
        }

        if (character == ')') {
            depth = qMax(0, depth - 1);
            continue;
        }

        if (character == ',' && depth == 0) {
            return index;
        }
    }

    return -1;
}

QString resolveStyleValue(const QString &value,
                          const QHash<QString, QString> &styleVariables,
                          int depth)
{
    QString resolved = value;
    if (depth > 16 || !resolved.contains(QStringLiteral("var("))) {
        return resolved.trimmed();
    }

    int searchFrom = 0;
    while (true) {
        const int varIndex = resolved.indexOf(QStringLiteral("var("), searchFrom);
        if (varIndex < 0) {
            break;
        }

        const int openIndex = varIndex + 3;
        const int closeIndex = matchingParenthesisIndex(resolved, openIndex);
        if (closeIndex < 0) {
            break;
        }

        const QString arguments = resolved.mid(openIndex + 1, closeIndex - openIndex - 1).trimmed();
        const int commaIndex = topLevelCommaIndex(arguments);
        const QString variableName =
            (commaIndex < 0 ? arguments : arguments.left(commaIndex)).trimmed();
        const QString fallback =
            commaIndex < 0 ? QString() : arguments.mid(commaIndex + 1).trimmed();

        QString replacement;
        if (styleVariables.contains(variableName)) {
            replacement = resolveStyleValue(styleVariables.value(variableName), styleVariables, depth + 1);
        } else if (!fallback.isEmpty()) {
            replacement = resolveStyleValue(fallback, styleVariables, depth + 1);
        }

        resolved.replace(varIndex, closeIndex - varIndex + 1, replacement);
        searchFrom = varIndex + replacement.size();
    }

    return resolved.trimmed();
}

QString resolvedAttributeValue(const QXmlStreamAttributes &attributes,
                               const QStringList &keys,
                               const QHash<QString, QString> &styleVariables)
{
    for (const QString &key : keys) {
        if (!attributes.hasAttribute(key)) {
            continue;
        }

        return resolveStyleValue(attributes.value(key).toString(), styleVariables);
    }

    return {};
}

QColor colorFromStyleValue(const QString &value)
{
    return QColor(value.trimmed());
}

QColor styleColorValue(const QHash<QString, QString> &styleVariables,
                       const QString &name,
                       const QString &fallbackName,
                       const QColor &fallbackColor)
{
    QString value;
    if (styleVariables.contains(name)) {
        value = styleVariables.value(name);
    } else if (styleVariables.contains(fallbackName)) {
        value = styleVariables.value(fallbackName);
    }

    if (value.isEmpty()) {
        return fallbackColor;
    }

    const QColor color = colorFromStyleValue(resolveStyleValue(value, styleVariables));
    return color.isValid() ? color : fallbackColor;
}

bool parseStyleLength(const QString &value, int *parsed)
{
    QString trimmed = value.trimmed().toLower();
    if (trimmed.endsWith(QStringLiteral("px"))) {
        trimmed.chop(2);
        trimmed = trimmed.trimmed();
    }

    bool ok = false;
    const int parsedValue = trimmed.toInt(&ok);
    if (!ok) {
        return false;
    }

    *parsed = parsedValue;
    return true;
}

bool parseFirstStyleLength(const QString &value, int *parsed)
{
    const QStringList parts = value.split(QRegularExpression(QStringLiteral("\\s+")),
                                          Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }

    return parseStyleLength(parts.first(), parsed);
}

bool parsePaddingValue(const QString &value, QMargins *margins)
{
    const QStringList parts = value.split(QRegularExpression(QStringLiteral("\\s+")),
                                          Qt::SkipEmptyParts);
    if (parts.isEmpty() || parts.size() > 4) {
        return false;
    }

    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
    if (!parseStyleLength(parts.at(0), &top)) {
        return false;
    }

    if (parts.size() == 1) {
        right = top;
        bottom = top;
        left = top;
    } else if (parts.size() == 2) {
        if (!parseStyleLength(parts.at(1), &right)) {
            return false;
        }
        bottom = top;
        left = right;
    } else if (parts.size() == 3) {
        if (!parseStyleLength(parts.at(1), &right) ||
            !parseStyleLength(parts.at(2), &bottom)) {
            return false;
        }
        left = right;
    } else {
        if (!parseStyleLength(parts.at(1), &right) ||
            !parseStyleLength(parts.at(2), &bottom) ||
            !parseStyleLength(parts.at(3), &left)) {
            return false;
        }
    }

    *margins = QMargins(qMax(0, left), qMax(0, top), qMax(0, right), qMax(0, bottom));
    return true;
}

int styleLengthValue(const QHash<QString, QString> &styleVariables,
                     const QString &name,
                     int fallback)
{
    if (!styleVariables.contains(name)) {
        return fallback;
    }

    int parsed = 0;
    if (!parseStyleLength(resolveStyleValue(styleVariables.value(name), styleVariables), &parsed)) {
        return fallback;
    }

    return qMax(0, parsed);
}

QMargins notificationCardPadding(const QHash<QString, QString> &styleVariables)
{
    QMargins padding(defaultCardPaddingLeft,
                     defaultCardPaddingTop,
                     defaultCardPaddingRight,
                     defaultCardPaddingBottom);

    const QString shorthandName = QStringLiteral("--notification-card-padding");
    if (styleVariables.contains(shorthandName)) {
        QMargins parsedPadding;
        if (parsePaddingValue(resolveStyleValue(styleVariables.value(shorthandName), styleVariables),
                              &parsedPadding)) {
            padding = parsedPadding;
        }
    }

    padding.setTop(styleLengthValue(styleVariables,
                                    QStringLiteral("--notification-card-padding-top"),
                                    padding.top()));
    padding.setRight(styleLengthValue(styleVariables,
                                      QStringLiteral("--notification-card-padding-right"),
                                      padding.right()));
    padding.setBottom(styleLengthValue(styleVariables,
                                       QStringLiteral("--notification-card-padding-bottom"),
                                       padding.bottom()));
    padding.setLeft(styleLengthValue(styleVariables,
                                     QStringLiteral("--notification-card-padding-left"),
                                     padding.left()));
    return padding;
}

int notificationCardGap(const QHash<QString, QString> &styleVariables)
{
    return styleLengthValue(styleVariables,
                            QStringLiteral("--notification-card-gap"),
                            defaultCardSpacing);
}

int notificationCardBlurRadius(const QHash<QString, QString> &styleVariables)
{
    return styleLengthValue(styleVariables,
                            QStringLiteral("--notification-card-blur-radius"),
                            0);
}

int notificationCardBorderRadius(const QString &styleSheet,
                                 const QHash<QString, QString> &styleVariables)
{
    static const QRegularExpression blockPattern(
        QStringLiteral(R"(([^{}]*notificationCard[^{}]*)\{([^{}]*)\})"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression radiusPattern(
        QStringLiteral(R"(border-radius\s*:\s*([^;]+))"),
        QRegularExpression::CaseInsensitiveOption);

    auto blockMatches = blockPattern.globalMatch(styleSheet);
    int radius = 0;
    while (blockMatches.hasNext()) {
        const QRegularExpressionMatch blockMatch = blockMatches.next();
        const QRegularExpressionMatch radiusMatch = radiusPattern.match(blockMatch.captured(2));
        if (!radiusMatch.hasMatch()) {
            continue;
        }

        int parsed = 0;
        const QString value = resolveStyleValue(radiusMatch.captured(1), styleVariables);
        if (parseFirstStyleLength(value, &parsed)) {
            radius = qMax(0, parsed);
        }
    }

    return radius;
}

QRegion roundedRectRegion(const QRect &rect, int radius)
{
    if (rect.isEmpty() || radius <= 0) {
        return QRegion(rect);
    }

    const int boundedRadius = qMin(radius, qMin(rect.width(), rect.height()) / 2);
    const int diameter = boundedRadius * 2;
    if (diameter <= 0) {
        return QRegion(rect);
    }

    QRegion region(rect.adjusted(boundedRadius, 0, -boundedRadius, 0));
    region += QRegion(rect.adjusted(0, boundedRadius, 0, -boundedRadius));
    region += QRegion(QRect(rect.left(), rect.top(), diameter, diameter), QRegion::Ellipse);
    region += QRegion(QRect(rect.right() - diameter + 1, rect.top(), diameter, diameter), QRegion::Ellipse);
    region += QRegion(QRect(rect.left(), rect.bottom() - diameter + 1, diameter, diameter), QRegion::Ellipse);
    region += QRegion(QRect(rect.right() - diameter + 1,
                            rect.bottom() - diameter + 1,
                            diameter,
                            diameter),
                       QRegion::Ellipse);
    return region;
}

QString spanStyle(const QXmlStreamAttributes &attributes, const QHash<QString, QString> &styleVariables)
{
    QStringList styles;

    const auto addColorStyle = [&styles, &attributes, &styleVariables](const QString &key, const QString &cssKey) {
        if (!attributes.hasAttribute(key)) {
            return;
        }

        const QString resolvedValue = resolveStyleValue(attributes.value(key).toString(), styleVariables);
        const QColor color = colorFromStyleValue(resolvedValue);
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
        const QString family = stripEnclosingQuotes(resolveStyleValue(
            attributes.value(QStringLiteral("font_family")).toString(),
            styleVariables));
        if (!family.isEmpty()) {
            styles.append(QStringLiteral("font-family: '%1';").arg(family.toHtmlEscaped()));
        }
    } else if (attributes.hasAttribute(QStringLiteral("face"))) {
        const QString family = stripEnclosingQuotes(resolveStyleValue(
            attributes.value(QStringLiteral("face")).toString(),
            styleVariables));
        if (!family.isEmpty()) {
            styles.append(QStringLiteral("font-family: '%1';").arg(family.toHtmlEscaped()));
        }
    }

    const QString fontWeight = resolvedAttributeValue(attributes,
                                                      {
                                                          QStringLiteral("font_weight"),
                                                          QStringLiteral("font-weight"),
                                                          QStringLiteral("weight")
                                                      },
                                                      styleVariables);
    if (!fontWeight.isEmpty()) {
        styles.append(QStringLiteral("font-weight: %1;").arg(fontWeight.toHtmlEscaped()));
    }

    if (attributes.hasAttribute(QStringLiteral("font_style"))) {
        const QString fontStyle = resolveStyleValue(attributes.value(QStringLiteral("font_style")).toString(),
                                                    styleVariables);
        if (!fontStyle.isEmpty()) {
            styles.append(QStringLiteral("font-style: %1;").arg(fontStyle.toHtmlEscaped()));
        }
    } else if (attributes.hasAttribute(QStringLiteral("style"))) {
        const QString fontStyle = resolveStyleValue(attributes.value(QStringLiteral("style")).toString(),
                                                    styleVariables);
        if (!fontStyle.isEmpty()) {
            styles.append(QStringLiteral("font-style: %1;").arg(fontStyle.toHtmlEscaped()));
        }
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
        const QString style = sizeStyle(resolveStyleValue(attributes.value(QStringLiteral("size")).toString(),
                                                          styleVariables));
        if (!style.isEmpty()) {
            styles.append(style);
        }
    } else if (attributes.hasAttribute(QStringLiteral("font_size"))) {
        const QString style = sizeStyle(resolveStyleValue(
            attributes.value(QStringLiteral("font_size")).toString(),
            styleVariables));
        if (!style.isEmpty()) {
            styles.append(style);
        }
    }

    return styles.join(' ');
}

QString richTextFromMarkup(const QString &text,
                           const QHash<QString, QString> &styleVariables)
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
                const QString style = spanStyle(xml.attributes(), styleVariables).toHtmlEscaped();
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
        {
            QString characters = xml.text().toString();
            characters.replace(QChar::Nbsp, QLatin1Char(' '));
            characters.replace(QChar(0x202F), QLatin1Char(' '));
            characters.replace(QChar(0x2007), QLatin1Char(' '));
            characters.replace(QChar(0xFEFF), QLatin1Char(' '));
            characters.remove(QChar(0x2060));
            html += characters.toHtmlEscaped().replace('\n', QStringLiteral("<br/>"));
            break;
        }
        default:
            break;
        }
    }

    if (xml.hasError()) {
        QString escapedText = text;
        escapedText.replace(QChar::Nbsp, QLatin1Char(' '));
        escapedText.replace(QChar(0x202F), QLatin1Char(' '));
        escapedText.replace(QChar(0x2007), QLatin1Char(' '));
        escapedText.replace(QChar(0xFEFF), QLatin1Char(' '));
        escapedText.remove(QChar(0x2060));
        return escapedText.toHtmlEscaped().replace('\n', QStringLiteral("<br/>"));
    }

    return html;
}

QString cssQuotedFontFamily(const QString &family)
{
    QString escaped = family;
    escaped.replace('\\', QStringLiteral("\\\\"));
    escaped.replace('\'', QStringLiteral("\\'"));
    return QStringLiteral("font-family: '%1';").arg(escaped);
}

QString applyLabelFontFamily(const QString &html, const QLabel *label)
{
    if (html.isEmpty() || !label) {
        return html;
    }

    label->ensurePolished();
    const QString family = QFontInfo(label->font()).family().trimmed();
    if (family.isEmpty()) {
        return html;
    }

    return QStringLiteral("<span style=\"%1\">%2</span>")
        .arg(cssQuotedFontFamily(family).toHtmlEscaped(), html);
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

QString expandHomePath(const QString &value)
{
    if (value == "~") {
        return QDir::homePath();
    }

    if (value.startsWith("~/")) {
        return QDir::homePath() + value.mid(1);
    }

    return value;
}

QStringList expandCommandPaths(const QStringList &arguments)
{
    QStringList expandedArguments;
    expandedArguments.reserve(arguments.size());

    for (const QString &argument : arguments) {
        expandedArguments.append(expandHomePath(argument));
    }

    return expandedArguments;
}

void runNotificationCommand(const QString &command)
{
    const QString trimmedCommand = command.trimmed();
    if (trimmedCommand.isEmpty()) {
        return;
    }

    QStringList parsedCommand = QProcess::splitCommand(trimmedCommand);
    if (parsedCommand.isEmpty()) {
        return;
    }

    const QString program = expandHomePath(parsedCommand.takeFirst());
    QProcess::startDetached(program, expandCommandPaths(parsedCommand));
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
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAutoFillBackground(false);
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

QString NotificationPopup::stackTag() const
{
    return notificationStackTag(request_.hints);
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

void NotificationPopup::applyStyleSheet(const QString &styleSheet,
                                        const QHash<QString, QString> &styleVariables)
{
    appliedStyleSheet_ = styleSheet;
    styleVariables_ = styleVariables;
    setStyleSheet(styleSheet);
    applyCardLayoutStyle();
    updateTimeoutProgressStyle();
    applyWindowBlurStyle();
    refreshContent();
    refreshGeometry();
}

void NotificationPopup::showAnimated(const QPoint &targetPosition, int stackOffset)
{
    stopAnimations();
    refreshGeometry();
    currentStackOffset_ = stackOffset;
    timeoutTimer_.stop();
    const bool silentOpen = notificationSilentOpen(request_.hints);
    const bool animateOpen = config_.animation.enabled && !silentOpen;
    const bool slideOpen = animateOpen && config_.animation.slideIn;

    if (usesLayerShellPlacement()) {
        configureLayerShell(screen());
        resetContentState();
        if (animateOpen && !slideOpen) {
            setContentOpacity(0.0);
        }
        applyLayerShellPlacement(stackOffset,
                                 slideOpen ? directionalOffset(config_.animation.enterFrom)
                                           : QPoint());

        show();
        raise();
        applyWindowBlurStyle();
        schedulePostShowGeometrySync();

        if (!animateOpen) {
            restartTimeout();
            return;
        }

        if (slideOpen) {
            startLayerShellAnimation(QPoint(), config_.animation.enterDurationMs, [this]() {
                restartTimeout();
            });
        } else {
            startContentAnimation(QPoint(), 0, 1.0, config_.animation.fadeDurationMs, [this]() {
                restartTimeout();
            });
        }
        return;
    }

    move(targetPosition);
    show();
    raise();
    applyWindowBlurStyle();
    schedulePostShowGeometrySync();

    if (!animateOpen) {
        if (supportsOpacityAnimation()) {
            setWindowOpacity(1.0);
        }
        restartTimeout();
        return;
    }

    if (slideOpen) {
        move(targetPosition + directionalOffset(config_.animation.enterFrom));
    }
    if (supportsOpacityAnimation()) {
        setWindowOpacity(0.0);
    } else if (!slideOpen) {
        setContentOpacity(0.0);
    }

    if (slideOpen) {
        moveAnimation_ = new QPropertyAnimation(this, "pos", this);
        moveAnimation_->setDuration(config_.animation.enterDurationMs);
        moveAnimation_->setStartValue(pos());
        moveAnimation_->setEndValue(targetPosition);
        moveAnimation_->setEasingCurve(animationEasing());
        connect(moveAnimation_, &QPropertyAnimation::finished, this, [this]() {
            restartTimeout();
        });
        moveAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
    }

    if (supportsOpacityAnimation()) {
        fadeAnimation_ = new QPropertyAnimation(this, "windowOpacity", this);
        fadeAnimation_->setDuration(config_.animation.fadeDurationMs);
        fadeAnimation_->setStartValue(0.0);
        fadeAnimation_->setEndValue(1.0);
        fadeAnimation_->setEasingCurve(animationEasing());
        if (!slideOpen) {
            connect(fadeAnimation_, &QPropertyAnimation::finished, this, [this]() {
                restartTimeout();
            });
        }
        fadeAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
    } else if (!slideOpen) {
        startContentAnimation(QPoint(), 0, 1.0, config_.animation.fadeDurationMs, [this]() {
            restartTimeout();
        });
    }
}

void NotificationPopup::schedulePostShowGeometrySync()
{
    if (postShowGeometrySyncPending_) {
        return;
    }

    postShowGeometrySyncPending_ = true;
    QTimer::singleShot(0, this, [this]() {
        postShowGeometrySyncPending_ = false;
        if (!isVisible()) {
            return;
        }

        const int previousHeight = popupHeight();
        refreshGeometry();
        if (popupHeight() != previousHeight) {
            emit geometryUpdated();
        }
    });
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
    stopTimeoutProgress();
    stopAnimations();
    const bool silentClose = forceSilent || notificationSilentClose(request_.hints);
    const bool animateClose = config_.animation.enabled && !silentClose;
    const bool slideClose = animateClose && config_.animation.slideOut;
    const QPoint exitOffset = slideClose ? directionalOffset(effectiveExitDirection(exitDirection))
                                         : QPoint();

    if (usesLayerShellPlacement()) {
        if (!animateClose) {
            hide();
            resetContentState();
            applyLayerShellPlacement(currentStackOffset_);
            emit dismissed(request_.id, pendingCloseReason_);
            return;
        }

        auto finalizeDismissal = [this]() {
            hide();
            resetContentState();
            applyLayerShellPlacement(currentStackOffset_);
            emit dismissed(request_.id, pendingCloseReason_);
        };

        if (slideClose) {
            startLayerShellAnimation(exitOffset, config_.animation.exitDurationMs, finalizeDismissal);
        } else {
            startContentFadeOutAnimation(finalizeDismissal);
        }
        return;
    }

    if (!animateClose) {
        hide();
        emit dismissed(request_.id, pendingCloseReason_);
        return;
    }

    if (slideClose) {
        moveAnimation_ = new QPropertyAnimation(this, "pos", this);
        moveAnimation_->setDuration(config_.animation.exitDurationMs);
        moveAnimation_->setStartValue(pos());
        moveAnimation_->setEndValue(pos() + exitOffset);
        moveAnimation_->setEasingCurve(animationEasing());
    }

    auto completionCount = std::make_shared<int>(0);
    auto finalizeDismissal = [this]() {
        hide();
        resetContentState();
        emit dismissed(request_.id, pendingCloseReason_);
    };

    if (!slideClose && !supportsOpacityAnimation()) {
        startContentFadeOutAnimation(finalizeDismissal);
        return;
    }

    auto handleAnimationFinished = [completionCount, finalizeDismissal]() {
        *completionCount -= 1;
        if (*completionCount == 0) {
            finalizeDismissal();
        }
    };

    if (supportsOpacityAnimation()) {
        *completionCount += 1;
        fadeAnimation_ = new QPropertyAnimation(this, "windowOpacity", this);
        fadeAnimation_->setDuration(config_.animation.fadeDurationMs);
        fadeAnimation_->setStartValue(windowOpacity());
        fadeAnimation_->setEndValue(0.0);
        fadeAnimation_->setEasingCurve(animationEasing());
        connect(fadeAnimation_, &QPropertyAnimation::finished, this, handleAnimationFinished);
    }

    if (moveAnimation_) {
        *completionCount += 1;
        connect(moveAnimation_, &QPropertyAnimation::finished, this, handleAnimationFinished);
    }

    if (*completionCount == 0) {
        finalizeDismissal();
        return;
    }

    if (moveAnimation_) {
        moveAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
    }
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
    runNotificationCommand(notificationExecCommand(request_.hints));
    dismiss(2);
}

void NotificationPopup::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.end();

    QWidget::paintEvent(event);
}

void NotificationPopup::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncCardGeometry();
    applyWindowBlurStyle();
}

void NotificationPopup::buildUi()
{
    card_ = new QFrame(this);
    card_->setObjectName("notificationCard");

    auto *cardLayout = new QHBoxLayout(card_);
    cardLayout->setContentsMargins(notificationCardPadding(styleVariables_));
    cardLayout->setSpacing(notificationCardGap(styleVariables_));

    iconLabel_ = new QLabel(card_);
    iconLabel_->setObjectName("iconLabel");
    iconLabel_->setAlignment(Qt::AlignCenter);
    iconLabel_->setFixedSize(QSize(0, 0));
    iconLabel_->setMaximumSize(absoluteMaxIconSize, absoluteMaxIconSize);
    iconLabel_->setScaledContents(false);
    cardLayout->addWidget(iconLabel_, 0, Qt::AlignVCenter);

    textLayout_ = new QVBoxLayout();
    textLayout_->setContentsMargins(0, 0, 0, 0);
    textLayout_->setSpacing(0);
    textLayout_->addStretch(1);
    textBlockLayout_ = new QVBoxLayout();
    textBlockLayout_->setContentsMargins(0, 0, 0, 0);
    textBlockLayout_->setSpacing(effectiveTextGap());
    textLayout_->addLayout(textBlockLayout_);
    textLayout_->addStretch(1);
    cardLayout->addLayout(textLayout_, 1);

    summaryLabel_ = new QLabel(card_);
    summaryLabel_->setObjectName("summaryLabel");
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setTextFormat(Qt::RichText);
    summaryLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    textBlockLayout_->addWidget(summaryLabel_);

    bodyLabel_ = new QLabel(card_);
    bodyLabel_->setObjectName("bodyLabel");
    bodyLabel_->setWordWrap(true);
    bodyLabel_->setTextFormat(Qt::RichText);
    bodyLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    textBlockLayout_->addWidget(bodyLabel_);

    timeoutProgressRing_ = new TimeoutProgressRing(card_);
    timeoutProgressRing_->hide();
    cardLayout->addWidget(timeoutProgressRing_, 0, Qt::AlignTop | Qt::AlignRight);
    updateTimeoutProgressStyle();

    opacityEffect_ = new QGraphicsOpacityEffect(card_);
    opacityEffect_->setOpacity(1.0);
    card_->setGraphicsEffect(opacityEffect_);
    syncCardGeometry();
}

void NotificationPopup::applyCardLayoutStyle()
{
    auto *layout = qobject_cast<QBoxLayout *>(card_ ? card_->layout() : nullptr);
    if (!layout) {
        return;
    }

    layout->setContentsMargins(notificationCardPadding(styleVariables_));
    layout->setSpacing(notificationCardGap(styleVariables_));
}

void NotificationPopup::applyWindowBlurStyle()
{
#if WARD_HAS_KWINDOWSYSTEM
    QWindow *popupWindow = windowHandle();
    if (!popupWindow || !card_) {
        return;
    }

    const bool blurEnabled = notificationCardBlurRadius(styleVariables_) > 0;
    const int borderRadius = effectiveCardBorderRadius();
    KWindowEffects::enableBlurBehind(
        popupWindow,
        blurEnabled,
        blurEnabled ? roundedRectRegion(card_->geometry(), borderRadius) : QRegion());
#endif
}

void NotificationPopup::clearWindowBlurStyle()
{
#if WARD_HAS_KWINDOWSYSTEM
    if (QWindow *popupWindow = windowHandle()) {
        KWindowEffects::enableBlurBehind(popupWindow, false, QRegion());
    }
#endif
}

void NotificationPopup::syncWindowShape()
{
    if (!usesLayerShellPlacement() || !card_) {
        clearMask();
        return;
    }

    const int borderRadius = effectiveCardBorderRadius();
    if (borderRadius <= 0) {
        clearMask();
        return;
    }

    setMask(roundedRectRegion(card_->geometry(), borderRadius));
}

void NotificationPopup::refreshContent()
{
    const QString summaryText = request_.summary.trimmed();
    const QString bodyText = request_.body.trimmed();
    summaryLabel_->setVisible(!summaryText.isEmpty() || bodyText.isEmpty());
    summaryLabel_->setText(formatNotificationText(summaryText.isEmpty()
                               ? QStringLiteral("Notification")
                               : request_.summary,
                           summaryLabel_));
    bodyLabel_->setVisible(!bodyText.isEmpty());
    bodyLabel_->setText(formatNotificationText(request_.body, bodyLabel_));

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
    if (textBlockLayout_) {
        textBlockLayout_->setSpacing(effectiveTextGap());
    }
    invalidateLayout();
}

void NotificationPopup::refreshGeometry()
{
    if (timeoutProgressRing_) {
        timeoutProgressRing_->setVisible(effectiveTimeoutMs() > 0);
    }

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
    syncWindowShape();
    applyWindowBlurStyle();
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
    if (card_) {
        textWidth -= card_->frameWidth() * 2;
    }
    if (iconLabel_ && iconLabel_->isVisible()) {
        textWidth -= currentIconSize_.width();
        textWidth -= layout->spacing();
    }
    if (timeoutProgressRing_ && timeoutProgressRing_->isVisible()) {
        textWidth -= timeoutProgressRing_->width();
        textWidth -= layout->spacing();
    }

    textWidth = qMax(textWidth, 1);
    summaryLabel_->setFixedWidth(textWidth);
    bodyLabel_->setFixedWidth(textWidth);
}

void NotificationPopup::updateTimeoutProgressStyle()
{
    if (!timeoutProgressRing_) {
        return;
    }

    const QColor activeColor = styleColorValue(styleVariables_,
                                               QStringLiteral("--notification-progress-active-color"),
                                               QStringLiteral("--border-color"),
                                               QColor(Qt::white));
    const QColor troughColor = styleColorValue(styleVariables_,
                                               QStringLiteral("--notification-progress-trough-color"),
                                               QStringLiteral("--background"),
                                               QColor(255, 255, 255, 42));
    timeoutProgressRing_->setColors(activeColor, troughColor);
    timeoutProgressRing_->setRingSize(styleLengthValue(styleVariables_,
                                                       QStringLiteral("--notification-progress-size"),
                                                       18));
    timeoutProgressRing_->setStrokeWidth(styleLengthValue(styleVariables_,
                                                          QStringLiteral("--notification-progress-thickness"),
                                                          2));
}

void NotificationPopup::restartTimeoutProgress(int timeoutMs)
{
    stopTimeoutProgress();

    if (!timeoutProgressRing_) {
        return;
    }

    if (timeoutMs <= 0) {
        timeoutProgressRing_->hide();
        return;
    }

    timeoutProgressRing_->show();
    timeoutProgressRing_->setProgress(1.0);
    timeoutProgressAnimation_ = new QVariantAnimation(this);
    timeoutProgressAnimation_->setDuration(timeoutMs);
    timeoutProgressAnimation_->setStartValue(1.0);
    timeoutProgressAnimation_->setEndValue(0.0);
    timeoutProgressAnimation_->setEasingCurve(QEasingCurve::Linear);
    connect(timeoutProgressAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        if (timeoutProgressRing_) {
            timeoutProgressRing_->setProgress(value.toReal());
        }
    });
    timeoutProgressAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
}

void NotificationPopup::stopTimeoutProgress()
{
    if (timeoutProgressAnimation_) {
        timeoutProgressAnimation_->stop();
        timeoutProgressAnimation_->deleteLater();
        timeoutProgressAnimation_ = nullptr;
    }
}

QSize NotificationPopup::contentSize() const
{
    const int popupWidth = config_.layout.width;
    int measuredHeight = 0;

    if (card_) {
        card_->ensurePolished();
        if (const auto *layout = qobject_cast<const QBoxLayout *>(card_->layout())) {
            const QMargins margins = layout->contentsMargins();
            int textWidth = popupWidth - margins.left() - margins.right();
            if (card_) {
                textWidth -= card_->frameWidth() * 2;
            }
            if (iconLabel_ && iconLabel_->isVisible()) {
                textWidth -= currentIconSize_.width();
                textWidth -= layout->spacing();
            }
            if (timeoutProgressRing_ && timeoutProgressRing_->isVisible()) {
                textWidth -= timeoutProgressRing_->width();
                textWidth -= layout->spacing();
            }
            textWidth = qMax(textWidth, 1);

            int summaryHeight = 0;
            if (summaryLabel_ && summaryLabel_->isVisible()) {
                summaryLabel_->ensurePolished();
                summaryHeight = qMax(summaryLabel_->heightForWidth(textWidth),
                                     summaryLabel_->minimumSizeHint().height());
            }

            int bodyHeight = 0;
            if (bodyLabel_ && bodyLabel_->isVisible()) {
                bodyLabel_->ensurePolished();
                bodyHeight = qMax(bodyLabel_->heightForWidth(textWidth),
                                  bodyLabel_->minimumSizeHint().height());
            }

            const int textSpacing =
                (summaryHeight > 0 && bodyHeight > 0 && textBlockLayout_) ? textBlockLayout_->spacing() : 0;
            const int textHeight = summaryHeight + bodyHeight + textSpacing;
            const int iconHeight = (iconLabel_ && iconLabel_->isVisible()) ? currentIconSize_.height() : 0;

            measuredHeight = margins.top() + qMax(textHeight, iconHeight) + margins.bottom();
            measuredHeight = qMax(measuredHeight, card_->minimumSizeHint().height());
        } else {
            measuredHeight = card_->sizeHint().height();
        }
    }

    const int popupHeight = qMax(measuredHeight + textHeightSafetyPadding, effectiveMinimumHeight());
    return QSize(popupWidth, popupHeight);
}

QSize NotificationPopup::surfaceSize() const
{
    return contentSize();
}

QPoint NotificationPopup::restingContentPosition() const
{
    if (usesLayerShellPlacement()) {
        return QPoint();
    }

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
        restartTimeoutProgress(timeoutMs);
        return;
    }

    timeoutTimer_.start(timeoutMs);
    restartTimeoutProgress(timeoutMs);
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

int NotificationPopup::effectiveCardBorderRadius() const
{
    return notificationCardBorderRadius(appliedStyleSheet_, styleVariables_);
}

int NotificationPopup::effectiveTextGap() const
{
    return qMax(notificationTextGap(request_.hints, config_.notifications.textGap), 0);
}

QPoint NotificationPopup::directionalOffset(const QString &direction) const
{
    const QSize popupSize = card_ ? card_->size() : contentSize();
    const int horizontalDistance = popupSize.width() + config_.animation.slideDistance;
    const int verticalDistance = popupSize.height() + config_.animation.slideDistance;

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

QString NotificationPopup::formatNotificationText(const QString &text, const QLabel *label) const
{
    if (!label) {
        return {};
    }

    const QString html = applyLabelFontFamily(
        richTextFromMarkup(text, styleVariables_),
        label);
    if (html.isEmpty()) {
        return {};
    }

    return QStringLiteral("<div style=\"margin:0; padding:0 %1px 0 0;\">%2</div>")
        .arg(textRightCushion)
        .arg(html);
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
    if (fadeSnapshotLabel_) {
        fadeSnapshotLabel_->deleteLater();
        fadeSnapshotLabel_ = nullptr;
    }
    if (card_) {
        card_->show();
    }
    setContentOffset(QPoint());
    setContentOpacity(1.0);
    syncWindowShape();
    applyWindowBlurStyle();
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

void NotificationPopup::startContentFadeOutAnimation(const std::function<void()> &onFinished)
{
    clearWindowBlurStyle();
    clearMask();

    if (!card_ || config_.animation.fadeDurationMs <= 0) {
        setContentOpacity(0.0);
        if (onFinished) {
            onFinished();
        }
        return;
    }

    const qreal devicePixelRatio = devicePixelRatioF();
    QPixmap snapshot(card_->size() * devicePixelRatio);
    snapshot.setDevicePixelRatio(devicePixelRatio);
    snapshot.fill(Qt::transparent);
    card_->render(&snapshot);

    card_->hide();
    repaint(card_->geometry());

    auto *snapshotLabel = new QLabel(this);
    snapshotLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    snapshotLabel->setAttribute(Qt::WA_TranslucentBackground);
    snapshotLabel->setAutoFillBackground(false);
    snapshotLabel->setGeometry(card_->geometry());
    snapshotLabel->setPixmap(snapshot);
    snapshotLabel->show();
    snapshotLabel->raise();
    fadeSnapshotLabel_ = snapshotLabel;

    auto *snapshotOpacityEffect = new QGraphicsOpacityEffect(snapshotLabel);
    snapshotOpacityEffect->setOpacity(1.0);
    snapshotLabel->setGraphicsEffect(snapshotOpacityEffect);

    fadeAnimation_ = new QPropertyAnimation(snapshotOpacityEffect, "opacity", this);
    fadeAnimation_->setDuration(config_.animation.fadeDurationMs);
    fadeAnimation_->setStartValue(1.0);
    fadeAnimation_->setEndValue(0.0);
    fadeAnimation_->setEasingCurve(animationEasing());
    connect(fadeAnimation_, &QPropertyAnimation::finished, this, [this, snapshotLabel, onFinished]() {
        if (fadeSnapshotLabel_ == snapshotLabel) {
            fadeSnapshotLabel_ = nullptr;
        }
        snapshotLabel->deleteLater();
        if (card_) {
            card_->show();
        }
        setContentOpacity(0.0);
        if (onFinished) {
            onFinished();
        }
    });
    fadeAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
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

    layerShellOffset_ = offset;
    layerShellWindow_->setDesiredSize(surfaceSize());
    layerShellWindow_->setMargins(layerShellMargins(stackOffset, offset));
}

QMargins NotificationPopup::layerShellMargins(int stackOffset, const QPoint &offset) const
{
    int left = anchorAtRight() ? 0 : config_.layout.marginLeft + offset.x();
    int top = anchorAtTop() ? config_.layout.marginTop + stackOffset + offset.y() : 0;
    int right = anchorAtRight() ? config_.layout.marginRight - offset.x() : 0;
    int bottom = anchorAtTop() ? 0 : config_.layout.marginBottom + stackOffset - offset.y();

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

void NotificationPopup::startLayerShellAnimation(const QPoint &endOffset,
                                                 int durationMs,
                                                 const std::function<void()> &onFinished)
{
    if (layerShellOffset_ == endOffset || durationMs <= 0) {
        applyLayerShellPlacement(currentStackOffset_, endOffset);
        if (onFinished) {
            onFinished();
        }
        return;
    }

    moveAnimation_ = new QVariantAnimation(this);
    moveAnimation_->setDuration(durationMs);
    moveAnimation_->setStartValue(layerShellOffset_);
    moveAnimation_->setEndValue(endOffset);
    moveAnimation_->setEasingCurve(animationEasing());
    connect(moveAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        applyLayerShellPlacement(currentStackOffset_, value.toPoint());
    });
    connect(moveAnimation_, &QVariantAnimation::finished, this, [this, endOffset, onFinished]() {
        applyLayerShellPlacement(currentStackOffset_, endOffset);
        if (onFinished) {
            onFinished();
        }
    });
    moveAnimation_->start(QAbstractAnimation::DeleteWhenStopped);
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

void NotificationPopup::startLayerShellAnimation(const QPoint &, int, const std::function<void()> &)
{
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

    if (fadeSnapshotLabel_) {
        fadeSnapshotLabel_->deleteLater();
        fadeSnapshotLabel_ = nullptr;
    }

    if (card_) {
        card_->show();
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
