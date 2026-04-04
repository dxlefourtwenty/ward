#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QString>

struct WardLayoutConfig {
    QString anchor = "top-right";
    QString screen = "cursor";
    int width = 360;
    int outerMargin = 12;
    int marginTop = 24;
    int marginRight = 24;
    int marginBottom = 24;
    int marginLeft = 24;
    int minimumHeight = 88;
};

struct WardNotificationConfig {
    int defaultTimeoutMs = 5000;
    int maxIconSize = 64;
    int maxNotifications = 4;
    int textGap = 6;
};

struct WardAnimationConfig {
    bool enabled = true;
    int enterDurationMs = 180;
    int exitDurationMs = 140;
    int moveDurationMs = 140;
    int fadeDurationMs = 140;
    int slideDistance = 32;
    QString enterFrom = "right";
    QString exitTo = "right";
    QString easing = "out-cubic";
};

struct WardConfig {
    WardLayoutConfig layout;
    WardNotificationConfig notifications;
    WardAnimationConfig animation;
};

class WardConfigLoader : public QObject {
    Q_OBJECT

public:
    explicit WardConfigLoader(QObject *parent = nullptr);

    const WardConfig &config() const;
    const QString &styleSheet() const;
    QString configPath() const;
    QString stylePath() const;

public slots:
    void reload();

signals:
    void configChanged(const WardConfig &config);
    void styleChanged(const QString &styleSheet);

private:
    void ensureConfigFiles() const;
    void refreshWatchers();

    QFileSystemWatcher watcher_;
    WardConfig config_;
    QString styleSheet_;
    QStringList styleDependencyFiles_;
    QStringList styleDependencyDirectories_;
};
