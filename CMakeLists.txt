cmake_minimum_required(VERSION 3.16)

project(RinzePlayer VERSION 0.1 LANGUAGES CXX)

# 基础编译配置
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 导出编译命令（供 IntelliSense 使用）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# set(CMAKE_BUILD_TYPE Debug)
# add_compile_options(-g -o0)

# -------------------------- FFmpeg 静态库配置 --------------------------
set(FFMPEG_PREFIX "/usr/local" CACHE PATH "FFmpeg 安装路径")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_PREFIX}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_PREFIX}/lib")

# FFmpeg 静态库列表（按「高层依赖底层」顺序，避免链接错误）
set(FFMPEG_STATIC_LIBS
        "${FFMPEG_LIB_DIR}/libavfilter.a"
        "${FFMPEG_LIB_DIR}/libavformat.a"
        "${FFMPEG_LIB_DIR}/libavcodec.a"
        "${FFMPEG_LIB_DIR}/libavdevice.a"
        "${FFMPEG_LIB_DIR}/libswresample.a"
        "${FFMPEG_LIB_DIR}/libswscale.a"
        "${FFMPEG_LIB_DIR}/libpostproc.a"
        "${FFMPEG_LIB_DIR}/libavutil.a"
)

# 验证 FFmpeg 静态库完整性
foreach(lib ${FFMPEG_STATIC_LIBS})
    if(NOT EXISTS "${lib}")
        message(FATAL_ERROR "FFmpeg 静态库缺失：${lib}\n解决方案：1. 重新编译 FFmpeg 并启用静态库；2. 通过 -DFFMPEG_PREFIX 指定正确路径")
    endif()
endforeach()

# -------------------------- 依赖库查找（纯 CMake 原生，无 pkg-config） --------------------------
# 1. Qt 库
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
        Widgets Multimedia MultimediaWidgets OpenGL Network Core Gui
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
        Widgets Multimedia MultimediaWidgets OpenGL Network Core Gui
)

# 2. SDL2
find_path(SDL2_INCLUDE_DIRS NAMES SDL2/SDL.h PATHS /usr/include /usr/local/include)
find_library(SDL2_LIBRARIES NAMES SDL2 PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

# 3. Opus
find_path(OPUS_INCLUDE_DIRS NAMES opus/opus.h PATHS /usr/include /usr/local/include)
find_library(OPUS_LIBRARIES NAMES opus PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

# 4. FDK-AAC
find_path(FDK_AAC_INCLUDE_DIRS NAMES fdk-aac/aacenc_lib.h PATHS /usr/include /usr/local/include)
find_library(FDK_AAC_LIBRARIES NAMES fdk-aac PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

# 5. MP3LAME（MP3 解码核心）
find_path(MP3LAME_INCLUDE_DIRS NAMES lame/lame.h PATHS /usr/include /usr/local/include)
find_library(MP3LAME_LIBRARIES NAMES mp3lame PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

# 6. VPX
find_path(VPX_INCLUDE_DIRS NAMES vpx/vpx_encoder.h PATHS /usr/include /usr/local/include)
find_library(VPX_LIBRARIES NAMES vpx PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

# 7. X264
find_path(X264_INCLUDE_DIRS NAMES x264.h PATHS /usr/include /usr/local/include /usr/include/x264)
find_library(X264_LIBRARIES NAMES x264 PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

# 8. X265
find_path(X265_INCLUDE_DIRS NAMES x265.h PATHS /usr/include /usr/local/include /usr/include/x265)
find_library(X265_LIBRARIES NAMES x265 PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

# -------------------------- 依赖完整性验证（修复语法错误：用 CMake 原生方式转小写） --------------------------
set(REQUIRED_DEPS SDL2 OPUS FDK_AAC MP3LAME VPX X264 X265)
foreach(dep ${REQUIRED_DEPS})

    # 步骤1：将依赖名转为小写（处理下划线）
    string(TOLOWER ${dep} dep_lower)
    # 步骤2：特殊处理 FDK_AAC（将下划线替换为连字符，符合 Ubuntu 包名规范）
    if(dep STREQUAL "FDK_AAC")
        set(dep_lower "fdk-aac")
    endif()
    # 步骤2：验证头文件
    if(NOT ${dep}_INCLUDE_DIRS)
        message(FATAL_ERROR "[依赖缺失] ${dep} 头文件未找到！\nUbuntu 安装命令：sudo apt install lib${dep_lower}-dev")
    endif()

    # 步骤3：验证库文件
    if(NOT ${dep}_LIBRARIES)
        message(FATAL_ERROR "[依赖缺失] ${dep} 库文件未找到！\nUbuntu 安装命令：sudo apt install lib${dep_lower}-dev")
    endif()
endforeach()

# -------------------------- 项目源文件列表 --------------------------
set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        Header/Ui/mainwindow.h
        src/mainwindow.ui
        src/BaseControlBar.cpp
        Header/Ui/BaseControlBar.h
        src/AudioControlBar.cpp
        Header/Ui/AudioControlBar.h
        src/MusicPlayerWidget.cpp
        Header/Ui/MusicPlayerWidget.h
        src/Audio/FFmpegAudioProcessor.cpp
        Header/AudioPart/FFmpegAudioProcessor.h
        src/Audio/AudioMetaParser.cpp
        Header/AudioPart/AudioMetaParser.h
        src/Audio/Decode/AudioDecoder.cpp
        Header/AudioPart/Decode/AudioDecoder.h
        Global/Global.h
        Header/Util.h
        src/Audio/AudioPlay/AudioPlayer.cpp
        Header/AudioPart/AudioPlay/AudioPlayer.h
        src/AudioPlayListWidget.cpp
        Header/Ui/AudioPlayListWidget.h
        src/Manager/PlaylistManager.cpp
        Header/Manager/PlaylistManager.h
        src/HomeWidget.cpp
        Header/Ui/HomeWidget.h
        src/Controller/PlayQueueController.cpp
        Header/Controller/PlayQueueController.h
        src/MusicSheetUI.cpp
        Header/Ui/MusicSheetUI.h
        src/Manager/MusicSheetManager.cpp
        Header/Manager/MusicSheetManager.h
        src/etc/SongMeta.cpp
        src/Clock/Clock.cpp
        Header/Clock/Clock.h
        src/Lrc/LyricParser.cpp
        Header/Lrc/LyricParser.h
        src/etc/TestHeader.h
        src/etc/s.cpp
)

# -------------------------- 生成可执行目标 --------------------------
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(RinzePlayer MANUAL_FINALIZATION ${PROJECT_SOURCES}
            src/Audio/AudioPlay/AudioPlayer.cpp
            Header/AudioPart/AudioPlay/AudioPlayer.h
            src/AudioPlayListWidget.cpp
            Header/Ui/AudioPlayListWidget.h
            src/Manager/PlaylistManager.cpp
            Header/Manager/PlaylistManager.h
            src/Controller/PlayQueueController.cpp
            Header/Controller/PlayQueueController.h
            src/MusicSheetUI.cpp
            Header/Ui/MusicSheetUI.h
            src/Clock/Clock.cpp
            Header/Clock/Clock.h
            src/Lrc/LyricParser.cpp
            Header/Lrc/LyricParser.h
            )
else()
    if(ANDROID)
        add_library(RinzePlayer SHARED ${PROJECT_SOURCES}
                src/Audio/AudioPlay/AudioPlayer.cpp
                Header/AudioPart/AudioPlay/AudioPlayer.h
                src/AudioPlayListWidget.cpp
                Header/Ui/AudioPlayListWidget.h
                src/Manager/PlaylistManager.cpp
                Header/Manager/PlaylistManager.h
                src/Controller/PlayQueueController.cpp
                Header/Controller/PlayQueueController.h
                src/MusicSheetUI.cpp
                Header/Ui/MusicSheetUI.h
                src/Clock/Clock.cpp
                Header/Clock/Clock.h
                src/Lrc/LyricParser.cpp
                Header/Lrc/LyricParser.h
        )
    else()
        add_executable(RinzePlayer ${PROJECT_SOURCES}
                src/Audio/AudioPlay/AudioPlayer.cpp
                Header/AudioPart/AudioPlay/AudioPlayer.h
                src/AudioPlayListWidget.cpp
                Header/Ui/AudioPlayListWidget.h
                src/Manager/PlaylistManager.cpp
                Header/Manager/PlaylistManager.h
                src/MusicSheetUI.cpp
                Header/Ui/MusicSheetUI.h
                src/Clock/Clock.cpp
                Header/Clock/Clock.h
                src/Lrc/LyricParser.cpp
                Header/Lrc/LyricParser.h
        )
    endif()
endif()

# -------------------------- 目标编译配置 --------------------------
# 1. 头文件路径
target_include_directories(RinzePlayer PRIVATE
        BEFORE
        ${FFMPEG_INCLUDE_DIR}
        ${SDL2_INCLUDE_DIRS}
        ${OPUS_INCLUDE_DIRS}
        ${FDK_AAC_INCLUDE_DIRS}
        ${MP3LAME_INCLUDE_DIRS}
        ${VPX_INCLUDE_DIRS}
        ${X264_INCLUDE_DIRS}
        ${X265_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/Header
        ${CMAKE_CURRENT_SOURCE_DIR}/Global
)

# 2. 编译选项
target_compile_options(RinzePlayer PRIVATE
        -D__STDC_CONSTANT_MACROS
        -D__STDC_LIMIT_MACROS
        -Wno-deprecated-declarations
        -Wall
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O2 -march=native>
)

# 3. 链接依赖（顺序正确）
target_link_libraries(RinzePlayer PRIVATE
        # Qt 库
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::MultimediaWidgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui

        # FFmpeg 静态库
        ${FFMPEG_STATIC_LIBS}

        # 编解码器库
        ${FDK_AAC_LIBRARIES}
        ${MP3LAME_LIBRARIES}
        ${OPUS_LIBRARIES}
        ${VPX_LIBRARIES}
        ${X264_LIBRARIES}
        ${X265_LIBRARIES}

        # 基础库
        ${SDL2_LIBRARIES}

        # 系统库
        X11 atomic m z pthread dl va va-drm va-x11 lzma bz2
)

# -------------------------- 配置验证输出 --------------------------
message(STATUS "================================ 构建配置验证 ================================")
message(STATUS "项目名称：${PROJECT_NAME}")
message(STATUS "Qt 版本：${Qt${QT_VERSION_MAJOR}_VERSION}")
message(STATUS "FFmpeg 路径：${FFMPEG_PREFIX}")
message(STATUS "MP3LAME 头文件：${MP3LAME_INCLUDE_DIRS}")
message(STATUS "MP3LAME 库文件：${MP3LAME_LIBRARIES}")
message(STATUS "构建模式：${CMAKE_BUILD_TYPE}")
message(STATUS "==========================================================================")

# -------------------------- 跨平台配置 --------------------------
set_target_properties(RinzePlayer PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER com.example.RinzePlayer
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(RinzePlayer)
endif()