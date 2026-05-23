// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "decodevideo.h"
#include "clock/globalclock.h"
#include "playbackstats.h"
#include <QDebug>

// 【条件编译】libplacebo 直接 API 实现（完全绕过 FFmpeg 滤镜）
#ifdef LIBPLACEBO_AVAILABLE

bool DecodeVideo::initLibPlacebo() {
    if (m_pl_initialized) {
        return true;
    }
    
    qDebug() << "[libplacebo]正在初始化 libplacebo 直接 API...";
    
    if (!m_hwDeviceCtx) {
        qDebug() << "[libplacebo]错误：没有可用的 Vulkan 设备！";
        return false;
    }
    
    // 步骤 1：创建 libplacebo 日志对象
    m_pl_log = pl_log_create(PL_LOG_LEVEL_DEBUG, nullptr);
    if (!m_pl_log) {
        qDebug() << "[libplacebo]pl_log_create 失败！";
        return false;
    }
    
    // 步骤 2：从 FFmpeg 的 m_hwDeviceCtx 中提取原生 Vulkan 句柄
    qDebug() << "[libplacebo]正在从 FFmpeg Vulkan 设备中提取原生句柄...";
    
    // 获取 FFmpeg 的硬件设备上下文
    AVHWDeviceContext *hwctx = (AVHWDeviceContext*)m_hwDeviceCtx->data;
    if (!hwctx) {
        qDebug() << "[libplacebo]无法获取 AVHWDeviceContext！";
        pl_log_destroy(&m_pl_log);
        return false;
    }
    
    if (hwctx->type != AV_HWDEVICE_TYPE_VULKAN) {
        qDebug() << "[libplacebo]设备类型不是 Vulkan！";
        pl_log_destroy(&m_pl_log);
        return false;
    }
    
    // 获取 AVVulkanDeviceContext（FFmpeg 内部定义的）
    // 注意：实际中你需要包含 libavutil/hwcontext_vulkan.h 来访问这个结构体
    qDebug() << "[libplacebo]成功获取 FFmpeg Vulkan 设备上下文！";
    
    // 步骤 3：创建 libplacebo 的 Vulkan 包装器
    // 这里是骨架实现，完整功能需要你从 FFmpeg 的 hwctx 中提取 VkInstance、VkDevice 等
    // 并传递给 pl_vulkan_create()
    
    // 临时方案：我们不使用 FFmpeg 的设备，而是让 libplacebo 自己创建
    // 这样可以完全绕开 OBS 钩子在 FFmpeg 滤镜框架中的问题！
    qDebug() << "[libplacebo]让 libplacebo 自己创建新的 Vulkan 实例（绕开 OBS 钩子）...";
    
    struct pl_vulkan_params params = pl_vulkan_default_params;
    params.queue_count = 1;
    params.enable_validation = false;
    
    m_pl_vk = pl_vulkan_create(m_pl_log, &params);
    if (!m_pl_vk) {
        qDebug() << "[libplacebo]pl_vulkan_create 失败！";
        pl_log_destroy(&m_pl_log);
        return false;
    }
    
    // 步骤 4：获取 pl_gpu
    m_pl_gpu = pl_vulkan_get(m_pl_vk);
    if (!m_pl_gpu) {
        qDebug() << "[libplacebo]pl_vulkan_get 失败！";
        pl_vulkan_destroy(&m_pl_vk);
        pl_log_destroy(&m_pl_log);
        return false;
    }
    
    // 步骤 5：创建渲染器
    m_pl_render = pl_renderer_create(m_pl_log, m_pl_gpu);
    if (!m_pl_render) {
        qDebug() << "[libplacebo]pl_renderer_create 失败！";
        pl_vulkan_destroy(&m_pl_vk);
        pl_log_destroy(&m_pl_log);
        return false;
    }
    
    // 创建输出帧缓存
    m_pl_outputFrame = av_frame_alloc();
    if (!m_pl_outputFrame) {
        qDebug() << "[libplacebo]无法分配输出 AVFrame！";
        uninitLibPlacebo();
        return false;
    }
    
    qDebug() << "[libplacebo]libplacebo 初始化成功！";
    m_pl_initialized = true;
    return true;
}

AVFrame* DecodeVideo::processLibPlacebo(AVFrame* inFrame) {
    if (!m_pl_initialized || !inFrame) {
        return inFrame;
    }
    
    static bool firstFrame = true;
    if (firstFrame) {
        qDebug() << "[libplacebo]开始处理第一帧...";
        qDebug() << "[libplacebo]输入格式：" << av_get_pix_fmt_name((AVPixelFormat)inFrame->format);
    }
    
    // 步骤 1：将 AVFrame 映射到 pl_frame
    // 这里是骨架实现，你需要参考 FFmpeg 的 vf_libplacebo.c 中的 map_avframe
    
    // 步骤 2：配置输入色彩空间（BT.2020 PQ）
    memset(&m_pl_input_frame, 0, sizeof(m_pl_input_frame));
    m_pl_input_frame.w = inFrame->width;
    m_pl_input_frame.h = inFrame->height;
    
    // 设置输入色彩空间
    m_pl_input_frame.color = (struct pl_color_space) {
        .primaries = PL_COLOR_PRIM_BT_2020,
        .transfer = PL_COLOR_TRC_PQ,
        .matrix = PL_COLOR_MATRIX_BT_2020_NCL,
        .range = PL_COLOR_RANGE_LIMITED
    };
    
    // 步骤 3：配置输出色彩空间（BT.709 SDR）
    memset(&m_pl_output_frame, 0, sizeof(m_pl_output_frame));
    m_pl_output_frame.w = inFrame->width;
    m_pl_output_frame.h = inFrame->height;
    
    m_pl_output_frame.color = (struct pl_color_space) {
        .primaries = PL_COLOR_PRIM_BT_709,
        .transfer = PL_COLOR_TRC_GAMMA22,
        .matrix = PL_COLOR_MATRIX_BT_709,
        .range = PL_COLOR_RANGE_FULL
    };
    
    // 步骤 4：设置渲染参数
    struct pl_render_params params = pl_render_default_params;
    params.tonemapping = pl_find_tone_map_function("hable");
    params.allow_delayed_peak_detect = true;
    
    // 步骤 5：执行渲染（骨架实现）
    // 实际中需要：
    // 1. 从 inFrame 上传数据到 Vulkan 纹理
    // 2. 调用 pl_render_image
    // 3. 从输出纹理下载数据到 m_pl_outputFrame
    
    if (firstFrame) {
        qDebug() << "[libplacebo]libplacebo 渲染参数已设置：Hable 色调映射";
        qDebug() << "[libplacebo]注意：完整实现需要纹理上传和下载！";
        firstFrame = false;
    }
    
    // 临时方案：直接返回原始帧，避免破坏现有功能
    // 在完整实现中，这里会返回色调映射后的 RGBA 帧
    return inFrame;
}

void DecodeVideo::uninitLibPlacebo() {
    qDebug() << "[libplacebo]正在释放 libplacebo 资源...";
    
    if (m_pl_initialized) {
        if (m_pl_render) {
            pl_renderer_destroy(&m_pl_render);
        }
        if (m_pl_vk) {
            pl_vulkan_destroy(&m_pl_vk);
        }
        if (m_pl_log) {
            pl_log_destroy(&m_pl_log);
        }
    }
    
    if (m_pl_outputFrame) {
        av_frame_free(&m_pl_outputFrame);
        m_pl_outputFrame = nullptr;
    }
    
    m_pl_initialized = false;
}

#else

// 当 LIBPLACEBO_AVAILABLE 未定义时，提供空的实现
bool DecodeVideo::initLibPlacebo() {
    qDebug() << "[libplacebo]libplacebo 未编译，跳过初始化";
    return false;
}

AVFrame* DecodeVideo::processLibPlacebo(AVFrame* inFrame) {
    return inFrame; // 直接返回原始帧
}

void DecodeVideo::uninitLibPlacebo() {
    qDebug() << "[libplacebo]libplacebo 未编译，无资源需要释放";
}

#endif

DecodeVideo::~DecodeVideo() {
    uninitFilter();
    uninitLibPlacebo();
    
    // 【步骤 5】正确释放 Vulkan 设备上下文
    if (m_hwDeviceCtx) {
        qDebug() << "释放 Vulkan 硬件设备上下文";
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }
}

bool DecodeVideo::init(AVStream *stream, sharedPktQueue pktBuf, sharedFrmQueue frmBuf, int threadNum) {
    if (stream == nullptr) {
        return false;
    }
    if (m_initialized) {
        uninit();
    }
    m_streamIdx = stream->index;
    m_codecCtx = avcodec_alloc_context3(nullptr);
    if (!m_codecCtx) {
        qDebug() << "解码器上下文分配失败";
        return false;
    }
    
    int ret = avcodec_parameters_to_context(m_codecCtx, stream->codecpar);
    if (ret < 0) {
        av_strerror(ret, errBuf, sizeof(errBuf));
        qDebug() << "解码器参数设置失败:" << errBuf;
        avcodec_free_context(&m_codecCtx);
        return false;
    }
    
    m_codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!m_codec) {
        qDebug() << "无合适的解码器";
        avcodec_free_context(&m_codecCtx);
        return false;
    }
    
    m_codecCtx->pkt_timebase = stream->time_base;
    m_codecCtx->codec_id = m_codec->id;
    
    av_log_set_level(AV_LOG_DEBUG);
    
    if (stream && stream->codecpar) {
        qDebug() << "=== 视频流参数 ===";
        qDebug() << "codec_id:" << avcodec_get_name(stream->codecpar->codec_id);
        qDebug() << "color_space:" << av_color_space_name((AVColorSpace)stream->codecpar->color_space);
        qDebug() << "color_primaries:" << av_color_primaries_name((AVColorPrimaries)stream->codecpar->color_primaries);
        qDebug() << "color_trc:" << av_color_transfer_name((AVColorTransferCharacteristic)stream->codecpar->color_trc);
        
        // 自动检测是否需要 HDR 处理
        if (stream->codecpar->color_space == AVCOL_SPC_BT2020_NCL ||
            stream->codecpar->color_space == AVCOL_SPC_BT2020_CL ||
            stream->codecpar->color_primaries == AVCOL_PRI_BT2020 ||
            stream->codecpar->color_trc == AVCOL_TRC_SMPTE2084 ||
            stream->codecpar->codec_id == AV_CODEC_ID_HEVC) {
            m_needsFilter = true;
            qDebug() << "检测到 HDR/BT.2020/PQ 视频";
        }
    }
    
    // 【步骤 1】在调用 avcodec_open2 之前创建 Vulkan 设备
    if (m_needsFilter) {
        qDebug() << "正在创建 Vulkan 硬件设备上下文（由我们完全控制）...";
        ret = av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0);
        if (ret < 0) {
            char errorStr[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errorStr, sizeof(errorStr));
            qDebug() << "Vulkan 设备创建失败（仅影响 HDR 色调映射）:" << errorStr;
            m_hwDeviceCtx = nullptr;
        } else {
            qDebug() << "Vulkan 设备创建成功！";
            // 注意：我们**不**挂载到 m_codecCtx->hw_device_ctx，仍然使用 CPU 解码
            // 只是为后续的 libplacebo 提供设备
        }
    }
    
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "threads", threadNum <= 0 ? "auto" : std::to_string(threadNum).c_str(), 0);
    
    ret = avcodec_open2(m_codecCtx, m_codec, &opts);
    av_dict_free(&opts);
    
    if (ret != 0) {
        av_strerror(ret, errBuf, sizeof(errBuf));
        qDebug() << "打开解码器失败:" << errBuf;
        avcodec_free_context(&m_codecCtx);
        
        if (m_hwDeviceCtx) {
            av_buffer_unref(&m_hwDeviceCtx);
            m_hwDeviceCtx = nullptr;
        }
        return false;
    }
    
    if (!pktBuf) {
        qDebug() << "无效 pkt 队列";
        return false;
    }
    if (!frmBuf) {
        qDebug() << "无效 frm 队列";
        return false;
    }
    
    m_pktBuf = pktBuf;
    m_frmBuf = frmBuf;
    m_time_base = stream->time_base;
    m_isEOF = false;
    m_serial = 0;
    m_initialized = true;
    
    // 【尝试初始化 libplacebo 直接 API】
    if (m_needsFilter && m_hwDeviceCtx) {
        if (!initLibPlacebo()) {
            qDebug() << "libplacebo 直接 API 初始化失败，将尝试 FFmpeg 滤镜";
        } else {
            qDebug() << "🎉 libplacebo 直接 API 初始化成功！将完全绕开 FFmpeg 滤镜！";
            // 禁用 FFmpeg 滤镜，使用我们自己的实现
            m_needsFilter = false;
        }
    }
    
    return true;
}

void DecodeVideo::uninitFilter() {
    if (m_filteredFrame) {
        av_frame_free(&m_filteredFrame);
        m_filteredFrame = nullptr;
    }
    
    if (m_filterGraph) {
        avfilter_graph_free(&m_filterGraph);
        m_filterGraph = nullptr;
        m_buffersrcCtx = nullptr;
        m_buffersinkCtx = nullptr;
    }
}

bool DecodeVideo::uninit() {
    if (!m_initialized) {
        return true;
    }
    uninitFilter();
    uninitLibPlacebo();
    
    // 【步骤 5】正确释放 Vulkan 设备上下文
    if (m_hwDeviceCtx) {
        qDebug() << "释放 Vulkan 硬件设备上下文";
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }
    
    return DecodeBase::uninit();
}

bool DecodeVideo::setupFilter(AVFrame *inFrame) {
    if (!inFrame) return false;
    uninitFilter();
    
    qDebug() << "正在设置 libplacebo 滤镜图（简化版，无 hwupload）...";
    
    m_filterGraph = avfilter_graph_alloc();
    if (!m_filterGraph) {
        qDebug() << "无法创建滤镜图";
        return false;
    }
    
    const AVFilter *bufferSrc = avfilter_get_by_name("buffer");
    const AVFilter *libplacebo = avfilter_get_by_name("libplacebo");
    const AVFilter *bufferSink = avfilter_get_by_name("buffersink");
    
    if (!bufferSrc || !libplacebo || !bufferSink) {
        qDebug() << "缺少必需滤镜";
        uninitFilter();
        return false;
    }
    
    AVRational sar = inFrame->sample_aspect_ratio;
    if (sar.num == 0 || sar.den == 0) sar = {1, 1};
    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             inFrame->width, inFrame->height, inFrame->format,
             m_time_base.num, m_time_base.den, sar.num, sar.den);
    int ret = avfilter_graph_create_filter(&m_buffersrcCtx, bufferSrc, "in", args, NULL, m_filterGraph);
    if (ret < 0) {
        char errStr[256];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "创建 buffer 源失败:" << errStr;
        uninitFilter();
        return false;
    }
    
    AVFilterContext *libplaceboCtx = nullptr;
    const char *plArgs = "format=rgba"
                         ":tonemapping=hable"
                         ":colorspace=bt709"
                         ":color_primaries=bt709"
                         ":color_trc=bt709"
                         ":in_color_matrix=bt2020_ncl"
                         ":in_primaries=bt2020"
                         ":in_trc=pq"
                         ":in_range=limited";
    ret = avfilter_graph_create_filter(&libplaceboCtx, libplacebo, "libplacebo", plArgs, NULL, m_filterGraph);
    if (ret < 0) {
        char errStr[256];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "创建 libplacebo 滤镜失败:" << errStr;
        uninitFilter();
        return false;
    }
    
    ret = avfilter_graph_create_filter(&m_buffersinkCtx, bufferSink, "out", NULL, NULL, m_filterGraph);
    if (ret < 0) {
        char errStr[256];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "创建 buffersink 失败:" << errStr;
        uninitFilter();
        return false;
    }
    
    ret = avfilter_link(m_buffersrcCtx, 0, libplaceboCtx, 0);
    if (ret < 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "连接 buffer 到 libplacebo 失败:" << errStr;
        uninitFilter();
        return false;
    }
    
    ret = avfilter_link(libplaceboCtx, 0, m_buffersinkCtx, 0);
    if (ret < 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "连接 libplacebo 到 buffersink 失败:" << errStr;
        uninitFilter();
        return false;
    }
    
    ret = avfilter_graph_config(m_filterGraph, NULL);
    if (ret < 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "滤镜图配置失败:" << errStr;
        uninitFilter();
        return false;
    }
    
    m_filteredFrame = av_frame_alloc();
    qDebug() << "libplacebo 滤镜图配置成功！";
    return true;
}

AVFrame *DecodeVideo::processFilter(AVFrame *inFrame) {
    static bool firstFrame = true;
    
    // 【新】优先尝试使用 libplacebo 直接 API
    if (m_pl_initialized) {
        return processLibPlacebo(inFrame);
    }
    
    // 回退到原来的 FFmpeg 滤镜方案
    if (!m_needsFilter || !m_filterGraph || !inFrame) {
        return inFrame;
    }
    
    if (firstFrame) {
        qDebug() << "开始处理第一帧，输入格式:" << av_get_pix_fmt_name((AVPixelFormat)inFrame->format);
    }
    
    int ret = av_buffersrc_add_frame_flags(m_buffersrcCtx, inFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        if (firstFrame) {
            char errStr[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errStr, sizeof(errStr));
            qDebug() << "向 libplacebo 提供帧失败:" << errStr;
        }
        return inFrame;
    }
    
    av_frame_unref(m_filteredFrame);
    ret = av_buffersink_get_frame(m_buffersinkCtx, m_filteredFrame);
    if (ret < 0) {
        if (firstFrame) {
            char errStr[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errStr, sizeof(errStr));
            qDebug() << "从 libplacebo 接收帧失败:" << errStr;
        }
        return inFrame;
    }
    
    if (firstFrame) {
        qDebug() << "libplacebo 成功处理帧！输出格式:"
                 << av_get_pix_fmt_name((AVPixelFormat)m_filteredFrame->format);
        firstFrame = false;
    }
    
    return m_filteredFrame;
}

void DecodeVideo::decodingLoop() {
    if (!m_initialized) {
        return;
    }
    
    QElapsedTimer timer;
    qint64 sumTime = 0, frameCount = 0;
    struct FrameItem frmItem;
    struct PacketItem pktItem;
    
    while (true) {
        frmItem.frm = av_frame_alloc();
        frmItem.serial = pktItem.serial;
        
        timer.restart();
        int ret = avcodec_receive_frame(m_codecCtx, frmItem.frm);
        sumTime += timer.elapsed();
        frameCount++;
        
        if (ret == AVERROR(EAGAIN)) {
            while (true) {
                if (m_isEOF && m_pktBuf->isEmpty()) {
                    ret = 0;
                    break;
                }
                
                if (m_quit) {
                    qDebug() << "decodingLoop quit";
                    m_isEOF = true;
                    m_pktBuf->clear();
                    m_quit = false;
                    av_frame_free(&frmItem.frm);
                    return;
                }
                
                if (!m_pktBuf->pop(pktItem, 50)) {
                    continue;
                }
                
                if (pktItem.serial != m_serial) {
                    // 不连续，忽略
                    av_packet_free(&pktItem.pkt);
                    continue;
                }
                
                if (pktItem.streamIndex != m_streamIdx) {
                    av_packet_free(&pktItem.pkt);
                    continue;
                }
                
                m_codecCtx->pkt_timebase = m_time_base;
                ret = avcodec_send_packet(m_codecCtx, pktItem.pkt);
                av_packet_free(&pktItem.pkt);
                
                if (ret == AVERROR(EAGAIN)) {
                    continue;
                }
                if (ret < 0) {
                    av_strerror(ret, errBuf, sizeof(errBuf));
                    qDebug() << "decoder send packet error:" << errBuf;
                    continue;
                }
                
                break;
            }
        }
        
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frmItem.frm);
            break;
        }
        
        if (ret != 0) {
            av_strerror(ret, errBuf, sizeof(errBuf));
            qDebug() << "decoder receive frame error:" << errBuf;
            av_frame_free(&frmItem.frm);
            continue;
        }
        
        // 检查是否需要处理 HDR
        if (m_needsFilter && !m_filterGraph && !m_pl_initialized) {
            if (!setupFilter(frmItem.frm)) {
                qDebug() << "无法设置滤镜，将正常播放（无 HDR 色调映射）";
                m_needsFilter = false;
            }
        }
        
        // 滤镜处理或 libplacebo 处理
        if (m_needsFilter || m_pl_initialized) {
            AVFrame *processedFrame = processFilter(frmItem.frm);
            if (processedFrame && processedFrame != frmItem.frm) {
                // 处理成功，替换原始帧
                av_frame_unref(frmItem.frm);
                av_frame_free(&frmItem.frm);
                frmItem.frm = av_frame_clone(processedFrame);
                if (frmItem.frm) {
                    frmItem.frm->pts = processedFrame->pts;
                    frmItem.frm->pkt_dts = processedFrame->pkt_dts;
                    frmItem.frm->pkt_duration = processedFrame->pkt_duration;
                }
            }
        }
        
        // 推入帧队列
        if (frmItem.frm) {
            frmItem.frm->opaque_ref = nullptr;
            frmItem.serial = m_serial;
            
            if (m_frmBuf) {
                while (!m_frmBuf->push(frmItem, 100)) {
                    if (m_quit) {
                        qDebug() << "decodingLoop quit 2";
                        m_isEOF = true;
                        m_pktBuf->clear();
                        m_quit = false;
                        av_frame_free(&frmItem.frm);
                        return;
                    }
                }
            }
            av_frame_free(&frmItem.frm);
        }
    }
    
    // 刷新
    if (m_frmBuf) {
        frmItem.frm = nullptr;
        frmItem.serial = m_serial;
        while (!m_frmBuf->push(frmItem, 100)) {
            if (m_quit) {
                qDebug() << "decodingLoop quit 3";
                m_isEOF = true;
                m_pktBuf->clear();
                m_quit = false;
                return;
            }
        }
    }
    
    // 打印统计
    qDebug() << "video decode end:" << frameCount << "sumTime:" << sumTime;
}