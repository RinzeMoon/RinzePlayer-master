/****************************************************************************
** Meta object code from reading C++ file 'AudioControlBar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../Header/Ui/AudioControlBar.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AudioControlBar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15AudioControlBarE_t {};
} // unnamed namespace

template <> constexpr inline auto AudioControlBar::qt_create_metaobjectdata<qt_meta_tag_ZN15AudioControlBarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AudioControlBar",
        "playListRequested",
        "",
        "volumeRequested",
        "value",
        "playPauseRequested",
        "prevRequested",
        "nextRequested",
        "progressSliderReleased",
        "playlistDlgCreated",
        "AudioPlaylistPopup*",
        "_m",
        "playlistDlgDestroyed",
        "clicked",
        "setPlayStatusToPlayList_Ui",
        "SongId",
        "sentCurrentSongMetaToUi",
        "SongMeta",
        "meta",
        "changeTitleScrollStateByPlayState",
        "onVolumeBtnClicked",
        "onVolumeSliderChanged",
        "onVolumeSliderReleased",
        "onPlayListClicked",
        "onVolumeButtonClicked",
        "onPlayPauseBtnClicked",
        "onPrevBtnClicked",
        "onNextBtnClicked",
        "onProgressSliderMoved",
        "onProgressSliderReleased",
        "onPlayDialogClicked",
        "showPopupNearby",
        "QWidget*",
        "target",
        "onModeButtonClicked",
        "onbtnListPlayClicked",
        "onbtnLoopClicked",
        "onbtnRandomClicked",
        "onProgressUpdated",
        "progress",
        "int64_t",
        "currentMs",
        "totalMs",
        "onProgressSliderPressed",
        "onProgressSliderReleasedInternal"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'playListRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'volumeRequested'
        QtMocHelpers::SignalData<void(int)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Signal 'playPauseRequested'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'prevRequested'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'nextRequested'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'progressSliderReleased'
        QtMocHelpers::SignalData<void(int)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Signal 'playlistDlgCreated'
        QtMocHelpers::SignalData<void(AudioPlaylistPopup *)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Signal 'playlistDlgDestroyed'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'clicked'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'setPlayStatusToPlayList_Ui'
        QtMocHelpers::SignalData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Signal 'sentCurrentSongMetaToUi'
        QtMocHelpers::SignalData<void(const SongMeta &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 17, 18 },
        }}),
        // Slot 'changeTitleScrollStateByPlayState'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onVolumeBtnClicked'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onVolumeSliderChanged'
        QtMocHelpers::SlotData<void(int)>(21, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Slot 'onVolumeSliderReleased'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPlayListClicked'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onVolumeButtonClicked'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPlayPauseBtnClicked'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPrevBtnClicked'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onNextBtnClicked'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProgressSliderMoved'
        QtMocHelpers::SlotData<void(int)>(28, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Slot 'onProgressSliderReleased'
        QtMocHelpers::SlotData<void(int)>(29, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Slot 'onPlayDialogClicked'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showPopupNearby'
        QtMocHelpers::SlotData<void(QWidget *)>(31, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 32, 33 },
        }}),
        // Slot 'onModeButtonClicked'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onbtnListPlayClicked'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onbtnLoopClicked'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onbtnRandomClicked'
        QtMocHelpers::SlotData<void()>(37, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProgressUpdated'
        QtMocHelpers::SlotData<void(float, int64_t, int64_t)>(38, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Float, 39 }, { 0x80000000 | 40, 41 }, { 0x80000000 | 40, 42 },
        }}),
        // Slot 'onProgressSliderPressed'
        QtMocHelpers::SlotData<void()>(43, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProgressSliderReleasedInternal'
        QtMocHelpers::SlotData<void()>(44, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AudioControlBar, qt_meta_tag_ZN15AudioControlBarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AudioControlBar::staticMetaObject = { {
    QMetaObject::SuperData::link<BaseControlBar::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AudioControlBarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AudioControlBarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15AudioControlBarE_t>.metaTypes,
    nullptr
} };

void AudioControlBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AudioControlBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->playListRequested(); break;
        case 1: _t->volumeRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->playPauseRequested(); break;
        case 3: _t->prevRequested(); break;
        case 4: _t->nextRequested(); break;
        case 5: _t->progressSliderReleased((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->playlistDlgCreated((*reinterpret_cast< std::add_pointer_t<AudioPlaylistPopup*>>(_a[1]))); break;
        case 7: _t->playlistDlgDestroyed(); break;
        case 8: _t->clicked(); break;
        case 9: _t->setPlayStatusToPlayList_Ui((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->sentCurrentSongMetaToUi((*reinterpret_cast< std::add_pointer_t<SongMeta>>(_a[1]))); break;
        case 11: _t->changeTitleScrollStateByPlayState(); break;
        case 12: _t->onVolumeBtnClicked(); break;
        case 13: _t->onVolumeSliderChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 14: _t->onVolumeSliderReleased(); break;
        case 15: _t->onPlayListClicked(); break;
        case 16: _t->onVolumeButtonClicked(); break;
        case 17: _t->onPlayPauseBtnClicked(); break;
        case 18: _t->onPrevBtnClicked(); break;
        case 19: _t->onNextBtnClicked(); break;
        case 20: _t->onProgressSliderMoved((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 21: _t->onProgressSliderReleased((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 22: _t->onPlayDialogClicked(); break;
        case 23: _t->showPopupNearby((*reinterpret_cast< std::add_pointer_t<QWidget*>>(_a[1]))); break;
        case 24: _t->onModeButtonClicked(); break;
        case 25: _t->onbtnListPlayClicked(); break;
        case 26: _t->onbtnLoopClicked(); break;
        case 27: _t->onbtnRandomClicked(); break;
        case 28: _t->onProgressUpdated((*reinterpret_cast< std::add_pointer_t<float>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[3]))); break;
        case 29: _t->onProgressSliderPressed(); break;
        case 30: _t->onProgressSliderReleasedInternal(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< AudioPlaylistPopup* >(); break;
            }
            break;
        case 23:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QWidget* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)()>(_a, &AudioControlBar::playListRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)(int )>(_a, &AudioControlBar::volumeRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)()>(_a, &AudioControlBar::playPauseRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)()>(_a, &AudioControlBar::prevRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)()>(_a, &AudioControlBar::nextRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)(int )>(_a, &AudioControlBar::progressSliderReleased, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)(AudioPlaylistPopup * )>(_a, &AudioControlBar::playlistDlgCreated, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)()>(_a, &AudioControlBar::playlistDlgDestroyed, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)()>(_a, &AudioControlBar::clicked, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)(const QString & )>(_a, &AudioControlBar::setPlayStatusToPlayList_Ui, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (AudioControlBar::*)(const SongMeta & )>(_a, &AudioControlBar::sentCurrentSongMetaToUi, 10))
            return;
    }
}

const QMetaObject *AudioControlBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AudioControlBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AudioControlBarE_t>.strings))
        return static_cast<void*>(this);
    return BaseControlBar::qt_metacast(_clname);
}

int AudioControlBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = BaseControlBar::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 31)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 31;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 31)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 31;
    }
    return _id;
}

// SIGNAL 0
void AudioControlBar::playListRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void AudioControlBar::volumeRequested(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void AudioControlBar::playPauseRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void AudioControlBar::prevRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void AudioControlBar::nextRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void AudioControlBar::progressSliderReleased(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void AudioControlBar::playlistDlgCreated(AudioPlaylistPopup * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void AudioControlBar::playlistDlgDestroyed()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void AudioControlBar::clicked()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void AudioControlBar::setPlayStatusToPlayList_Ui(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void AudioControlBar::sentCurrentSongMetaToUi(const SongMeta & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}
QT_WARNING_POP
