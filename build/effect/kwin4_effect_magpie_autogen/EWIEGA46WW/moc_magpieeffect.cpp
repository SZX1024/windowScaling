/****************************************************************************
** Meta object code from reading C++ file 'magpieeffect.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../effect/magpieeffect.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'magpieeffect.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
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
struct qt_meta_tag_ZN4KWin12MagpieEffectE_t {};
} // unnamed namespace

template <> constexpr inline auto KWin::MagpieEffect::qt_create_metaobjectdata<qt_meta_tag_ZN4KWin12MagpieEffectE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KWin::MagpieEffect",
        "toggle",
        "",
        "handleWindowDeleted",
        "KWin::EffectWindow*",
        "w",
        "slotWindowClosed"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'toggle'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'handleWindowDeleted'
        QtMocHelpers::SlotData<void(KWin::EffectWindow *)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
        // Slot 'slotWindowClosed'
        QtMocHelpers::SlotData<void(KWin::EffectWindow *)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MagpieEffect, qt_meta_tag_ZN4KWin12MagpieEffectE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KWin::MagpieEffect::staticMetaObject = { {
    QMetaObject::SuperData::link<OffscreenEffect::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN4KWin12MagpieEffectE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN4KWin12MagpieEffectE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN4KWin12MagpieEffectE_t>.metaTypes,
    nullptr
} };

void KWin::MagpieEffect::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MagpieEffect *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->toggle(); break;
        case 1: _t->handleWindowDeleted((*reinterpret_cast<std::add_pointer_t<KWin::EffectWindow*>>(_a[1]))); break;
        case 2: _t->slotWindowClosed((*reinterpret_cast<std::add_pointer_t<KWin::EffectWindow*>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< KWin::EffectWindow* >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< KWin::EffectWindow* >(); break;
            }
            break;
        }
    }
}

const QMetaObject *KWin::MagpieEffect::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KWin::MagpieEffect::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN4KWin12MagpieEffectE_t>.strings))
        return static_cast<void*>(this);
    return OffscreenEffect::qt_metacast(_clname);
}

int KWin::MagpieEffect::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = OffscreenEffect::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}
QT_WARNING_POP
