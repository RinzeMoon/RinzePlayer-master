# RinzePlayer 核心架构图

## 一、解复用 → 解码 → 渲染管线

```mermaid
graph TD
    subgraph 输入
        URL[URL / 本地文件]
    end

    subgraph StreamManager
        SM[StreamManager] --> |协议识别| PROTO{HLS? RTMP? RTSP? Local?}
        PROTO --> |配置| SCFG[StreamingConfig<br/>预缓冲/退避/防抖]
    end

    subgraph Demux
        DTH[Demux 线程] --> |av_read_frame| DPKT[AVPacket]
        DFLAG[m_needSeek 原子标志]
        DFLAG --> |seekBySec| DTH
        DPKT --> |video| VPQ[Video PacketQueue<br/>容量:10]
        DPKT --> |audio| APQ[Audio PacketQueue<br/>容量:2]
        DPKT --> |subtitle| SPQ[Subtitle PacketQueue<br/>容量:2]
    end

    subgraph Decode
        DVT[DecodeVideo 线程] --> |avcodec_send_packet| VFQ[Video FrameQueue<br/>容量:3 低延迟]
        DAT[DecodeAudio 线程] --> |avcodec_send_packet| AFQ[Audio FrameQueue<br/>容量:50]
        DST[DecodeSubtitle 线程] --> |avcodec_send_packet| SFQ[Subtitle FrameQueue]
        VFQ --> |可选| FLT[FFmpeg 滤镜链<br/>scale/format/crop]
        FLT --> |可选| LP[libplacebo Vulkan<br/>色域转换/缩放]
    end

    subgraph Render
        VP[VideoPlayer 线程] --> |PTS 对比| GC{GlobalClock}
        VP --> VR[VideoRenderer<br/>OpenGL 3.3 Core]
        AP[AudioPlayer 线程] --> |miniaudio 回调| ADEV[AudioDevice<br/>PCM Float32]
        AP --> |更新 audioPts| GC
        GC --> |audioPts 主时钟| VP
    end

    URL --> SM
    SM --> DTH
    VPQ --> DVT
    APQ --> DAT
    SPQ --> DST
    VFQ --> VP
    AFQ --> AP
    SFQ --> SUB[Subtitle Renderer<br/>libass]
```

## 二、OpenGL 着色器与变换管线

```mermaid
graph TD
    subgraph Frame上传
        AVF[AVFrame<br/>YUV420P/NV12/P010] --> |av_frame_get_buffer| RAW[原始 Plane 数据]
        RAW --> |glTexImage2D| TEX[多平面 GL 纹理<br/>Y/U/V 各一张]
    end

    subgraph Vertex_Shader
        VIN[顶点: 四边形 4 顶点] --> VAO[VAO + VBO]
        VAO --> VSHADE[vertex shader<br/>MVP 变换]
    end

    subgraph Transform
        TMAT[QMatrix4x4 变换矩阵] --> ROT[旋转 0°~360°]
        TMAT --> SCL[缩放 10%~400%]
        TMAT --> TRANS[平移 X/Y]
        TMAT --> MIRROR[水平/垂直镜像]
        ROT & SCL & TRANS & MIRROR --> UNIFORM[uniform mat4 uMVP]
    end

    subgraph Fragment_Shader
        UNIFORM --> FSHADE[fragment shader]
        TEX --> FSHADE
        FSHADE --> |YUV→RGB 矩阵| COL[色域转换<br/>BT.601/BT.709/BT.2020]
        COL --> |uniform| FILTER[亮度/对比度/饱和度]
        FILTER --> |glDrawArrays| FBUF[Framebuffer]
    end

    subgraph HUD 叠加
        FBUF --> |QPainter| FPS[FPS 显示]
        FBUF --> |QPainter| MVEC[运动向量箭头]
    end
```

## 三、流媒体处理管线

```mermaid
graph TD
    subgraph 协议层
        URL[输入 URL] --> PARSE{解析协议}
        PARSE --> HLS[HLS<br/>m3u8 playlist]
        PARSE --> RTMP[RTMP<br/>直播推流]
        PARSE --> RTSP[RTSP<br/>实时流]
        PARSE --> HTTP[HTTP<br/>直链文件]
        PARSE --> LOCAL[本地文件]
    end

    subgraph 连接管理
        HLS & RTMP & RTSP & HTTP --> CONN[StreamManager::open]
        CONN --> STATE[StreamState 状态机]
        STATE --> IDLE((Idle))
        IDLE --> |open| CONNECTING((Connecting))
        CONNECTING --> |成功| CONNECTED((Connected))
        CONNECTED --> |缓冲不足| BUFFERING((Buffering))
        BUFFERING --> |缓冲充足| PLAYING((Playing))
        PLAYING --> |暂停| PAUSED((Paused))
        CONNECTING --> |失败| RECONNECTING((Reconnecting))
        RECONNECTING --> |重试| CONNECTING
        RECONNECTING --> |超限| ERROR((Error))
        PLAYING --> |断开| DISCONNECTED((Disconnected))
    end

    subgraph 容错机制
        EB[ExponentialBackoff<br/>1s→2s→4s→8s→16s→30s] --> RECONNECTING
        DB[DebounceManager<br/>300ms 防抖窗口] --> EB
        DB --> |ignore| SKIP((忽略重复操作))
    end

    subgraph 缓冲控制
        PLAYING --> |定时检查| BUFCHECK{queueSize/maxSize < threshold?}
        BUFCHECK --> |是| BUFFERING
        BUFCHECK --> |否| PLAYING
        CONFIG[StreamingConfig<br/>preBuffer:2MB<br/>minThreshold:0.5MB<br/>maxLatency:3s] --> BUFCHECK
    end

    subgraph 低延迟模式
        LDMODE[低延迟开关] --> NOBUF[AVFMT_FLAG_NOBUFFER]
        LDMODE --> LODELAY[AVFMT_FLAG_LOW_DELAY]
        LDMODE --> DISCORR[AVFMT_FLAG_DISCARD_CORRUPT]
        LDMODE --> SHORT[快速探测<br/>fpsprobesize=32]
        NOBUF & LODELAY & DISCORR & SHORT --> DEMUX[Demux AVFormatContext]
    end

    CONN --> DEMUX
```

## 四、组件速查

| 组件 | 线程模型 | 输入 | 输出 |
|------|---------|------|------|
| Demux | `std::thread` 独立 | AVFormatContext URL | AVPacket ×3 队列 |
| DecodeVideo | `std::thread` 独立 | Video PacketQueue | AVFrame → Video FrameQueue |
| DecodeAudio | `std::thread` 独立 | Audio PacketQueue | AVFrame → Audio FrameQueue |
| DecodeSubtitle | `std::thread` 独立 | Subtitle PacketQueue | AVFrame → Subtitle FrameQueue |
| VideoPlayer | `std::thread` 独立 | Video FrameQueue | OpenGL Framebuffer |
| AudioPlayer | miniaudio 回调线程 | Audio FrameQueue | PCM Float32 → 设备 |
| StreamManager | QTimer + 回调 | QUrl | AVFormatContext |
| GlobalClock | 无(单例) | audioPts/videoPts | 主时钟 PTS |
