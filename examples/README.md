# RinzePlayer - Vulkan &amp; FFmpeg 示例代码

## 快速开始（构建和运行）

```bash
# 进入 examples 目录
cd d:\TraeProject\RinzePlayer\examples

# 创建构建目录
mkdir build
cd build

# 配置 CMake（如果 FFmpeg 不在默认位置，使用 -DFFMPEG_DIR 指定）
cmake .. -DFFMPEG_DIR="D:/ffmpeg-8.0-full_build-shared"

# 编译
cmake --build . --config Release

# 运行
Release\feature_check.exe
```

---

## 文件说明

本文件夹包含三个需求的完整示例代码：

### 需求 A - setupFilter 函数示例
- `example_setupfilter.h` / `example_setupfilter.cpp`
- 功能：
  - 使用 `av_hwdevice_ctx_create` 创建 Vulkan 设备
  - 将设备关联到 `m_filterGraph-&gt;hw_device_ctx`
  - 在滤镜链中插入 `hwupload` 滤镜
  - 保留原有 libplacebo 参数
  - 完整的错误处理和日志输出

### 需求 B - FFmpeg 功能检查
- `ffmpeg_feature_check.h` / `ffmpeg_feature_check.cpp`
- 功能：
  - 检查 `av_hwdevice_ctx_create` 是否可用
  - 检查 `libplacebo` 滤镜是否可用
  - 检查 `hwupload` 滤镜是否可用

### 需求 C - Vulkan 运行时检测
- `vulkan_check.h` / `vulkan_check.cpp`
- 功能：
  - 检查 `vulkan-1.dll` (Windows) 或 `libvulkan.so.1` (Linux) 是否存在
  - 不依赖 Qt

### 测试程序
- `example_main.cpp` - 综合演示如何使用上述所有功能

## 编译说明

### Windows (使用 MSVC)
```bash
# 需要链接以下库：
# - avfilter
# - avutil
# - avcodec (可选)

cl example_main.cpp ffmpeg_feature_check.cpp vulkan_check.cpp ^
   /I"path\to\ffmpeg\include" ^
   /link /LIBPATH:"path\to\ffmpeg\lib" ^
   avfilter.lib avutil.lib
```

### Linux/macOS (使用 GCC)
```bash
g++ example_main.cpp ffmpeg_feature_check.cpp vulkan_check.cpp \
    -I/path/to/ffmpeg/include \
    -L/path/to/ffmpeg/lib \
    -lavfilter -lavutil \
    -o feature_check
```

## 使用示例

```cpp
#include "ffmpeg_feature_check.h"
#include "vulkan_check.h"

// 检查 Vulkan 运行时
VulkanStatus vkStatus = checkVulkanRuntime();
if (vkStatus.dllLoaded) {
    // Vulkan 可用
}

// 检查 FFmpeg 功能
FFmpegFeatureStatus ffmpegStatus = checkFFmpegFeatures();
if (ffmpegStatus.hasLibplaceboFilter) {
    // libplacebo 可用
}
```

## 滤镜链说明
```
buffer → hwupload → libplacebo → format → buffersink
  |         |            |          |          |
输入帧  上传到GPU  HDR色调映射  格式转换  输出RGBA
```
