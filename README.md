# RinzePlayer

> 本项目基于 [AZPlayer](https://github.com/az7792/AZPlayer) 二次开发，在原有播放器基础上新增异地双人同步观影、弹幕聊天、视频分析报告等功能。

基于 C++/Qt6 + FFmpeg 8.0 的全功能音视频播放器，集成异地双人实时同步观影。

## 技术栈

`C++17` `Qt 6.5` `FFmpeg 8.0` `OpenGL 3.3` `miniaudio` `Node.js` `gRPC`

## 功能

- 本地文件 / HLS / RTMP / RTSP 播放
- 多线程解码管线（Demux → Decode ×3 → Render）
- OpenGL 视频渲染 + miniaudio 音频输出
- ASS/SRT 字幕支持
- 10段参数均衡器 / 31段图形均衡器 / 混响效果器
- FFT 频谱可视化（4 种模式）
- 画面旋转 / 缩放 / 镜像 / 亮度对比度调节
- 画中画 / 无边框窗口 / 悬浮控制栏
- 运动向量可视化 / 热力图 / 轨迹图
- 视频分析报告（HTML + PDF）
- **异地双人同步观影** — TCP 帧协议 + Server 权威同步 + 弹幕聊天

## 构建

```bash
# 依赖: Qt 6.5+, FFmpeg 8.0, CMake 3.16+, MSVC 2019+
cmake -B build -DCMAKE_PREFIX_PATH=D:/Qt/6.5.3/msvc2019_64 -DFFMPEG_DIR=D:/FFmpegLIB
cmake --build build --config Release
```

## 双人观影

```bash
# 1. 启动 ChatRoom Server
cd ../chatroom-server && npm install && node src/index.js

# 2. 启动两个客户端
./build/Release/RinzePlayer.exe
# Host: 视图 → 双人观影 → 创建房间 → 输入URL → 点"我准备好了"
# Guest: 视图 → 双人观影 → 加入房间 → 等待加载 → 点"我准备好了"
# 双方就绪后自动开始同步播放
```

## 项目结构

| 目录 | 说明 |
|------|------|
| `core/` | 播放控制 + 同步管理 |
| `demux/` | FFmpeg 解复用 |
| `decode/` | 音视频字幕解码 |
| `renderer/` | 音视频渲染输出 |
| `streaming/` | 网络流 + TCP 帧协议 |
| `ui/` | Qt 界面 |
| `audioeffects/` | 均衡器/混响 |
| `visualizer/` | FFT 频谱可视化 |
| `../chatroom-server/` | Node.js 房间 + 同步仲裁 |
| `../status-server/` | Node.js 在线心跳 |

## 许可

基于原项目 [AZPlayer](https://github.com/az7792/AZPlayer) 的 GPL-3.0 许可证。
