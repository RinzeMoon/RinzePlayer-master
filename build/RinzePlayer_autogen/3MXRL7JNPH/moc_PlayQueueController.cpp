/****************************************************************************
** Meta object code from reading C++ file 'PlayQueueController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../Header/Controller/PlayQueueController.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PlayQueueController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19PlayQueueControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto PlayQueueController::qt_create_metaobjectdata<qt_meta_tag_ZN19PlayQueueControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "PlayQueueController",
        "currentSongChanged",
        "",
        "SongMeta",
        "song",
        "playbackStateChanged",
        "PlayState",
        "state",
        "positionChangedToUi",
        "position",
        "durationChanged",
        "duration",
        "playQueueUpdated",
        "QList<SongMeta>",
        "queue",
        "coverChanged",
        "cover",
        "sendCurrentSongMeta",
        "sMeta",
        "progressUpdated",
        "progress",
        "int64_t",
        "currentMs",
        "totalMs",
        "errorOccurred",
        "errorMsg",
        "playbackFinished",
        "currentSongFinished",
        "onPlaylistUpdated",
        "playlist",
        "onAudioPositionChanged",
        "onAudioDurationChanged",
        "onAudioStateChanged",
        "onSongAdded",
        "index",
        "onSongRemoved",
        "onSongsCleared",
        "onPlaylistReplaced",
        "newList",
        "onAudioPlayerProgressUpdated",
        "onAudioPlayerErrorOccurred",
        "onAudioPlayerPlaybackFinished",
        "onAudioPlayerCurrentSongFinished",
        "onCurrentPositonChanged",
        "positon"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'currentSongChanged'
        QtMocHelpers::SignalData<void(const SongMeta &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'playbackStateChanged'
        QtMocHelpers::SignalData<void(PlayState)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'positionChangedToUi'
        QtMocHelpers::SignalData<void(qint64)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 9 },
        }}),
        // Signal 'durationChanged'
        QtMocHelpers::SignalData<void(qint64)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 11 },
        }}),
        // Signal 'playQueueUpdated'
        QtMocHelpers::SignalData<void(const QList<SongMeta> &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Signal 'coverChanged'
        QtMocHelpers::SignalData<void(const QPixmap &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QPixmap, 16 },
        }}),
        // Signal 'sendCurrentSongMeta'
        QtMocHelpers::SignalData<void(const SongMeta &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 18 },
        }}),
        // Signal 'progressUpdated'
        QtMocHelpers::SignalData<void(float, int64_t, int64_t)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Float, 20 }, { 0x80000000 | 21, 22 }, { 0x80000000 | 21, 23 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 25 },
        }}),
        // Signal 'playbackFinished'
        QtMocHelpers::SignalData<void()>(26, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'currentSongFinished'
        QtMocHelpers::SignalData<void()>(27, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onPlaylistUpdated'
        QtMocHelpers::SlotData<void(const QList<SongMeta> &)>(28, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 13, 29 },
        }}),
        // Slot 'onAudioPositionChanged'
        QtMocHelpers::SlotData<void(qint64)>(30, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::LongLong, 9 },
        }}),
        // Slot 'onAudioDurationChanged'
        QtMocHelpers::SlotData<void(qint64)>(31, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::LongLong, 11 },
        }}),
        // Slot 'onAudioStateChanged'
        QtMocHelpers::SlotData<void(PlayState)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Slot 'onSongAdded'
        QtMocHelpers::SlotData<void(const SongMeta &, int)>(33, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Int, 34 },
        }}),
        // Slot 'onSongRemoved'
        QtMocHelpers::SlotData<void(int)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 34 },
        }}),
        // Slot 'onSongsCleared'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPlaylistReplaced'
        QtMocHelpers::SlotData<void(const QList<SongMeta> &)>(37, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 13, 38 },
        }}),
        // Slot 'onAudioPlayerProgressUpdated'
        QtMocHelpers::SlotData<void(float, int64_t, int64_t)>(39, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Float, 20 }, { 0x80000000 | 21, 22 }, { 0x80000000 | 21, 23 },
        }}),
        // Slot 'onAudioPlayerErrorOccurred'
        QtMocHelpers::SlotData<void(const QString &)>(40, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 25 },
        }}),
        // Slot 'onAudioPlayerPlaybackFinished'
        QtMocHelpers::SlotData<void()>(41, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAudioPlayerCurrentSongFinished'
        QtMocHelpers::SlotData<void()>(42, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCurrentPositonChanged'
        QtMocHelpers::SlotData<void(qint64)>(43, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::LongLong, 44 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PlayQueueController, qt_meta_tag_ZN19PlayQueueControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject PlayQueueController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19PlayQueueControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19PlayQueueControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19PlayQueueControllerE_t>.metaTypes,
    nullptr
} };

void PlayQueueController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PlayQueueController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->currentSongChanged((*reinterpret_cast< std::add_pointer_t<SongMeta>>(_a[1]))); break;
        case 1: _t->playbackStateChanged((*reinterpret_cast< std::add_pointer_t<PlayState>>(_a[1]))); break;
        case 2: _t->positionChangedToUi((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 3: _t->durationChanged((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 4: _t->playQueueUpdated((*reinterpret_cast< std::add_pointer_t<QList<SongMeta>>>(_a[1]))); break;
        case 5: _t->coverChanged((*reinterpret_cast< std::add_pointer_t<QPixmap>>(_a[1]))); break;
        case 6: _t->sendCurrentSongMeta((*reinterpret_cast< std::add_pointer_t<SongMeta>>(_a[1]))); break;
        case 7: _t->progressUpdated((*reinterpret_cast< std::add_pointer_t<float>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[3]))); break;
        case 8: _t->errorOccurred((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->playbackFinished(); break;
        case 10: _t->currentSongFinished(); break;
        case 11: _t->onPlaylistUpdated((*reinterpret_cast< std::add_pointer_t<QList<SongMeta>>>(_a[1]))); break;
        case 12: _t->onAudioPositionChanged((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 13: _t->onAudioDurationChanged((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 14: _t->onAudioStateChanged((*reinterpret_cast< std::add_pointer_t<PlayState>>(_a[1]))); break;
        case 15: _t->onSongAdded((*reinterpret_cast< std::add_pointer_t<SongMeta>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 16: _t->onSongRemoved((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 17: _t->onSongsCleared(); break;
        case 18: _t->onPlaylistReplaced((*reinterpret_cast< std::add_pointer_t<QList<SongMeta>>>(_a[1]))); break;
        case 19: _t->onAudioPlayerProgressUpdated((*reinterpret_cast< std::add_pointer_t<float>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[3]))); break;
        case 20: _t->onAudioPlayerErrorOccurred((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 21: _t->onAudioPlayerPlaybackFinished(); break;
        case 22: _t->onAudioPlayerCurrentSongFinished(); break;
        case 23: _t->onCurrentPositonChanged((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)(const SongMeta & )>(_a, &PlayQueueController::currentSongChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)(PlayState )>(_a, &PlayQueueController::playbackStateChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)(qint64 )>(_a, &PlayQueueController::positionChangedToUi, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)(qint64 )>(_a, &PlayQueueController::durationChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)(const QList<SongMeta> & )>(_a, &PlayQueueController::playQueueUpdated, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)(const QPixmap & )>(_a, &PlayQueueController::coverChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)(const SongMeta & )>(_a, &PlayQueueController::sendCurrentSongMeta, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)(float , int64_t , int64_t )>(_a, &PlayQueueController::progressUpdated, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)(const QString & )>(_a, &PlayQueueController::errorOccurred, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)()>(_a, &PlayQueueController::playbackFinished, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlayQueueController::*)()>(_a, &PlayQueueController::currentSongFinished, 10))
            return;
    }
}

const QMetaObject *PlayQueueController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PlayQueueController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19PlayQueueControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int PlayQueueController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 24)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 24;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 24)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 24;
    }
    return _id;
}

// SIGNAL 0
void PlayQueueController::currentSongChanged(const SongMeta & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void PlayQueueController::playbackStateChanged(PlayState _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void PlayQueueController::positionChangedToUi(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void PlayQueueController::durationChanged(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void PlayQueueController::playQueueUpdated(const QList<SongMeta> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void PlayQueueController::coverChanged(const QPixmap & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void PlayQueueController::sendCurrentSongMeta(const SongMeta & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void PlayQueueController::progressUpdated(float _t1, int64_t _t2, int64_t _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2, _t3);
}

// SIGNAL 8
void PlayQueueController::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void PlayQueueController::playbackFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void PlayQueueController::currentSongFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}
QT_WARNING_POP
