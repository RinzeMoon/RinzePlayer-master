# 流媒体播放器 - RinzePlayer

[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-blue.svg)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)\
[![FFmpeg](https://img.shields.io/badge/FFmpeg-4.4+-green.svg)](https://ffmpeg.org/)
[![Qt](https://img.shields.io/badge/Qt-6.9.2-brightgreen.svg)](https://www.qt.io/)

## 📖 项目简介

**模块化流媒体播放引擎**是一个基于 C++17、FFmpeg 和 SDL2 的跨平台音视频播放器。其内部组件（解复用器、解码器、渲染器、同步控制器）通过抽象接口解耦，具备良好的可复用性和扩展性。支持本地文件、流媒体播放，并实现音视频同步。

## ✨ 特性

- **支持的媒体源**：
  - 本地音视频文件（MP4、AVI、MKV、FLV、MP3 等 FFmpeg 支持格式）
  - HLS 直播/点播流（m3u8 + ts 分片，多线程预加载）
- **音视频渲染**：
  - 视频：SDL2/OpenGL 渲染，支持窗口缩放、全屏
  - 音频：SDL2 音频输出，自动适配设备
- **性能优化**：
  - 多线程解码：解复用、音频解码、视频解码独立线程
  - 无锁环形缓冲区（RingBuffer）传递 AVPacket/AVFrame
  - 动态丢帧策略，保证同步流畅
- **扩展性**：
  - 协议处理器工厂，新增协议只需继承 `AVDataFetcher`
- **跨平台**：已在 Ubuntu 20.04/22.04 (WSL2 及原生) 编译通过

## 🛠 技术栈

- **语言**：C++17
- **多媒体处理**：FFmpeg (libavformat, avcodec, avfilter, swscale, swresample)
- **渲染与音频输出**：SDL2, OpenGL (可选)
- **用户界面**：Qt 6.9.2 (Widgets) —— 用于演示客户端，播放核心不依赖 Qt
- **构建系统**：CMake 3.16+, Ninja
- **调试与性能分析**：GDB, Valgrind, perf

## 🚀 快速开始

### 依赖安装 (Ubuntu)

```bash
# 安装系统依赖
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config \
                   libavcodec-dev libavformat-dev libavutil-dev \
                   libswscale-dev libswresample-dev libavfilter-dev \
                   libsdl2-dev libgl1-mesa-dev \
                   qt6-base-dev qt6-tools-dev
```

### 编译构建

```bash
git clone https://github.com/RinzeMoon/RinzePlayer-master.git
cd RinzePlayer
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
# or cmake -G "Ninja" ..
ninja
```

### 运行

```bash
./RinzePlayer
```
## 📄 许可证

本项目基于 GPL v3 许可证开源，详情请参见 [LICENSE](LICENSE) 文件。

## 📬 联系方式

- 作者：[RinzeMoon]
- GitHub：[RinzePlayer-master](https://github.com/RinzeMoon/RinzePlayer-master#)

---

如果这个项目对您有帮助，请给一个 ⭐️ 鼓励！
