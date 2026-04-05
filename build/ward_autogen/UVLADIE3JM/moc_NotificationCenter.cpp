/****************************************************************************
** Meta object code from reading C++ file 'NotificationCenter.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/NotificationCenter.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'NotificationCenter.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN18NotificationCenterE_t {};
} // unnamed namespace

template <> constexpr inline auto NotificationCenter::qt_create_metaobjectdata<qt_meta_tag_ZN18NotificationCenterE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "NotificationCenter",
        "notificationClosed",
        "",
        "id",
        "reason",
        "reloadConfiguration",
        "showNotification",
        "appName",
        "summary",
        "body",
        "iconName",
        "timeoutMs",
        "QVariantMap",
        "hints",
        "closeNotification",
        "applyConfig",
        "WardConfig",
        "config",
        "applyStyle",
        "styleSheet",
        "QHash<QString,QString>",
        "styleVariables",
        "handlePopupDismissed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'notificationClosed'
        QtMocHelpers::SignalData<void(uint, uint)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 3 }, { QMetaType::UInt, 4 },
        }}),
        // Slot 'reloadConfiguration'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showNotification'
        QtMocHelpers::SlotData<void(uint, const QString &, const QString &, const QString &, const QString &, int, const QVariantMap &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 3 }, { QMetaType::QString, 7 }, { QMetaType::QString, 8 }, { QMetaType::QString, 9 },
            { QMetaType::QString, 10 }, { QMetaType::Int, 11 }, { 0x80000000 | 12, 13 },
        }}),
        // Slot 'closeNotification'
        QtMocHelpers::SlotData<void(uint, uint)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 3 }, { QMetaType::UInt, 4 },
        }}),
        // Slot 'closeNotification'
        QtMocHelpers::SlotData<void(uint)>(14, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::UInt, 3 },
        }}),
        // Slot 'applyConfig'
        QtMocHelpers::SlotData<void(const WardConfig &)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Slot 'applyStyle'
        QtMocHelpers::SlotData<void(const QString &, const QHash<QString,QString> &)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 19 }, { 0x80000000 | 20, 21 },
        }}),
        // Slot 'handlePopupDismissed'
        QtMocHelpers::SlotData<void(uint, uint)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::UInt, 3 }, { QMetaType::UInt, 4 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<NotificationCenter, qt_meta_tag_ZN18NotificationCenterE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject NotificationCenter::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18NotificationCenterE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18NotificationCenterE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18NotificationCenterE_t>.metaTypes,
    nullptr
} };

void NotificationCenter::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<NotificationCenter *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->notificationClosed((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<uint>>(_a[2]))); break;
        case 1: _t->reloadConfiguration(); break;
        case 2: _t->showNotification((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[7]))); break;
        case 3: _t->closeNotification((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<uint>>(_a[2]))); break;
        case 4: _t->closeNotification((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1]))); break;
        case 5: _t->applyConfig((*reinterpret_cast<std::add_pointer_t<WardConfig>>(_a[1]))); break;
        case 6: _t->applyStyle((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QHash<QString,QString>>>(_a[2]))); break;
        case 7: _t->handlePopupDismissed((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<uint>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (NotificationCenter::*)(uint , uint )>(_a, &NotificationCenter::notificationClosed, 0))
            return;
    }
}

const QMetaObject *NotificationCenter::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *NotificationCenter::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18NotificationCenterE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int NotificationCenter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void NotificationCenter::notificationClosed(uint _t1, uint _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}
QT_WARNING_POP
