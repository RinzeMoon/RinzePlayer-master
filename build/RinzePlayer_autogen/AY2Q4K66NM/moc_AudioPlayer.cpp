/****************************************************************************
** Meta object code from reading C++ file 'AudioPlayer.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../Header/AudioPart/AudioPlay/AudioPlayer.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AudioPlayer.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11AudioPlayerE_t {};
} // unnamed namespace

template <> constexpr inline auto AudioPlayer::qt_create_metaobjectdata<qt_meta_tag_ZN11AudioPlayerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AudioPlayer",
        "stateChanged",
        "",
        "PlayState",
        "state",
        "positionChanged",
        "position",
        "progressUpdated",
        "progress",
        "int64_t",
        "currentMs",
        "totalMs",
        "errorOccurred",
        "errorMsg",
        "durationChanged",
        "duration",
        "playbackFinished",
        "currentSongFinished",
        "play",
        "filePath",
        "pause",
        "stop",
        "seek",
        "targetMs",
        "resume",
        "runPlayLoop",
        "emitProgressSignals"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'stateChanged'
        QtMocHelpers::SignalData<void(PlayState)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'positionChanged'
        QtMocHelpers::SignalData<void(qint64)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 6 },
        }}),
        // Signal 'progressUpdated'
        QtMocHelpers::SignalData<void(float, int64_t, int64_t)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Float, 8 }, { 0x80000000 | 9, 10 }, { 0x80000000 | 9, 11 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Signal 'durationChanged'
        QtMocHelpers::SignalData<void(quint64)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 15 },
        }}),
        // Signal 'playbackFinished'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'currentSongFinished'
        QtMocHelpers::SignalData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'play'
        QtMocHelpers::SlotData<bool(const QString &)>(18, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 19 },
        }}),
        // Slot 'pause'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'stop'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'seek'
        QtMocHelpers::SlotData<void(int64_t)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 23 },
        }}),
        // Slot 'resume'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'runPlayLoop'
        QtMocHelpers::SlotData<void(const QString &)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 19 },
        }}),
        // Slot 'emitProgressSignals'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AudioPlayer, qt_meta_tag_ZN11AudioPlayerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AudioPlayer::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11AudioPlayerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11AudioPlayerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11AudioPlayerE_t>.metaTypes,
    nullptr
} };

void AudioPlayer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AudioPlayer *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->stateChanged((*reinterpret_cast< std::add_pointer_t<PlayState>>(_a[1]))); break;
        case 1: _t->positionChanged((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 2: _t->progressUpdated((*reinterpret_cast< std::add_pointer_t<float>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[3]))); break;
        case 3: _t->errorOccurred((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->durationChanged((*reinterpret_cast< std::add_pointer_t<quint64>>(_a[1]))); break;
        case 5: _t->playbackFinished(); break;
        case 6: _t->currentSongFinished(); break;
        case 7: { bool _r = _t->play((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 8: _t->pause(); break;
        case 9: _t->stop(); break;
        case 10: _t->seek((*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[1]))); break;
        case 11: _t->resume(); break;
        case 12: _t->runPlayLoop((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->emitProgressSignals(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AudioPlayer::*)(PlayState )>(_a, &AudioPlayer::stateChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioPlayer::*)(qint64 )>(_a, &AudioPlayer::positionChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioPlayer::*)(float , int64_t , int64_t )>(_a, &AudioPlayer::progressUpdated, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioPlayer::*)(const QString & )>(_a, &AudioPlayer::errorOccurred, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioPlayer::*)(quint64 )>(_a, &AudioPlayer::durationChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioPlayer::*)()>(_a, &AudioPlayer::playbackFinished, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioPlayer::*)()>(_a, &AudioPlayer::currentSongFinished, 6))
            return;
    }
}

const QMetaObject *AudioPlayer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AudioPlayer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11AudioPlayerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AudioPlayer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 14;
    }
    return _id;
}

// SIGNAL 0
void AudioPlayer::stateChanged(PlayState _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void AudioPlayer::positionChanged(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void AudioPlayer::progressUpdated(float _t1, int64_t _t2, int64_t _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}

// SIGNAL 3
void AudioPlayer::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void AudioPlayer::durationChanged(quint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void AudioPlayer::playbackFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void AudioPlayer::currentSongFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP
