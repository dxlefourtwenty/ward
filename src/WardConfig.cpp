#include "WardConfig.h"

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

QString parseString(const QString &value, const QString &fallback)
{
    const QString trimmed = value.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"') && trimmed.size() >= 2) {
        return trimmed.mid(1, trimmed.size() - 2);
    }

    return trimmed.isEmpty() ? fallback : trimmed;
}

int parseInt(const QString &value, int fallback)
{
    bool ok = false;
    const int parsed = stripInlineComment(value).toInt(&ok);
    return ok ? parsed : fallback;
}

bool parseBool(const QString &value, bool fallback)
{
    const QString lowered = stripInlineComment(value).trimmed().toLower();

    if (lowered == "true") {
        return true;
    }

    if (lowered == "false") {
        return false;
    }

    return fallback;
}

WardConfig loadWardConfig(const QString &path)
{
    WardConfig config;
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return config;
    }

    QTextStream stream(&file);
    QString section;

    while (!stream.atEnd()) {
        const QString rawLine = stream.readLine().trimmed();

        if (rawLine.isEmpty() || rawLine.startsWith('#')) {
            continue;
        }

        if (rawLine.startsWith('[') && rawLine.endsWith(']')) {
            section = rawLine.mid(1, rawLine.size() - 2).trimmed().toLower();
            continue;
        }

        const int separatorIndex = rawLine.indexOf('=');
        if (separatorIndex <= 0) {
            continue;
        }

        const QString key = rawLine.left(separatorIndex).trimmed().toLower();
        const QString value = rawLine.mid(separatorIndex + 1).trimmed();

        if (section == "layout") {
            if (key == "anchor") {
                config.layout.anchor = parseString(value, config.layout.anchor);
            } else if (key == "screen") {
                config.layout.screen = parseString(value, config.layout.screen);
            } else if (key == "width") {
                config.layout.width = parseInt(value, config.layout.width);
            } else if (key == "gap" || key == "outer_margin") {
                config.layout.outerMargin = parseInt(value, config.layout.outerMargin);
            } else if (key == "margin_x") {
                const int margin = parseInt(value, config.layout.marginLeft);
                config.layout.marginLeft = margin;
                config.layout.marginRight = margin;
            } else if (key == "margin_y") {
                const int margin = parseInt(value, config.layout.marginTop);
                config.layout.marginTop = margin;
                config.layout.marginBottom = margin;
            } else if (key == "top_margin") {
                config.layout.marginTop = parseInt(value, config.layout.marginTop);
            } else if (key == "right_margin") {
                config.layout.marginRight = parseInt(value, config.layout.marginRight);
            } else if (key == "bottom_margin") {
                config.layout.marginBottom = parseInt(value, config.layout.marginBottom);
            } else if (key == "left_margin") {
                config.layout.marginLeft = parseInt(value, config.layout.marginLeft);
            } else if (key == "minimum_height") {
                config.layout.minimumHeight = parseInt(value, config.layout.minimumHeight);
            }
        } else if (section == "notifications") {
            if (key == "default_timeout_ms") {
                config.notifications.defaultTimeoutMs =
                    parseInt(value, config.notifications.defaultTimeoutMs);
            } else if (key == "max_icon_size") {
                config.notifications.maxIconSize =
                    parseInt(value, config.notifications.maxIconSize);
            } else if (key == "max_notifications") {
                config.notifications.maxNotifications =
                    parseInt(value, config.notifications.maxNotifications);
            } else if (key == "max_visible") {
                config.notifications.maxNotifications =
                    parseInt(value, config.notifications.maxNotifications);
            } else if (key == "text_gap") {
                config.notifications.textGap =
                    parseInt(value, config.notifications.textGap);
            }
        } else if (section == "animation") {
            if (key == "enabled") {
                config.animation.enabled = parseBool(value, config.animation.enabled);
            } else if (key == "enter_duration_ms") {
                config.animation.enterDurationMs =
                    parseInt(value, config.animation.enterDurationMs);
            } else if (key == "exit_duration_ms") {
                config.animation.exitDurationMs =
                    parseInt(value, config.animation.exitDurationMs);
            } else if (key == "move_duration_ms") {
                config.animation.moveDurationMs =
                    parseInt(value, config.animation.moveDurationMs);
            } else if (key == "fade_duration_ms") {
                config.animation.fadeDurationMs =
                    parseInt(value, config.animation.fadeDurationMs);
            } else if (key == "slide_distance") {
                config.animation.slideDistance =
                    parseInt(value, config.animation.slideDistance);
            } else if (key == "enter_from") {
                config.animation.enterFrom =
                    parseString(value, config.animation.enterFrom).toLower();
            } else if (key == "exit_to") {
                config.animation.exitTo =
                    parseString(value, config.animation.exitTo).toLower();
            } else if (key == "easing") {
                config.animation.easing =
                    parseString(value, config.animation.easing).toLower();
            }
        }
    }

    config.layout.width = qMax(config.layout.width, 220);
    config.layout.outerMargin = qMax(config.layout.outerMargin, 0);
    config.layout.marginTop = qMax(config.layout.marginTop, 0);
    config.layout.marginRight = qMax(config.layout.marginRight, 0);
    config.layout.marginBottom = qMax(config.layout.marginBottom, 0);
    config.layout.marginLeft = qMax(config.layout.marginLeft, 0);
    config.layout.minimumHeight = qMax(config.layout.minimumHeight, 1);
    config.notifications.defaultTimeoutMs = qMax(config.notifications.defaultTimeoutMs, 0);
    config.notifications.maxIconSize = qBound(0, config.notifications.maxIconSize, 128);
    config.notifications.maxNotifications = qMax(config.notifications.maxNotifications, 1);
    config.notifications.textGap = qMax(config.notifications.textGap, 0);
    config.animation.enterDurationMs = qMax(config.animation.enterDurationMs, 0);
    config.animation.exitDurationMs = qMax(config.animation.exitDurationMs, 0);
    config.animation.moveDurationMs = qMax(config.animation.moveDurationMs, 0);
    config.animation.fadeDurationMs = qMax(config.animation.fadeDurationMs, 0);
    config.animation.slideDistance = qMax(config.animation.slideDistance, 0);

    return config;
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

QString loadStyleFile(const QString &path, bool isRootFile, StyleLoadContext *context);

QString inlineImports(const QString &styleSheet, const QString &baseDirectory, StyleLoadContext *context)
{
    static const QRegularExpression importPattern(
        R"(@import[ \t\f]+(?:url\([ \t\f]*(['"]?)([^'")\s]+)\1[ \t\f]*\)|(['"])([^'"]+)\3)[ \t\f]*[^;\n\r]*(?:;|(?=\r?\n)|$))");

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
            output += loadStyleFile(resolvedPath, false, context);
            output += '\n';
        }

        currentIndex = match.capturedEnd();
    }

    output += styleSheet.mid(currentIndex);
    return output;
}

QString loadStyleFile(const QString &path, bool isRootFile, StyleLoadContext *context)
{
    const QString resolvedPath = normalizedPath(path);
    if (context->activePaths.contains(resolvedPath)) {
        return {};
    }

    context->activePaths.insert(resolvedPath);
    context->dependencyDirectories.insert(QFileInfo(resolvedPath).absolutePath());

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        context->activePaths.remove(resolvedPath);
        return isRootFile ? defaultStyleContents() : QString{};
    }

    context->dependencyFiles.insert(resolvedPath);
    const QString styleSheet = QString::fromUtf8(file.readAll());
    const QString inlinedStyleSheet =
        inlineImports(styleSheet, QFileInfo(resolvedPath).absolutePath(), context);

    context->activePaths.remove(resolvedPath);
    return inlinedStyleSheet;
}

LoadedStyleSheet loadStyleSheet(const QString &path)
{
    StyleLoadContext context;
    const QString styleSheet = normalizeWardSelectors(
        stripAndResolveCustomProperties(loadStyleFile(path, true, &context)));

    LoadedStyleSheet loadedStyleSheet;
    loadedStyleSheet.styleSheet = styleSheet + QStringLiteral(
        "\nQLabel#iconLabel {\n"
        "    min-width: 0px;\n"
        "    max-width: 16777215px;\n"
        "    min-height: 0px;\n"
        "    max-height: 16777215px;\n"
        "}\n");
    loadedStyleSheet.dependencyFiles = context.dependencyFiles.values();
    loadedStyleSheet.dependencyDirectories = context.dependencyDirectories.values();

    return loadedStyleSheet;
}

} // namespace

WardConfigLoader::WardConfigLoader(QObject *parent)
    : QObject(parent)
{
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
    ensureConfigFiles();

    config_ = loadWardConfig(configPath());
    const LoadedStyleSheet loadedStyleSheet = loadStyleSheet(stylePath());
    styleSheet_ = loadedStyleSheet.styleSheet;
    styleDependencyFiles_ = loadedStyleSheet.dependencyFiles;
    styleDependencyDirectories_ = loadedStyleSheet.dependencyDirectories;
    refreshWatchers();

    emit configChanged(config_);
    emit styleChanged(styleSheet_);
}

void WardConfigLoader::ensureConfigFiles() const
{
    QDir().mkpath(wardConfigDirectory());

    if (!QFile::exists(configPath())) {
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
