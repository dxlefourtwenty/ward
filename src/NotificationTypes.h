#pragma once

#include <QDBusVariant>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

struct NotificationRequest {
    uint id = 0;
    QString appName;
    QString iconName;
    QString summary;
    QString body;
    int timeoutMs = -1;
    QVariantMap hints;
};

inline QVariant unwrapHintValue(const QVariant &value)
{
    if (value.canConvert<QDBusVariant>()) {
        return value.value<QDBusVariant>().variant();
    }

    return value;
}

inline int notificationUrgency(const QVariantMap &hints)
{
    const QVariant urgency = unwrapHintValue(hints.value("urgency"));
    return urgency.isValid() ? urgency.toInt() : 1;
}

inline bool notificationHintBool(const QVariantMap &hints,
                                 const QStringList &keys,
                                 bool fallback = false)
{
    for (const QString &key : keys) {
        const QVariant value = unwrapHintValue(hints.value(key));
        if (!value.isValid()) {
            continue;
        }

        if (value.metaType().id() == QMetaType::Bool) {
            return value.toBool();
        }

        if (value.canConvert<int>()) {
            return value.toInt() != 0;
        }

        const QString lowered = value.toString().trimmed().toLower();
        if (lowered == "true" || lowered == "yes" || lowered == "on") {
            return true;
        }

        if (lowered == "false" || lowered == "no" || lowered == "off") {
            return false;
        }
    }

    return fallback;
}

inline int notificationHintInt(const QVariantMap &hints,
                               const QStringList &keys,
                               int fallback)
{
    for (const QString &key : keys) {
        const QVariant value = unwrapHintValue(hints.value(key));
        if (!value.isValid()) {
            continue;
        }

        bool ok = false;
        const int parsed = value.toString().trimmed().toInt(&ok);
        if (ok) {
            return parsed;
        }

        if (value.canConvert<int>()) {
            return value.toInt();
        }
    }

    return fallback;
}

inline bool notificationSilentOpen(const QVariantMap &hints)
{
    return notificationHintBool(hints,
                                {QStringLiteral("x-ward-silent-open"),
                                 QStringLiteral("x-ward-silent_open"),
                                 QStringLiteral("silent-open"),
                                 QStringLiteral("silent_open")});
}

inline bool notificationSilentClose(const QVariantMap &hints)
{
    return notificationHintBool(hints,
                                {QStringLiteral("x-ward-silent-close"),
                                 QStringLiteral("x-ward-silent_close"),
                                 QStringLiteral("silent-close"),
                                 QStringLiteral("silent_close")});
}

inline bool notificationReplaceLast(const QVariantMap &hints)
{
    return notificationHintBool(hints,
                                {QStringLiteral("x-ward-replace-last"),
                                 QStringLiteral("x-ward-replace_last"),
                                 QStringLiteral("replace-last"),
                                 QStringLiteral("replace_last")});
}

inline int notificationTextGap(const QVariantMap &hints, int fallback)
{
    return notificationHintInt(hints,
                               {QStringLiteral("x-ward-text-gap"),
                                QStringLiteral("x-ward-text_gap"),
                                QStringLiteral("text-gap"),
                                QStringLiteral("text_gap")},
                               fallback);
}
