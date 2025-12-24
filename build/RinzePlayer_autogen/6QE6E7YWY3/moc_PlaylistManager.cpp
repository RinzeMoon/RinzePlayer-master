/****************************************************************************
** Meta object code from reading C++ file 'PlaylistManager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../Header/Manager/PlaylistManager.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PlaylistManager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15PlaylistManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto PlaylistManager::qt_create_metaobjectdata<qt_meta_tag_ZN15PlaylistManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "PlaylistManager",
        "playlistUpdated",
        "",
        "playlistRefreshed",
        "songPlaylistUpdated",
        "QList<SongMeta>",
        "playlist",
        "songPlaylistCleared",
        "songAdded",
        "SongMeta",
        "song",
        "index",
        "songRemoved",
        "songRemovedById",
        "songId",
        "songsCleared",
        "playlistReplaced",
        "newList"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'playlistUpdated'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'playlistRefreshed'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'songPlaylistUpdated'
        QtMocHelpers::SignalData<void(const QList<SongMeta> &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'songPlaylistCleared'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'songAdded'
        QtMocHelpers::SignalData<void(const SongMeta &, int)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 }, { QMetaType::Int, 11 },
        }}),
        // Signal 'songRemoved'
        QtMocHelpers::SignalData<void(int)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 11 },
        }}),
        // Signal 'songRemovedById'
        QtMocHelpers::SignalData<void(const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 },
        }}),
        // Signal 'songsCleared'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'playlistReplaced'
        QtMocHelpers::SignalData<void(const QList<SongMeta> &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 17 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PlaylistManager, qt_meta_tag_ZN15PlaylistManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject PlaylistManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15PlaylistManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15PlaylistManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15PlaylistManagerE_t>.metaTypes,
    nullptr
} };

void PlaylistManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PlaylistManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->playlistUpdated(); break;
        case 1: _t->playlistRefreshed(); break;
        case 2: _t->songPlaylistUpdated((*reinterpret_cast< std::add_pointer_t<QList<SongMeta>>>(_a[1]))); break;
        case 3: _t->songPlaylistCleared(); break;
        case 4: _t->songAdded((*reinterpret_cast< std::add_pointer_t<SongMeta>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 5: _t->songRemoved((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->songRemovedById((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->songsCleared(); break;
        case 8: _t->playlistReplaced((*reinterpret_cast< std::add_pointer_t<QList<SongMeta>>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (PlaylistManager::*)()>(_a, &PlaylistManager::playlistUpdated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaylistManager::*)()>(_a, &PlaylistManager::playlistRefreshed, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaylistManager::*)(const QList<SongMeta> & )>(_a, &PlaylistManager::songPlaylistUpdated, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaylistManager::*)()>(_a, &PlaylistManager::songPlaylistCleared, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaylistManager::*)(const SongMeta & , int )>(_a, &PlaylistManager::songAdded, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaylistManager::*)(int )>(_a, &PlaylistManager::songRemoved, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaylistManager::*)(const QString & )>(_a, &PlaylistManager::songRemovedById, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaylistManager::*)()>(_a, &PlaylistManager::songsCleared, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaylistManager::*)(const QList<SongMeta> & )>(_a, &PlaylistManager::playlistReplaced, 8))
            return;
    }
}

const QMetaObject *PlaylistManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PlaylistManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15PlaylistManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int PlaylistManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void PlaylistManager::playlistUpdated()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void PlaylistManager::playlistRefreshed()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void PlaylistManager::songPlaylistUpdated(const QList<SongMeta> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void PlaylistManager::songPlaylistCleared()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void PlaylistManager::songAdded(const SongMeta & _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void PlaylistManager::songRemoved(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void PlaylistManager::songRemovedById(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void PlaylistManager::songsCleared()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void PlaylistManager::playlistReplaced(const QList<SongMeta> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}
QT_WARNING_POP
