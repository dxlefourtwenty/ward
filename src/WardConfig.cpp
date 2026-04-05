#include "WardConfig.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

namespace {

struct LoadedStyleSheet {
    QString styleSheet;
    QStringList dependencyFiles;
    QStringList dependencyDirectories;
};

struct ConfigLoadResult {
    WardConfig config;
    bool ok = false;
    QString error;
};

struct StyleLoadResult {
    LoadedStyleSheet styleSheet;
    bool ok = false;
    QString error;
};

struct StyleLoadContext {
    QSet<QString> activePaths;
    QSet<QString> dependencyFiles;
    QSet<QString> dependencyDirectories;
};

QString wardConfigDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/ward";
}

QString defaultConfigContents()
{
    return QStringLiteral(
        "# Animation and layout settings for ward.\n"
        "[layout]\n"
        "anchor = \"top-right\"\n"
        "screen = \"cursor\"\n"
        "# screen also accepts an exact output name such as \"HDMI-A-1\".\n"
        "width = 360\n"
        "outer_margin = 12\n"
        "top_margin = 24\n"
        "right_margin = 24\n"
        "bottom_margin = 24\n"
        "left_margin = 24\n"
        "minimum_height = 88\n"
        "\n"
        "[notifications]\n"
        "default_timeout_ms = 5000\n"
        "max_icon_size = 64\n"
        "max_notifications = 4\n"
        "text_gap = 6\n"
        "\n"
        "[animation]\n"
        "enabled = true\n"
        "enter_duration_ms = 180\n"
        "exit_duration_ms = 140\n"
        "move_duration_ms = 140\n"
        "fade_duration_ms = 140\n"
        "slide_distance = 32\n"
        "enter_from = \"right\"\n"
        "exit_to = \"right\"\n"
        "easing = \"out-cubic\"\n");
}

QString defaultStyleContents()
{
    return QStringLiteral(
        "NotificationPopup {\n"
        "    background: transparent;\n"
        "}\n"
        "\n"
        "QFrame#notificationCard {\n"
        "    background-color: rgba(18, 18, 22, 236);\n"
        "    border: 1px solid rgba(255, 255, 255, 0.08);\n"
        "    border-radius: 16px;\n"
        "}\n"
        "\n"
        "QLabel#summaryLabel {\n"
        "    color: rgb(247, 247, 250);\n"
        "    font-size: 15px;\n"
        "    font-weight: 700;\n"
        "}\n"
        "\n"
        "QLabel#bodyLabel {\n"
        "    color: rgba(255, 255, 255, 0.82);\n"
        "    font-size: 13px;\n"
        "    line-height: 1.35em;\n"
        "}\n"
        "\n"
        "QLabel#iconLabel {\n"
        "    min-width: 0px;\n"
        "}\n");
}

QString stripInlineComment(const QString &line)
{
    bool insideQuotes = false;

    for (int index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);

        if (character == '"') {
            insideQuotes = !insideQuotes;
        }

        if (!insideQuotes && character == '#') {
            return line.left(index).trimmed();
        }
    }

    return line.trimmed();
}

QString configErrorMessage(int lineNumber, const QString &message)
{
    return QStringLiteral("config.toml:%1: %2").arg(lineNumber).arg(message);
}

QString styleErrorMessage(const QString &path, const QString &message)
{
    return QStringLiteral("%1: %2").arg(path, message);
}

bool parseStringValue(const QString &value, QString *parsed)
{
    const QString trimmed = stripInlineComment(value).trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const bool startsWithQuote = trimmed.startsWith('"');
    const bool endsWithQuote = trimmed.endsWith('"');
    if (startsWithQuote != endsWithQuote || (startsWithQuote && trimmed.size() < 2)) {
        return false;
    }

    *parsed = startsWithQuote ? trimmed.mid(1, trimmed.size() - 2) : trimmed;
    return true;
}

bool parseIntValue(const QString &value, int *parsed)
{
    bool ok = false;
    const int parsedValue = stripInlineComment(value).trimmed().toInt(&ok);
    if (!ok) {
        return false;
    }

    *parsed = parsedValue;
    return true;
}

bool parseBoolValue(const QString &value, bool *parsed)
{
    const QString lowered = stripInlineComment(value).trimmed().toLower();

    if (lowered == "true") {
        *parsed = true;
        return true;
    }

    if (lowered == "false") {
        *parsed = false;
        return true;
    }

    return false;
}

bool applyLayoutEntry(const QString &key, const QString &value, WardConfig *config, QString *error)
{
    if (key == "anchor") {
        QString parsed;
        if (!parseStringValue(value, &parsed)) {
            *error = QStringLiteral("invalid string for layout.anchor");
            return false;
        }
        config->layout.anchor = parsed;
        return true;
    }

    if (key == "screen") {
        QString parsed;
        if (!parseStringValue(value, &parsed)) {
            *error = QStringLiteral("invalid string for layout.screen");
            return false;
        }
        config->layout.screen = parsed;
        return true;
    }

    if (key == "width") {
        return parseIntValue(value, &config->layout.width)
            ? true
            : ((*error = QStringLiteral("invalid integer for layout.width")), false);
    }

    if (key == "gap" || key == "outer_margin") {
        return parseIntValue(value, &config->layout.outerMargin)
            ? true
            : ((*error = QStringLiteral("invalid integer for layout.outer_margin")), false);
    }

    if (key == "margin_x") {
        int parsed = 0;
        if (!parseIntValue(value, &parsed)) {
            *error = QStringLiteral("invalid integer for layout.margin_x");
            return false;
        }
        config->layout.marginLeft = parsed;
        config->layout.marginRight = parsed;
        return true;
    }

    if (key == "margin_y") {
        int parsed = 0;
        if (!parseIntValue(value, &parsed)) {
            *error = QStringLiteral("invalid integer for layout.margin_y");
            return false;
        }
        config->layout.marginTop = parsed;
        config->layout.marginBottom = parsed;
        return true;
    }

    if (key == "top_margin") {
        return parseIntValue(value, &config->layout.marginTop)
            ? true
            : ((*error = QStringLiteral("invalid integer for layout.top_margin")), false);
    }

    if (key == "right_margin") {
        return parseIntValue(value, &config->layout.marginRight)
            ? true
            : ((*error = QStringLiteral("invalid integer for layout.right_margin")), false);
    }

    if (key == "bottom_margin") {
        return parseIntValue(value, &config->layout.marginBottom)
            ? true
            : ((*error = QStringLiteral("invalid integer for layout.bottom_margin")), false);
    }

    if (key == "left_margin") {
        return parseIntValue(value, &config->layout.marginLeft)
            ? true
            : ((*error = QStringLiteral("invalid integer for layout.left_margin")), false);
    }

    if (key == "minimum_height") {
        return parseIntValue(value, &config->layout.minimumHeight)
            ? true
            : ((*error = QStringLiteral("invalid integer for layout.minimum_height")), false);
    }

    return true;
}

bool applyNotificationEntry(const QString &key,
                            const QString &value,
                            WardConfig *config,
                            QString *error)
{
    if (key == "default_timeout_ms") {
        return parseIntValue(value, &config->notifications.defaultTimeoutMs)
            ? true
            : ((*error = QStringLiteral("invalid integer for notifications.default_timeout_ms")),
               false);
    }

    if (key == "max_icon_size") {
        return parseIntValue(value, &config->notifications.maxIconSize)
            ? true
            : ((*error = QStringLiteral("invalid integer for notifications.max_icon_size")), false);
    }

    if (key == "max_notifications" || key == "max_visible") {
        return parseIntValue(value, &config->notifications.maxNotifications)
            ? true
            : ((*error = QStringLiteral("invalid integer for notifications.max_notifications")),
               false);
    }

    if (key == "text_gap") {
        return parseIntValue(value, &config->notifications.textGap)
            ? true
            : ((*error = QStringLiteral("invalid integer for notifications.text_gap")), false);
    }

    return true;
}

bool applyAnimationEntry(const QString &key, const QString &value, WardConfig *config, QString *error)
{
    if (key == "enabled") {
        return parseBoolValue(value, &config->animation.enabled)
            ? true
            : ((*error = QStringLiteral("invalid boolean for animation.enabled")), false);
    }

    if (key == "enter_duration_ms") {
        return parseIntValue(value, &config->animation.enterDurationMs)
            ? true
            : ((*error = QStringLiteral("invalid integer for animation.enter_duration_ms")), false);
    }

    if (key == "exit_duration_ms") {
        return parseIntValue(value, &config->animation.exitDurationMs)
            ? true
            : ((*error = QStringLiteral("invalid integer for animation.exit_duration_ms")), false);
    }

    if (key == "move_duration_ms") {
        return parseIntValue(value, &config->animation.moveDurationMs)
            ? true
            : ((*error = QStringLiteral("invalid integer for animation.move_duration_ms")), false);
    }

    if (key == "fade_duration_ms") {
        return parseIntValue(value, &config->animation.fadeDurationMs)
            ? true
            : ((*error = QStringLiteral("invalid integer for animation.fade_duration_ms")), false);
    }

    if (key == "slide_distance") {
        return parseIntValue(value, &config->animation.slideDistance)
            ? true
            : ((*error = QStringLiteral("invalid integer for animation.slide_distance")), false);
    }

    if (key == "enter_from") {
        QString parsed;
        if (!parseStringValue(value, &parsed)) {
            *error = QStringLiteral("invalid string for animation.enter_from");
            return false;
        }
        config->animation.enterFrom = parsed.toLower();
        return true;
    }

    if (key == "exit_to") {
        QString parsed;
        if (!parseStringValue(value, &parsed)) {
            *error = QStringLiteral("invalid string for animation.exit_to");
            return false;
        }
        config->animation.exitTo = parsed.toLower();
        return true;
    }

    if (key == "easing") {
        QString parsed;
        if (!parseStringValue(value, &parsed)) {
            *error = QStringLiteral("invalid string for animation.easing");
            return false;
        }
        config->animation.easing = parsed.toLower();
        return true;
    }

    return true;
}

ConfigLoadResult loadWardConfig(const QString &path)
{
    ConfigLoadResult result;
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = styleErrorMessage(path, QStringLiteral("failed to open config file"));
        return result;
    }

    QTextStream stream(&file);
    QString section;
    int lineNumber = 0;

    while (!stream.atEnd()) {
        ++lineNumber;
        const QString rawLine = stream.readLine().trimmed();

        if (rawLine.isEmpty() || rawLine.startsWith('#')) {
            continue;
        }

        if (rawLine.startsWith('[')) {
            if (!rawLine.endsWith(']')) {
                result.error = configErrorMessage(lineNumber, QStringLiteral("unterminated section header"));
                return result;
            }
            section = rawLine.mid(1, rawLine.size() - 2).trimmed().toLower();
            continue;
        }

        const int separatorIndex = rawLine.indexOf('=');
        if (separatorIndex <= 0) {
            result.error = configErrorMessage(lineNumber, QStringLiteral("expected key = value"));
            return result;
        }

        const QString key = rawLine.left(separatorIndex).trimmed().toLower();
        const QString value = rawLine.mid(separatorIndex + 1).trimmed();
        QString error;

        if (section.isEmpty()) {
            result.error = configErrorMessage(lineNumber, QStringLiteral("key is outside a section"));
            return result;
        }

        if (section == "layout") {
            if (!applyLayoutEntry(key, value, &result.config, &error)) {
                result.error = configErrorMessage(lineNumber, error);
                return result;
            }
        } else if (section == "notifications") {
            if (!applyNotificationEntry(key, value, &result.config, &error)) {
                result.error = configErrorMessage(lineNumber, error);
                return result;
            }
        } else if (section == "animation") {
            if (!applyAnimationEntry(key, value, &result.config, &error)) {
                result.error = configErrorMessage(lineNumber, error);
                return result;
            }
        }
    }

    result.config.layout.width = qMax(result.config.layout.width, 220);
    result.config.layout.outerMargin = qMax(result.config.layout.outerMargin, 0);
    result.config.layout.marginTop = qMax(result.config.layout.marginTop, 0);
    result.config.layout.marginRight = qMax(result.config.layout.marginRight, 0);
    result.config.layout.marginBottom = qMax(result.config.layout.marginBottom, 0);
    result.config.layout.marginLeft = qMax(result.config.layout.marginLeft, 0);
    result.config.layout.minimumHeight = qMax(result.config.layout.minimumHeight, 1);
    result.config.notifications.defaultTimeoutMs =
        qMax(result.config.notifications.defaultTimeoutMs, 0);
    result.config.notifications.maxIconSize =
        qBound(0, result.config.notifications.maxIconSize, 128);
    result.config.notifications.maxNotifications =
        qMax(result.config.notifications.maxNotifications, 1);
    result.config.notifications.textGap = qMax(result.config.notifications.textGap, 0);
    result.config.animation.enterDurationMs = qMax(result.config.animation.enterDurationMs, 0);
    result.config.animation.exitDurationMs = qMax(result.config.animation.exitDurationMs, 0);
    result.config.animation.moveDurationMs = qMax(result.config.animation.moveDurationMs, 0);
    result.config.animation.fadeDurationMs = qMax(result.config.animation.fadeDurationMs, 0);
    result.config.animation.slideDistance = qMax(result.config.animation.slideDistance, 0);
    result.ok = true;

    return result;
}

QString normalizedPath(const QString &path)
{
    const QFileInfo fileInfo(path);
    return fileInfo.exists() ? fileInfo.canonicalFilePath() : fileInfo.absoluteFilePath();
}

QString resolveCustomPropertyValue(const QString &name,
                                   const QHash<QString, QString> &variables,
                                   QSet<QString> *resolvingNames);

QString substituteCustomProperties(const QString &input,
                                   const QHash<QString, QString> &variables,
                                   QSet<QString> *resolvingNames)
{
    static const QRegularExpression variablePattern(
        R"(var\(\s*(--[A-Za-z0-9_-]+)\s*(?:,\s*([^)]+))?\))");

    QString output;
    output.reserve(input.size());

    int currentIndex = 0;
    auto matches = variablePattern.globalMatch(input);
    while (matches.hasNext()) {
        const auto match = matches.next();
        output += input.mid(currentIndex, match.capturedStart() - currentIndex);

        const QString variableName = match.captured(1).trimmed();
        QString replacement = resolveCustomPropertyValue(variableName, variables, resolvingNames);
        if (replacement.isEmpty() && match.lastCapturedIndex() >= 2) {
            replacement = substituteCustomProperties(
                match.captured(2).trimmed(),
                variables,
                resolvingNames);
        }

        output += replacement.isEmpty() ? match.captured(0) : replacement;
        currentIndex = match.capturedEnd();
    }

    output += input.mid(currentIndex);
    return output;
}

QString resolveCustomPropertyValue(const QString &name,
                                   const QHash<QString, QString> &variables,
                                   QSet<QString> *resolvingNames)
{
    if (!variables.contains(name) || resolvingNames->contains(name)) {
        return {};
    }

    resolvingNames->insert(name);
    const QString value = substituteCustomProperties(variables.value(name), variables, resolvingNames);
    resolvingNames->remove(name);
    return value;
}

QString stripAndResolveCustomProperties(const QString &styleSheet)
{
    static const QRegularExpression blockPattern(R"(([^{}]+)\{([^{}]*)\})");

    QHash<QString, QString> variables;
    QString withoutVariables;
    withoutVariables.reserve(styleSheet.size());

    int currentIndex = 0;
    auto matches = blockPattern.globalMatch(styleSheet);
    while (matches.hasNext()) {
        const auto match = matches.next();
        withoutVariables += styleSheet.mid(currentIndex, match.capturedStart() - currentIndex);

        const QString selector = match.captured(1).trimmed();
        const QStringList declarations = match.captured(2).split(';', Qt::KeepEmptyParts);
        QStringList keptDeclarations;

        for (const QString &declaration : declarations) {
            const QString trimmedDeclaration = declaration.trimmed();
            if (trimmedDeclaration.isEmpty()) {
                continue;
            }

            const int separatorIndex = trimmedDeclaration.indexOf(':');
            if (separatorIndex <= 0) {
                keptDeclarations.append(trimmedDeclaration);
                continue;
            }

            const QString propertyName = trimmedDeclaration.left(separatorIndex).trimmed();
            const QString propertyValue = trimmedDeclaration.mid(separatorIndex + 1).trimmed();
            if (propertyName.startsWith("--")) {
                variables.insert(propertyName, propertyValue);
                continue;
            }

            keptDeclarations.append(QStringLiteral("%1: %2;").arg(propertyName, propertyValue));
        }

        if (!keptDeclarations.isEmpty()) {
            withoutVariables +=
                QStringLiteral("%1 {\n    %2\n}\n").arg(selector, keptDeclarations.join(QStringLiteral("\n    ")));
        }

        currentIndex = match.capturedEnd();
    }

    withoutVariables += styleSheet.mid(currentIndex);

    QSet<QString> resolvingNames;
    return substituteCustomProperties(withoutVariables, variables, &resolvingNames);
}

QString normalizeWardSelectors(const QString &styleSheet)
{
    QString normalized = styleSheet;
    static const QRegularExpression popupSelectorPattern(
        R"((^|[\n\r}]\s*)NotificationPopup(\s*\{))");
    normalized.replace(popupSelectorPattern, QStringLiteral("\\1QWidget#notificationPopup\\2"));
    return normalized;
}

bool validateStyleSheetSyntax(const QString &styleSheet, QString *error)
{
    int braceDepth = 0;
    bool insideSingleQuote = false;
    bool insideDoubleQuote = false;
    bool insideBlockComment = false;

    for (int index = 0; index < styleSheet.size(); ++index) {
        const QChar character = styleSheet.at(index);
        const QChar nextCharacter = index + 1 < styleSheet.size() ? styleSheet.at(index + 1) : QChar();
        const QChar previousCharacter = index > 0 ? styleSheet.at(index - 1) : QChar();

        if (insideBlockComment) {
            if (character == '*' && nextCharacter == '/') {
                insideBlockComment = false;
                ++index;
            }
            continue;
        }

        if (!insideSingleQuote && !insideDoubleQuote && character == '/' && nextCharacter == '*') {
            insideBlockComment = true;
            ++index;
            continue;
        }

        if (!insideDoubleQuote && character == '\'' && previousCharacter != '\\') {
            insideSingleQuote = !insideSingleQuote;
            continue;
        }

        if (!insideSingleQuote && character == '"' && previousCharacter != '\\') {
            insideDoubleQuote = !insideDoubleQuote;
            continue;
        }

        if (insideSingleQuote || insideDoubleQuote) {
            continue;
        }

        if (character == '{') {
            ++braceDepth;
            continue;
        }

        if (character == '}') {
            if (braceDepth == 0) {
                *error = QStringLiteral("unexpected closing brace");
                return false;
            }
            --braceDepth;
        }
    }

    if (insideSingleQuote || insideDoubleQuote) {
        *error = QStringLiteral("unterminated string");
        return false;
    }

    if (insideBlockComment) {
        *error = QStringLiteral("unterminated block comment");
        return false;
    }

    if (braceDepth != 0) {
        *error = QStringLiteral("unterminated rule block");
        return false;
    }

    return true;
}

QString finalizeStyleSheet(const QString &styleSheet)
{
    return normalizeWardSelectors(stripAndResolveCustomProperties(styleSheet)) + QStringLiteral(
        "\nQLabel#iconLabel {\n"
        "    min-width: 0px;\n"
        "    max-width: 16777215px;\n"
        "    min-height: 0px;\n"
        "    max-height: 16777215px;\n"
        "}\n");
}

StyleLoadResult loadStyleFile(const QString &path, bool isRootFile, StyleLoadContext *context);

StyleLoadResult inlineImports(const QString &styleSheet,
                              const QString &baseDirectory,
                              StyleLoadContext *context)
{
    static const QRegularExpression importPattern(
        R"(@import[ \t\f]+(?:url\([ \t\f]*(['"]?)([^'")\s]+)\1[ \t\f]*\)|(['"])([^'"]+)\3)[ \t\f]*[^;\n\r]*(?:;|(?=\r?\n)|$))");

    StyleLoadResult result;
    QString output;
    output.reserve(styleSheet.size());

    int currentIndex = 0;
    auto matches = importPattern.globalMatch(styleSheet);
    while (matches.hasNext()) {
        const auto match = matches.next();
        output += styleSheet.mid(currentIndex, match.capturedStart() - currentIndex);

        const QString importPath = match.captured(2).isEmpty()
            ? match.captured(4).trimmed()
            : match.captured(2).trimmed();

        if (importPath.startsWith(QStringLiteral("http://")) ||
            importPath.startsWith(QStringLiteral("https://"))) {
            output += match.captured(0);
        } else {
            const QString resolvedPath = normalizedPath(QDir(baseDirectory).absoluteFilePath(importPath));
            context->dependencyDirectories.insert(QFileInfo(resolvedPath).absolutePath());
            const StyleLoadResult imported = loadStyleFile(resolvedPath, false, context);
            if (!imported.ok) {
                return imported;
            }
            output += imported.styleSheet.styleSheet;
            output += '\n';
        }

        currentIndex = match.capturedEnd();
    }

    output += styleSheet.mid(currentIndex);
    result.styleSheet.styleSheet = output;
    result.ok = true;
    return result;
}

StyleLoadResult loadStyleFile(const QString &path, bool isRootFile, StyleLoadContext *context)
{
    StyleLoadResult result;
    const QString resolvedPath = normalizedPath(path);
    if (context->activePaths.contains(resolvedPath)) {
        result.ok = true;
        return result;
    }

    context->activePaths.insert(resolvedPath);
    context->dependencyDirectories.insert(QFileInfo(resolvedPath).absolutePath());

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        context->activePaths.remove(resolvedPath);
        result.error = styleErrorMessage(
            resolvedPath,
            isRootFile ? QStringLiteral("failed to open style.css")
                       : QStringLiteral("failed to open imported stylesheet"));
        return result;
    }

    context->dependencyFiles.insert(resolvedPath);
    const QString styleSheet = QString::fromUtf8(file.readAll());
    QString validationError;
    if (!validateStyleSheetSyntax(styleSheet, &validationError)) {
        context->activePaths.remove(resolvedPath);
        result.error = styleErrorMessage(resolvedPath, validationError);
        return result;
    }

    const StyleLoadResult inlinedStyleSheet =
        inlineImports(styleSheet, QFileInfo(resolvedPath).absolutePath(), context);
    if (!inlinedStyleSheet.ok) {
        context->activePaths.remove(resolvedPath);
        return inlinedStyleSheet;
    }

    context->activePaths.remove(resolvedPath);
    result.styleSheet.styleSheet = inlinedStyleSheet.styleSheet.styleSheet;
    result.ok = true;
    return result;
}

StyleLoadResult loadStyleSheet(const QString &path)
{
    StyleLoadResult result;
    StyleLoadContext context;
    const StyleLoadResult styleFile = loadStyleFile(path, true, &context);
    if (!styleFile.ok) {
        return styleFile;
    }

    result.styleSheet.styleSheet = finalizeStyleSheet(styleFile.styleSheet.styleSheet);
    result.styleSheet.dependencyFiles = context.dependencyFiles.values();
    result.styleSheet.dependencyDirectories = context.dependencyDirectories.values();
    result.ok = true;
    return result;
}

} // namespace

WardConfigLoader::WardConfigLoader(QObject *parent)
    : QObject(parent)
{
    ensureConfigFiles();
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, &WardConfigLoader::reload);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, &WardConfigLoader::reload);
    reload();
}

const WardConfig &WardConfigLoader::config() const
{
    return config_;
}

const QString &WardConfigLoader::styleSheet() const
{
    return styleSheet_;
}

QString WardConfigLoader::configPath() const
{
    return wardConfigDirectory() + "/config.toml";
}

QString WardConfigLoader::stylePath() const
{
    return wardConfigDirectory() + "/style.css";
}

void WardConfigLoader::reload()
{
    const ConfigLoadResult loadedConfig = loadWardConfig(configPath());
    if (loadedConfig.ok) {
        config_ = loadedConfig.config;
    } else {
        qWarning().noquote() << "ward:" << loadedConfig.error;
    }

    const StyleLoadResult loadedStyleSheet = loadStyleSheet(stylePath());
    if (loadedStyleSheet.ok) {
        styleSheet_ = loadedStyleSheet.styleSheet.styleSheet;
        styleDependencyFiles_ = loadedStyleSheet.styleSheet.dependencyFiles;
        styleDependencyDirectories_ = loadedStyleSheet.styleSheet.dependencyDirectories;
    } else {
        if (styleSheet_.isEmpty()) {
            styleSheet_ = finalizeStyleSheet(defaultStyleContents());
        }
        qWarning().noquote() << "ward:" << loadedStyleSheet.error;
    }

    refreshWatchers();

    if (loadedConfig.ok) {
        emit configChanged(config_);
    }

    if (loadedStyleSheet.ok) {
        emit styleChanged(styleSheet_);
    }
}

void WardConfigLoader::ensureConfigFiles()
{
    QDir().mkpath(wardConfigDirectory());

    const bool configExists = QFile::exists(configPath());
    const bool styleExists = QFile::exists(stylePath());
    if (configExists || styleExists) {
        return;
    }

    if (!configExists) {
        QFile file(configPath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(defaultConfigContents().toUtf8());
        }
    }

    if (!QFile::exists(stylePath())) {
        QFile file(stylePath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(defaultStyleContents().toUtf8());
        }
    }
}

void WardConfigLoader::refreshWatchers()
{
    if (!watcher_.files().isEmpty()) {
        watcher_.removePaths(watcher_.files());
    }

    if (!watcher_.directories().isEmpty()) {
        watcher_.removePaths(watcher_.directories());
    }

    QSet<QString> filePaths = {
        normalizedPath(configPath()),
        normalizedPath(stylePath()),
    };
    for (const QString &path : styleDependencyFiles_) {
        if (QFileInfo::exists(path)) {
            filePaths.insert(normalizedPath(path));
        }
    }

    QSet<QString> directoryPaths = {
        wardConfigDirectory(),
    };
    for (const QString &path : styleDependencyDirectories_) {
        if (QDir(path).exists()) {
            directoryPaths.insert(path);
        }
    }

    watcher_.addPaths(QStringList(filePaths.begin(), filePaths.end()));
    watcher_.addPaths(QStringList(directoryPaths.begin(), directoryPaths.end()));
}
