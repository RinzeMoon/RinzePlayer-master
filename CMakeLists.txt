cmake_minimum_required(VERSION 3.16)

project(RinzePlayer VERSION 0.1 LANGUAGES CXX)

# 简单校验依赖，参数为：名称，存放路径的变量
function(check_sdk_layout NAME PATH_VAR)
    set(ROOT_PATH "${${PATH_VAR}}")
    
    # 检查根路径是否存在
    if(NOT ROOT_PATH OR NOT EXISTS "${ROOT_PATH}")
        message(FATAL_ERROR 
            "\n[RinzePlayer 错误]: 未能找到 ${NAME} 根目录！\n请检查 FFmpeg 路径。\n")
    endif()

    # 检查必要的子目录
    set(SUB_DIRS "include" "lib" "bin")
    foreach(DIR ${SUB_DIRS})
        if(NOT EXISTS "${ROOT_PATH}/${DIR}")
            message(FATAL_ERROR 
                "\n[RinzePlayer 错误]: ${NAME} 目录结构完整性校验失败！\n"
                "在路径: ${ROOT_PATH} 下找不到关键子目录: ${DIR}。")
        endif()
    endforeach()

endfunction()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 FFmpeg
set(FFMPEG_FOUND FALSE)

# 1. 首先检查命令行参数 -DFFMPEG_DIR
if(DEFINED FFMPEG_DIR)
    message(STATUS "从命令行参数查找 FFmpeg: ${FFMPEG_DIR}")
# 2. 然后检查环境变量 FFMPEG_DIR
elseif(DEFINED ENV{FFMPEG_DIR})
    set(FFMPEG_DIR "$ENV{FFMPEG_DIR}")
    message(STATUS "从环境变量 FFMPEG_DIR 查找 FFmpeg: ${FFMPEG_DIR}")
# 3. 尝试在常见位置查找
elseif(EXISTS "C:/FFmpegLIB")
    set(FFMPEG_DIR "C:/FFmpegLIB")
    message(STATUS "在常见位置查找 FFmpeg: ${FFMPEG_DIR}")
elseif(EXISTS "D:/FFmpegLIB")
    set(FFMPEG_DIR "D:/FFmpegLIB")
    message(STATUS "在常见位置查找 FFmpeg: ${FFMPEG_DIR}")
elseif(EXISTS "C:/ffmpeg-8.0-full_build-shared")
    set(FFMPEG_DIR "C:/ffmpeg-8.0-full_build-shared")
    message(STATUS "在常见位置查找 FFmpeg: ${FFMPEG_DIR}")
elseif(EXISTS "D:/ffmpeg-8.0-full_build-shared")
    set(FFMPEG_DIR "D:/ffmpeg-8.0-full_build-shared")
    message(STATUS "在常见位置查找 FFmpeg: ${FFMPEG_DIR}")
endif()

# 如果找到了，验证目录结构
if(DEFINED FFMPEG_DIR)
    check_sdk_layout("FFmpeg" FFMPEG_DIR)
    
    # 设置 FFmpeg
    set(FFMPEG_INCLUDE_DIR ${FFMPEG_DIR}/include)
    set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/lib)
    set(FFMPEG_BIN_DIR ${FFMPEG_DIR}/bin)
    set(FFMPEG_FOUND TRUE)
else()
    message(FATAL_ERROR "\n[RinzePlayer 错误]: 请设置 FFMPEG_DIR 环境变量或通过 -DFFMPEG_DIR=... 传递！\n例如：cmake -DFFMPEG_DIR=\"D:/ffmpeg-8.0-full_build-shared\" ..\n")
endif()

# 查找 Vulkan（必需，即使没有 libplacebo）
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "✅ Vulkan 找到！")
else()
    message(STATUS "⚠️ Vulkan 未找到，Vulkan 功能将不可用")
endif()

# 尝试查找 libplacebo（如果可用）
find_package(libplacebo QUIET)
if(libplacebo_FOUND)
    if(Vulkan_FOUND)
        message(STATUS "✅ libplacebo 通过 CMake 配置找到！且 Vulkan 也找到！")
        set(LIBPLACEBO_AVAILABLE ON)
    else()
        message(STATUS "⚠️ libplacebo 通过 CMake 配置找到，但 Vulkan 未找到！libplacebo 需要 Vulkan 才能工作。")
        message(STATUS "⚠️ libplacebo 将不可用（仅使用 FFmpeg 滤镜）")
        set(LIBPLACEBO_AVAILABLE OFF)
    endif()
elseif(DEFINED ENV{LIBPLACEBO_DIR})
    # 如果设置了环境变量，尝试手动查找
    set(LIBPLACEBO_DIR "$ENV{LIBPLACEBO_DIR}")
    message(STATUS "从环境变量 LIBPLACEBO_DIR 手动查找: ${LIBPLACEBO_DIR}")
    
    set(LIBPLACEBO_INCLUDE_DIR "${LIBPLACEBO_DIR}/include")
    set(LIBPLACEBO_LIB_DIR "${LIBPLACEBO_DIR}/lib")
    set(LIBPLACEBO_BIN_DIR "${LIBPLACEBO_DIR}/bin")
    
    # 检查必要的文件
    set(LIBPLACEBO_FOUND_MANUALLY FALSE)
    if(EXISTS "${LIBPLACEBO_INCLUDE_DIR}/libplacebo/common.h")
        # 查找库文件
        set(LIBPLACEBO_LIB_NAMES "libplacebo" "placebo" "libplacebo.dll.a" "libplacebo.lib")
        foreach(LIB_NAME IN LISTS LIBPLACEBO_LIB_NAMES)
            if(EXISTS "${LIBPLACEBO_LIB_DIR}/${LIB_NAME}")
                set(LIBPLACEBO_LIB "${LIBPLACEBO_LIB_DIR}/${LIB_NAME}")
                set(LIBPLACEBO_FOUND_MANUALLY TRUE)
                message(STATUS "✅ 找到 libplacebo 库: ${LIBPLACEBO_LIB}")
                break()
            endif()
        endforeach()
    endif()
    
    if(LIBPLACEBO_FOUND_MANUALLY)
        if(Vulkan_FOUND)
            message(STATUS "✅ libplacebo 手动找到！且 Vulkan 也找到！将启用 libplacebo 直接 API")
            set(LIBPLACEBO_AVAILABLE ON)
            
            # 创建导入目标（即使没有 .cmake 配置文件）
            if(NOT TARGET libplacebo::libplacebo)
                add_library(libplacebo::libplacebo SHARED IMPORTED)
                set_target_properties(libplacebo::libplacebo PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${LIBPLACEBO_INCLUDE_DIR}"
                    IMPORTED_LOCATION "${LIBPLACEBO_LIB}"
                )
                # 也设置 DLL 位置（Windows）
                if(WIN32 AND EXISTS "${LIBPLACEBO_BIN_DIR}/libplacebo-362.dll")
                    set_target_properties(libplacebo::libplacebo PROPERTIES
                        IMPORTED_IMPLIB "${LIBPLACEBO_LIB}"
                        IMPORTED_LOCATION "${LIBPLACEBO_BIN_DIR}/libplacebo-362.dll"
                    )
                endif()
                message(STATUS "✅ 已手动创建 libplacebo::libplacebo 导入目标")
            endif()
        else()
            message(STATUS "⚠️ libplacebo 找到了，但 Vulkan 未找到！libplacebo 需要 Vulkan 才能工作。")
            message(STATUS "⚠️ libplacebo 将不可用（仅使用 FFmpeg 滤镜）")
            set(LIBPLACEBO_AVAILABLE OFF)
        endif()
    else()
        message(STATUS "⚠️ libplacebo 手动查找失败，请检查 LIBPLACEBO_DIR")
        set(LIBPLACEBO_AVAILABLE OFF)
    endif()
else()
    message(STATUS "⚠️ libplacebo 未找到，将不使用 libplacebo 直接 API（仅使用 FFmpeg 滤镜）")
    message(STATUS "ℹ️  提示：设置 LIBPLACEBO_DIR 环境变量可以手动指定 libplacebo 位置")
    set(LIBPLACEBO_AVAILABLE OFF)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGLWidgets
    WebEngineWidgets
    WebSockets
)

if (MSVC)
    add_compile_options(/W4)
else() # GCC/Clang 下
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

qt_add_executable(RinzePlayer
    main.cpp
)

target_sources(RinzePlayer
    PRIVATE
        demux/demux.h demux/demux.cpp
        decode/decodebase.h decode/decodebase.cpp
        decode/decodeaudio.h decode/decodeaudio.cpp
        decode/decodevideo.h decode/decodevideo.cpp
        decode/decodesubtitle.h decode/decodesubtitle.cpp
        renderer/audioplayer.h renderer/audioplayer.cpp
        renderer/videoplayer.h renderer/videoplayer.cpp
        renderer/renderdata.h renderer/renderdata.cpp
        clock/globalclock.h clock/globalclock.cpp
        core/mediacontroller.h core/mediacontroller.cpp
        ui/mainwindow.h ui/mainwindow.cpp
    ui/videowidget.h ui/videowidget.cpp
    ui/motionheatmap.h ui/motionheatmap.cpp
    ui/motiontrail.h ui/motiontrail.cpp
    ui/motionhistory.h ui/motionhistory.cpp
    ui/pipwindow.h ui/pipwindow.cpp
    ui/controlbar.h ui/controlbar.cpp
    ui/hovercontrolbar.h ui/hovercontrolbar.cpp
    ui/urlplaybackconfigdialog.h ui/urlplaybackconfigdialog.cpp
        compat/compat.h
        types/types.h
        types/ptrs.h
        utils/utils.h
        utils/dirtyrectmanager.h utils/dirtyrectmanager.cpp
        utils/AtomicDoubleBuffer.h
        utils/filehelper.h utils/filehelper.cpp
        utils/spscbuffer.h utils/spscbuffer.cpp
        utils/powermanager.h utils/powermanager.cpp
        playbackstats.h playbackstats.cpp
        3rdparty/miniaudio.h 3rdparty/miniaudio.cpp
        streaming/streaming_types.h
        streaming/exponential_backoff.h streaming/exponential_backoff.cpp
        streaming/debounce_manager.h streaming/debounce_manager.cpp
        streaming/stream_manager.h streaming/stream_manager.cpp
        visualizer/fftanalyzer.h visualizer/fftanalyzer.cpp
        visualizer/audiovisualwidget.h visualizer/audiovisualwidget.cpp
        audioeffects/audioeffect.h audioeffects/audioeffect.cpp
        audioeffects/audiomixer.h audioeffects/audiomixer.cpp
        audioeffects/effectfactory.h
        audioeffects/effects/equalizer.h audioeffects/effects/equalizer.cpp
        audioeffects/effects/reverb.h audioeffects/effects/reverb.cpp
        audioeffects/effects/graphiceq.h audioeffects/effects/graphiceq.cpp
        audioeffects/visualizers/effectvisualizer.h audioeffects/visualizers/effectvisualizer.cpp
        audioeffects/visualizers/equalizervisualizer.h audioeffects/visualizers/equalizervisualizer.cpp
        audioeffects/visualizers/reverbvisualizer.h audioeffects/visualizers/reverbvisualizer.cpp
        audioeffects/visualizers/graphiceqvisualizer.h audioeffects/visualizers/graphiceqvisualizer.cpp
        ui/effectspanel.h ui/effectspanel.cpp
        ui/reportdialog.h ui/reportdialog.cpp
        ui/reportgenerator.h ui/reportgenerator.cpp
        streaming/chatroom_client.h streaming/chatroom_client.cpp
        streaming/status_client.h streaming/status_client.cpp
        streaming/room_session.h streaming/room_session.cpp
        core/sync_manager.h core/sync_manager.cpp
        core/clock_aligner.h core/clock_aligner.cpp
        ui/chatwindow.h ui/chatwindow.cpp
        ui/danmakuoverlay.h ui/danmakuoverlay.cpp
        ui/readyoverlay.h ui/readyoverlay.cpp
        ui/roomdialog.h ui/roomdialog.cpp
)

# 资源文件
qt_add_resources(RinzePlayer "resources"
    PREFIX "/"
    FILES
        resource/shader.vert
        resource/shader.frag
        resource/motion_vector.vert
        resource/motion_vector.geom
        resource/motion_vector.frag
        resource/visualizer_spectrum.vert
        resource/visualizer_spectrum.frag
        resource/visualizer_particle.vert
        resource/visualizer_particle.frag
)

# 宏标志
target_compile_definitions(RinzePlayer PRIVATE
    MA_USE_STDINT # 让 miniaudio 使用标准整数类型
    $<$<BOOL:${LIBPLACEBO_AVAILABLE}>:LIBPLACEBO_AVAILABLE> # 如果找到 libplacebo，启用这个宏
)

set_target_properties(RinzePlayer PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE FALSE
)

# 添加头文件搜索路径
target_include_directories(RinzePlayer
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${FFMPEG_INCLUDE_DIR}
        $<$<BOOL:${Vulkan_FOUND}>:${Vulkan_INCLUDE_DIRS}>
        $<$<AND:$<NOT:$<BOOL:${libplacebo_FOUND}>>,$<BOOL:${LIBPLACEBO_AVAILABLE}>>:${LIBPLACEBO_INCLUDE_DIR}>
)

# 添加库文件搜索路径
target_link_directories(RinzePlayer
    PRIVATE
        ${FFMPEG_LIB_DIR}
        $<$<AND:$<NOT:$<BOOL:${libplacebo_FOUND}>>,$<BOOL:${LIBPLACEBO_AVAILABLE}>>:${LIBPLACEBO_LIB_DIR}>
)

target_link_libraries(RinzePlayer
    PRIVATE
        Qt6::Widgets
        Qt6::OpenGLWidgets
        Qt6::Gui
        Qt6::Core
        Qt6::WebEngineWidgets
        Qt6::WebSockets
        avcodec
        avformat
        avutil
        swscale
        swresample
        avfilter
        winmm
        $<$<BOOL:${Vulkan_FOUND}>:Vulkan::Vulkan>
        $<$<BOOL:${LIBPLACEBO_AVAILABLE}>:libplacebo::libplacebo> # 统一使用 libplacebo::libplacebo 目标
)

file(GLOB FFMPEG_DLLS
    "${FFMPEG_BIN_DIR}/avcodec-*.dll"
    "${FFMPEG_BIN_DIR}/avformat-*.dll"
    "${FFMPEG_BIN_DIR}/avutil-*.dll"
    "${FFMPEG_BIN_DIR}/swscale-*.dll"
    "${FFMPEG_BIN_DIR}/swresample-*.dll"
    "${FFMPEG_BIN_DIR}/avfilter-*.dll"
)

# 如果有 libplacebo DLL，也复制过去
if(LIBPLACEBO_AVAILABLE)
    if(DEFINED LIBPLACEBO_BIN_DIR)
        file(GLOB LIBPLACEBO_DLLS "${LIBPLACEBO_BIN_DIR}/*.dll")
    elseif(TARGET libplacebo::libplacebo)
        # 尝试从导入目标获取 DLL 位置
        get_target_property(LIBPLACEBO_DLL libplacebo::libplacebo IMPORTED_LOCATION)
        if(LIBPLACEBO_DLL)
            set(LIBPLACEBO_DLLS "${LIBPLACEBO_DLL}")
        endif()
    endif()
endif()

# 复制 FFmpeg 和 libplacebo DLL 到输出目录
add_custom_command(TARGET RinzePlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        ${LIBPLACEBO_DLLS}
        $<TARGET_FILE_DIR:RinzePlayer>
    COMMAND_EXPAND_LISTS
)

# ========================================
# Install
# ========================================
include(GNUInstallDirs)
install(TARGETS RinzePlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
