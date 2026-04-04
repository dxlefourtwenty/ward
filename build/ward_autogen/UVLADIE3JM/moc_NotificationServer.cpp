/****************************************************************************
** Meta object code from reading C++ file 'NotificationServer.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/NotificationServer.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'NotificationServer.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18NotificationServerE_t {};
} // unnamed namespace

template <> constexpr inline auto NotificationServer::qt_create_metaobjectdata<qt_meta_tag_ZN18NotificationServerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "NotificationServer",
        "D-Bus Interface",
        "org.freedesktop.Notifications",
        "newNotification",
        "",
        "id",
        "appName",
        "summary",
        "body",
        "iconName",
        "timeoutMs",
        "QVariantMap",
        "hints",
        "closeRequested",
        "reason",
        "NotificationClosed",
        "ActionInvoked",
        "actionKey",
        "Notify",
        "app_name",
        "replaces_id",
        "icon",
        "actions",
        "timeout",
        "GetCapabilities",
        "GetServerInformation",
        "QString&",
        "name",
        "vendor",
        "version",
        "specVersion",
        "CloseNotification",
        "handleNotificationClosed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'newNotification'
        QtMocHelpers::SignalData<void(uint, const QString &, const QString &, const QString &, const QString &, int, const QVariantMap &)>(3, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 5 }, { QMetaType::QString, 6 }, { QMetaType::QString, 7 }, { QMetaType::QString, 8 },
            { QMetaType::QString, 9 }, { QMetaType::Int, 10 }, { 0x80000000 | 11, 12 },
        }}),
        // Signal 'closeRequested'
        QtMocHelpers::SignalData<void(uint, uint)>(13, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 5 }, { QMetaType::UInt, 14 },
        }}),
        // Signal 'NotificationClosed'
        QtMocHelpers::SignalData<void(uint, uint)>(15, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 5 }, { QMetaType::UInt, 14 },
        }}),
        // Signal 'ActionInvoked'
        QtMocHelpers::SignalData<void(uint, const QString &)>(16, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 5 }, { QMetaType::QString, 17 },
        }}),
        // Slot 'Notify'
        QtMocHelpers::SlotData<uint(const QString &, uint, const QString &, const QString &, const QString &, const QStringList &, const QVariantMap &, int)>(18, 4, QMC::AccessPublic, QMetaType::UInt, {{
            { QMetaType::QString, 19 }, { QMetaType::UInt, 20 }, { QMetaType::QString, 21 }, { QMetaType::QString, 7 },
            { QMetaType::QString, 8 }, { QMetaType::QStringList, 22 }, { 0x80000000 | 11, 12 }, { QMetaType::Int, 23 },
        }}),
        // Slot 'GetCapabilities'
        QtMocHelpers::SlotData<QStringList() const>(24, 4, QMC::AccessPublic, QMetaType::QStringList),
        // Slot 'GetServerInformation'
        QtMocHelpers::SlotData<void(QString &, QString &, QString &, QString &) const>(25, 4, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 26, 27 }, { 0x80000000 | 26, 28 }, { 0x80000000 | 26, 29 }, { 0x80000000 | 26, 30 },
        }}),
        // Slot 'CloseNotification'
        QtMocHelpers::SlotData<void(uint)>(31, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 5 },
        }}),
        // Slot 'handleNotificationClosed'
        QtMocHelpers::SlotData<void(uint, uint)>(32, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 5 }, { QMetaType::UInt, 14 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<NotificationServer, qt_meta_tag_ZN18NotificationServerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject NotificationServer::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18NotificationServerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18NotificationServerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18NotificationServerE_t>.metaTypes,
    nullptr
} };

void NotificationServer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<NotificationServer *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->newNotification((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[7]))); break;
        case 1: _t->closeRequested((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<uint>>(_a[2]))); break;
        case 2: _t->NotificationClosed((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<uint>>(_a[2]))); break;
        case 3: _t->ActionInvoked((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: { uint _r = _t->Notify((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<uint>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[7])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[8])));
            if (_a[0]) *reinterpret_cast<uint*>(_a[0]) = std::move(_r); }  break;
        case 5: { QStringList _r = _t->GetCapabilities();
            if (_a[0]) *reinterpret_cast<QStringList*>(_a[0]) = std::move(_r); }  break;
        case 6: _t->GetServerInformation((*reinterpret_cast<std::add_pointer_t<QString&>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString&>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString&>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString&>>(_a[4]))); break;
        case 7: _t->CloseNotification((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1]))); break;
        case 8: _t->handleNotificationClosed((*reinterpret_cast<std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<uint>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (NotificationServer::*)(uint , const QString & , const QString & , const QString & , const QString & , int , const QVariantMap & )>(_a, &NotificationServer::newNotification, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (NotificationServer::*)(uint , uint )>(_a, &NotificationServer::closeRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (NotificationServer::*)(uint , uint )>(_a, &NotificationServer::NotificationClosed, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (NotificationServer::*)(uint , const QString & )>(_a, &NotificationServer::ActionInvoked, 3))
            return;
    }
}

const QMetaObject *NotificationServer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *NotificationServer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18NotificationServerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int NotificationServer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void NotificationServer::newNotification(uint _t1, const QString & _t2, const QString & _t3, const QString & _t4, const QString & _t5, int _t6, const QVariantMap & _t7)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3, _t4, _t5, _t6, _t7);
}

// SIGNAL 1
void NotificationServer::closeRequested(uint _t1, uint _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void NotificationServer::NotificationClosed(uint _t1, uint _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void NotificationServer::ActionInvoked(uint _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}
QT_WARNING_POP
