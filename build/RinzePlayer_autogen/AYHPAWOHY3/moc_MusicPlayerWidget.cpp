/****************************************************************************
** Meta object code from reading C++ file 'MusicPlayerWidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../Header/Ui/MusicPlayerWidget.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MusicPlayerWidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN17MusicPlayerWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto MusicPlayerWidget::qt_create_metaobjectdata<qt_meta_tag_ZN17MusicPlayerWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MusicPlayerWidget",
        "playRequested",
        "",
        "pauseRequested",
        "stopRequested",
        "seekRequested",
        "position",
        "prevRequested",
        "nextRequested",
        "lyricClicked",
        "currentSongChanged",
        "SongMeta",
        "songInfo",
        "lyricsLoaded",
        "success",
        "currentLyricChanged",
        "LrcLine::LyricLine",
        "lyric",
        "positionChanged",
        "playStateChanged",
        "isPlaying",
        "coverChanged",
        "cover",
        "errorOccurred",
        "errorMsg",
        "onMusicLoaded",
        "info",
        "onLyricsLoaded",
        "filePath",
        "onPositionChanged",
        "onPlayStateChanged",
        "onCoverLoaded",
        "onErrorOccurred",
        "currentSongUpdated",
        "songMeta",
        "onLyricItemClicked",
        "QListWidgetItem*",
        "item",
        "onClockTimeChanged",
        "time",
        "updateUIFromClock"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'playRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'pauseRequested'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'stopRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'seekRequested'
        QtMocHelpers::SignalData<void(qint64)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 6 },
        }}),
        // Signal 'prevRequested'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'nextRequested'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'lyricClicked'
        QtMocHelpers::SignalData<void(qint64)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 6 },
        }}),
        // Signal 'currentSongChanged'
        QtMocHelpers::SignalData<void(const SongMeta &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 12 },
        }}),
        // Signal 'lyricsLoaded'
        QtMocHelpers::SignalData<void(bool)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 14 },
        }}),
        // Signal 'currentLyricChanged'
        QtMocHelpers::SignalData<void(const LrcLine::LyricLine &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Signal 'positionChanged'
        QtMocHelpers::SignalData<void(qint64)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 6 },
        }}),
        // Signal 'playStateChanged'
        QtMocHelpers::SignalData<void(bool)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 20 },
        }}),
        // Signal 'coverChanged'
        QtMocHelpers::SignalData<void(const QPixmap &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QPixmap, 22 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 24 },
        }}),
        // Slot 'onMusicLoaded'
        QtMocHelpers::SlotData<void(const SongMeta &)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 26 },
        }}),
        // Slot 'onLyricsLoaded'
        QtMocHelpers::SlotData<void(const QString &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 28 },
        }}),
        // Slot 'onPositionChanged'
        QtMocHelpers::SlotData<void(qint64)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 6 },
        }}),
        // Slot 'onPlayStateChanged'
        QtMocHelpers::SlotData<void(bool)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 20 },
        }}),
        // Slot 'onCoverLoaded'
        QtMocHelpers::SlotData<void(const QPixmap &)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QPixmap, 22 },
        }}),
        // Slot 'onErrorOccurred'
        QtMocHelpers::SlotData<void(const QString &)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 24 },
        }}),
        // Slot 'currentSongUpdated'
        QtMocHelpers::SlotData<void(const SongMeta &)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 34 },
        }}),
        // Slot 'onLyricItemClicked'
        QtMocHelpers::SlotData<void(QListWidgetItem *)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 36, 37 },
        }}),
        // Slot 'onClockTimeChanged'
        QtMocHelpers::SlotData<void(double)>(38, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Double, 39 },
        }}),
        // Slot 'updateUIFromClock'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MusicPlayerWidget, qt_meta_tag_ZN17MusicPlayerWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MusicPlayerWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17MusicPlayerWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17MusicPlayerWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17MusicPlayerWidgetE_t>.metaTypes,
    nullptr
} };

void MusicPlayerWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MusicPlayerWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->playRequested(); break;
        case 1: _t->pauseRequested(); break;
        case 2: _t->stopRequested(); break;
        case 3: _t->seekRequested((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 4: _t->prevRequested(); break;
        case 5: _t->nextRequested(); break;
        case 6: _t->lyricClicked((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 7: _t->currentSongChanged((*reinterpret_cast< std::add_pointer_t<SongMeta>>(_a[1]))); break;
        case 8: _t->lyricsLoaded((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 9: _t->currentLyricChanged((*reinterpret_cast< std::add_pointer_t<LrcLine::LyricLine>>(_a[1]))); break;
        case 10: _t->positionChanged((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 11: _t->playStateChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 12: _t->coverChanged((*reinterpret_cast< std::add_pointer_t<QPixmap>>(_a[1]))); break;
        case 13: _t->errorOccurred((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->onMusicLoaded((*reinterpret_cast< std::add_pointer_t<SongMeta>>(_a[1]))); break;
        case 15: _t->onLyricsLoaded((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->onPositionChanged((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 17: _t->onPlayStateChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 18: _t->onCoverLoaded((*reinterpret_cast< std::add_pointer_t<QPixmap>>(_a[1]))); break;
        case 19: _t->onErrorOccurred((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 20: _t->currentSongUpdated((*reinterpret_cast< std::add_pointer_t<SongMeta>>(_a[1]))); break;
        case 21: _t->onLyricItemClicked((*reinterpret_cast< std::add_pointer_t<QListWidgetItem*>>(_a[1]))); break;
        case 22: _t->onClockTimeChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 23: _t->updateUIFromClock(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 9:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< LrcLine::LyricLine >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)()>(_a, &MusicPlayerWidget::playRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)()>(_a, &MusicPlayerWidget::pauseRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)()>(_a, &MusicPlayerWidget::stopRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)(qint64 )>(_a, &MusicPlayerWidget::seekRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)()>(_a, &MusicPlayerWidget::prevRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)()>(_a, &MusicPlayerWidget::nextRequested, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)(qint64 )>(_a, &MusicPlayerWidget::lyricClicked, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)(const SongMeta & )>(_a, &MusicPlayerWidget::currentSongChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)(bool )>(_a, &MusicPlayerWidget::lyricsLoaded, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)(const LrcLine::LyricLine & )>(_a, &MusicPlayerWidget::currentLyricChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)(qint64 )>(_a, &MusicPlayerWidget::positionChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)(bool )>(_a, &MusicPlayerWidget::playStateChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)(const QPixmap & )>(_a, &MusicPlayerWidget::coverChanged, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (MusicPlayerWidget::*)(const QString & )>(_a, &MusicPlayerWidget::errorOccurred, 13))
            return;
    }
}

const QMetaObject *MusicPlayerWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MusicPlayerWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17MusicPlayerWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int MusicPlayerWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 24)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 24;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 24)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 24;
    }
    return _id;
}

// SIGNAL 0
void MusicPlayerWidget::playRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void MusicPlayerWidget::pauseRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void MusicPlayerWidget::stopRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void MusicPlayerWidget::seekRequested(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void MusicPlayerWidget::prevRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void MusicPlayerWidget::nextRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void MusicPlayerWidget::lyricClicked(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void MusicPlayerWidget::currentSongChanged(const SongMeta & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void MusicPlayerWidget::lyricsLoaded(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void MusicPlayerWidget::currentLyricChanged(const LrcLine::LyricLine & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void MusicPlayerWidget::positionChanged(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void MusicPlayerWidget::playStateChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1);
}

// SIGNAL 12
void MusicPlayerWidget::coverChanged(const QPixmap & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1);
}

// SIGNAL 13
void MusicPlayerWidget::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1);
}
QT_WARNING_POP
