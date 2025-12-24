/****************************************************************************
** Meta object code from reading C++ file 'MusicSheetUI.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../Header/Ui/MusicSheetUI.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MusicSheetUI.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.2. It"
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
struct qt_meta_tag_ZN12MusicSheetUIE_t {};
} // unnamed namespace

template <> constexpr inline auto MusicSheetUI::qt_create_metaobjectdata<qt_meta_tag_ZN12MusicSheetUIE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MusicSheetUI",
        "item_stateChanged",
        "",
        "sheetId",
        "LabelName",
        "playSongById",
        "songId",
        "requestRemoveSongFromSheet",
        "requestAddSongToOtherSheet",
        "targetSheetId",
        "SongMeta",
        "song",
        "onSongPlayedUpdateUI_ById",
        "OnMoreBtnClicked",
        "onShowOperateMenu",
        "pos"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'item_stateChanged'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::QString, 4 },
        }}),
        // Signal 'playSongById'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 }, { QMetaType::QString, 3 },
        }}),
        // Signal 'requestRemoveSongFromSheet'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'requestAddSongToOtherSheet'
        QtMocHelpers::SignalData<void(const QString &, const SongMeta &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { 0x80000000 | 10, 11 },
        }}),
        // Slot 'onSongPlayedUpdateUI_ById'
        QtMocHelpers::SlotData<void(const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Slot 'OnMoreBtnClicked'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onShowOperateMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 15 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MusicSheetUI, qt_meta_tag_ZN12MusicSheetUIE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MusicSheetUI::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12MusicSheetUIE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12MusicSheetUIE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12MusicSheetUIE_t>.metaTypes,
    nullptr
} };

void MusicSheetUI::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MusicSheetUI *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->item_stateChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->playSongById((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->requestRemoveSongFromSheet((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->requestAddSongToOtherSheet((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<SongMeta>>(_a[2]))); break;
        case 4: _t->onSongPlayedUpdateUI_ById((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->OnMoreBtnClicked(); break;
        case 6: _t->onShowOperateMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MusicSheetUI::*)(const QString & , const QString & )>(_a, &MusicSheetUI::item_stateChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicSheetUI::*)(const QString & , const QString & )>(_a, &MusicSheetUI::playSongById, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicSheetUI::*)(const QString & , const QString & )>(_a, &MusicSheetUI::requestRemoveSongFromSheet, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicSheetUI::*)(const QString & , const SongMeta & )>(_a, &MusicSheetUI::requestAddSongToOtherSheet, 3))
            return;
    }
}

const QMetaObject *MusicSheetUI::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MusicSheetUI::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12MusicSheetUIE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int MusicSheetUI::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void MusicSheetUI::item_stateChanged(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void MusicSheetUI::playSongById(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void MusicSheetUI::requestRemoveSongFromSheet(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void MusicSheetUI::requestAddSongToOtherSheet(const QString & _t1, const SongMeta & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}
QT_WARNING_POP
