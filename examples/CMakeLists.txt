cmake_minimum_required(VERSION 3.16)

project(RinzePlayerExamples VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找 FFmpeg
set(FFMPEG_FOUND FALSE)

if(DEFINED FFMPEG_DIR)
    message(STATUS "从命令行参数查找 FFmpeg: ${FFMPEG_DIR}")
elseif(DEFINED ENV{FFMPEG_DIR})
    set(FFMPEG_DIR "$ENV{FFMPEG_DIR}")
    message(STATUS "从环境变量 FFMPEG_DIR 查找 FFmpeg: ${FFMPEG_DIR}")
elseif(EXISTS "C:/ffmpeg-8.0-full_build-shared")
    set(FFMPEG_DIR "C:/ffmpeg-8.0-full_build-shared")
    message(STATUS "在常见位置查找 FFmpeg: ${FFMPEG_DIR}")
elseif(EXISTS "D:/ffmpeg-8.0-full_build-shared")
    set(FFMPEG_DIR "D:/ffmpeg-8.0-full_build-shared")
    message(STATUS "在常见位置查找 FFmpeg: ${FFMPEG_DIR}")
endif()

if(DEFINED FFMPEG_DIR)
    set(FFMPEG_INCLUDE_DIR ${FFMPEG_DIR}/include)
    set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/lib)
    set(FFMPEG_BIN_DIR ${FFMPEG_DIR}/bin)
    set(FFMPEG_FOUND TRUE)
else()
    message(FATAL_ERROR "\n请设置 FFMPEG_DIR 环境变量或通过 -DFFMPEG_DIR=... 传递！\n例如：cmake -DFFMPEG_DIR=\"D:/ffmpeg-8.0-full_build-shared\" ..\n")
endif()

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(feature_check
    example_main.cpp
    ffmpeg_feature_check.cpp
    vulkan_check.cpp
)

target_include_directories(feature_check
    PRIVATE
        ${CMAKE_SOURCE_DIR}/..
        ${FFMPEG_INCLUDE_DIR}
)

target_link_directories(feature_check
    PRIVATE
        ${FFMPEG_LIB_DIR}
)

target_link_libraries(feature_check
    PRIVATE
        avfilter
        avutil
)

file(GLOB FFMPEG_DLLS
    "${FFMPEG_BIN_DIR}/avcodec-*.dll"
    "${FFMPEG_BIN_DIR}/avformat-*.dll"
    "${FFMPEG_BIN_DIR}/avutil-*.dll"
    "${FFMPEG_BIN_DIR}/swscale-*.dll"
    "${FFMPEG_BIN_DIR}/swresample-*.dll"
    "${FFMPEG_BIN_DIR}/avfilter-*.dll"
)

add_custom_command(TARGET feature_check POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        $<TARGET_FILE_DIR:feature_check>
    COMMAND_EXPAND_LISTS
)

message(STATUS "\n配置完成！\n构建：cmake --build . --config Release\n运行：Release/feature_check.exe\n")
