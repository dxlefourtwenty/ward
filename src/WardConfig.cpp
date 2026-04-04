#include "WardConfig.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

namespace {

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
    config.animation.enterDurationMs = qMax(config.animation.enterDurationMs, 0);
    config.animation.exitDurationMs = qMax(config.animation.exitDurationMs, 0);
    config.animation.moveDurationMs = qMax(config.animation.moveDurationMs, 0);
    config.animation.fadeDurationMs = qMax(config.animation.fadeDurationMs, 0);
    config.animation.slideDistance = qMax(config.animation.slideDistance, 0);

    return config;
}

QString loadStyleSheet(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return defaultStyleContents();
    }

    QString styleSheet = QString::fromUtf8(file.readAll());
    styleSheet += QStringLiteral(
        "\nQLabel#iconLabel {\n"
        "    min-width: 0px;\n"
        "    max-width: 16777215px;\n"
        "    min-height: 0px;\n"
        "    max-height: 16777215px;\n"
        "}\n");

    return styleSheet;
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
    refreshWatchers();

    config_ = loadWardConfig(configPath());
    styleSheet_ = loadStyleSheet(stylePath());

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

    watcher_.addPath(wardConfigDirectory());
    watcher_.addPath(configPath());
    watcher_.addPath(stylePath());
}
