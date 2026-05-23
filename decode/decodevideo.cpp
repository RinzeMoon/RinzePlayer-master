// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "decodevideo.h"
#include "clock/globalclock.h"
#include "playbackstats.h"
#include <QDebug>

// 【条件编译】libplacebo 直接 API 实现
#ifdef LIBPLACEBO_AVAILABLE

bool DecodeVideo::initLibPlacebo()
{
    if (m_pl_initialized) return true;

    qDebug() << "[libplacebo] 正在初始化 libplacebo (直接 API)...";

    // 步骤 1：创建 libplacebo 日志对象
    pl_log log = pl_log_create(PL_LOG_LEVEL_DEBUG, nullptr);
    if (!log) {
        qDebug() << "[libplacebo] pl_log_create 失败！";
        return false;
    }

    qDebug() << "[libplacebo] libplacebo 日志对象创建成功！";

    // 步骤 2：不使用 FFmpeg 的设备，而是让 libplacebo 自己创建新的 Vulkan 实例！
    // 这是关键的一步，完全绕开 OBS 钩子在 FFmpeg 滤镜框架中的问题！
    qDebug() << "[libplacebo] 让 libplacebo 自己创建新的 Vulkan 实例...";
    
    struct pl_vulkan_params params = pl_vulkan_default_params;
    params.queue_count = 1;
    params.enable_validation = false;

    pl_vulkan_t *vk = pl_vulkan_create(log, &params);
    if (!vk) {
        qDebug() << "[libplacebo] pl_vulkan_create 失败！";
        pl_log_destroy(&log);
        return false;
    }

    // 步骤 3：获取 GPU
    pl_gpu *gpu = pl_vulkan_get(vk);
    if (!gpu) {
        qDebug() << "[libplacebo] pl_vulkan_get 失败！";
        pl_vulkan_destroy(&vk);
        pl_log_destroy(&log);
        return false;
    }

    // 步骤 4：创建渲染器
    pl_renderer *render = pl_renderer_create(log, gpu);
    if (!render) {
        qDebug() << "[libplacebo] pl_renderer_create 失败！";
        pl_vulkan_destroy(&vk);
        pl_log_destroy(&log);
        return false;
    }

    // 创建输出帧缓存
    m_pl_outputFrame = av_frame_alloc();
    if (!m_pl_outputFrame) {
        qDebug() << "[libplacebo] 无法分配输出 AVFrame！";
        pl_renderer_destroy(&render);
        pl_vulkan_destroy(&vk);
        pl_log_destroy(&log);
        return false;
    }

    // 保存指针
    m_pl_log = log;
    m_pl_vk = vk;
    m_pl_gpu = gpu;
    m_pl_render = render;

    qDebug() << "[libplacebo] libplacebo 初始化成功！🎊";
    qDebug() << "[libplacebo] 完全由我们控制 Vulkan，彻底绕开 OBS 钩子！";
    m_pl_initialized = true;
    return true;
}

// ========== 辅助函数：创建 RGBA 输出纹理 ==========
static pl_tex createRGBAOutputTexture(pl_gpu gpu, int width, int height)
{
    pl_fmt fmt = pl_find_fmt(gpu, &(struct pl_fmt_params){
        .caps = PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_LINEAR,
        .type = PL_FMT_TYPE_FLOAT,
        .component_size = {8, 8, 8, 8},
        .color_order = PL_FMT_RGBA,
    });

    if (!fmt) {
        return nullptr;
    }

    return pl_tex_create(gpu, &(struct pl_tex_params){
        .w = width,
        .h = height,
        .d = 1,
        .format = fmt,
        .renderable = true,
        .sampleable = true,
        .usage = PL_TEX_USAGE_RENDER_ATTACHMENT,
    });
}

// ========== 辅助函数：配置 HDR → SDR 色彩转换参数 ==========
static void configColorConversion(struct pl_render_params *params)
{
    // 目标色彩空间：BT.709 SDR
    params->color.color.luminance = 100.0f;
    params->color.color.primaries = PL_COLOR_PRIM_BT709;
    params->color.color.transfer = PL_COLOR_TRC_BT709;
    params->color.color.light = PL_COLOR_LIGHT_DISPLAY;
    params->color.alpha = PL_ALPHA_INDEPENDENT;

    // 色调映射：Hable 算子
    params->tone_mapping.curve = PL_TONE_MAP_HABLE;
    params->tone_mapping.param = 1.0f;
    params->tone_mapping.lut_size = 256;

    // 去色带
    params->dither.method = PL_DITHER_BLUE_NOISE;
}

AVFrame* DecodeVideo::convertFrame(AVFrame *src)
{
    if (!m_pl_initialized || !src) {
        return src;
    }

    static bool firstFrame = true;
    if (firstFrame) {
        qDebug() << "[libplacebo] 开始处理第一帧...";
        qDebug() << "[libplacebo] 输入格式：" << av_get_pix_fmt_name((AVPixelFormat)src->format);
        qDebug() << "[libplacebo] 输入分辨率：" << src->width << "x" << src->height;
        firstFrame = false;
    }

    // ========== 步骤 1：检查尺寸，必要时重新创建纹理 ==========
    if (src->width != m_pl_lastWidth || src->height != m_pl_lastHeight) {
        qDebug() << "[libplacebo] 分辨率变化，重新创建纹理...";
        
        // 销毁旧的纹理
        if (m_pl_texIn) {
            pl_tex_destroy(m_pl_gpu, &m_pl_texIn);
        }
        if (m_pl_texOut) {
            pl_tex_destroy(m_pl_gpu, &m_pl_texOut);
        }

        // 创建新的输出纹理（RGBA 格式）
        m_pl_texOut = createRGBAOutputTexture(m_pl_gpu, src->width, src->height);
        if (!m_pl_texOut) {
            qDebug() << "[libplacebo] 无法创建输出纹理！";
            return src;
        }

        m_pl_lastWidth = src->width;
        m_pl_lastHeight = src->height;
        qDebug() << "[libplacebo] 纹理创建成功！";
    }

    // ========== 步骤 2：上传 AVFrame 到 libplacebo 纹理 ==========
    struct pl_avframe_params uploadParams = {
        .gpu = m_pl_gpu,
        .tex = &m_pl_texIn,
        .frame = src,
        .map = false,
        .color = {
            .light = PL_COLOR_LIGHT_UNKNOWN,
            .primaries = PL_COLOR_PRIM_BT2020,
            .transfer = PL_COLOR_TRC_PQ,
            .luminance = 1000.0f,
        },
        .repr = PL_COLOR_REPR_RGB,
    };

    if (!pl_upload_avframe(&uploadParams)) {
        qDebug() << "[libplacebo] pl_upload_avframe 失败！";
        return src;
    }

    // ========== 步骤 3：配置渲染参数 ==========
    struct pl_render_params renderParams = pl_render_params_default;
    configColorConversion(&renderParams);

    // 设置源图像
    struct pl_plane planes[4];
    struct pl_image srcImage = {
        .w = src->width,
        .h = src->height,
        .num_planes = 1,
        .planes = {
            {
                .texture = m_pl_texIn,
                .components = 4,
            },
        },
        .repr = PL_COLOR_REPR_RGB,
    };

    // 设置目标
    struct pl_render_target target = {
        .fbo = {{
            .params = {
                .w = src->width,
                .h = src->height,
                .tex = m_pl_texOut,
            },
        }},
    };

    // ========== 步骤 4：执行渲染！ ==========
    pl_render_image(m_pl_render, &srcImage, &target, &renderParams);

    // ========== 步骤 5：下载回系统内存到 RGBA 格式 ==========
    if (!m_pl_outputFrame) {
        m_pl_outputFrame = av_frame_alloc();
    }

    // 确保输出帧格式是 RGBA
    if (m_pl_outputFrame->format != AV_PIX_FMT_RGBA ||
        m_pl_outputFrame->width != src->width ||
        m_pl_outputFrame->height != src->height) {
        
        av_frame_unref(m_pl_outputFrame);
        m_pl_outputFrame->format = AV_PIX_FMT_RGBA;
        m_pl_outputFrame->width = src->width;
        m_pl_outputFrame->height = src->height;
        av_frame_get_buffer(m_pl_outputFrame, 32);
    }

    // 复制时间戳
    av_frame_copy_props(m_pl_outputFrame, src);

    // 下载纹理数据
    pl_gpu_download(m_pl_gpu, &(struct pl_dl_params){
        .tex = m_pl_texOut,
        .tex_w = src->width,
        .tex_h = src->height,
        .dst = {
            .stride[0] = m_pl_outputFrame->linesize[0],
            .ptr[0] = m_pl_outputFrame->data[0],
        },
    });

    qDebug() << "[libplacebo] 帧处理完成！输出 RGBA 格式";

    return m_pl_outputFrame;
}

void DecodeVideo::uninitLibPlacebo()
{
    qDebug() << "[libplacebo] 正在释放 libplacebo 资源...";
    
    if (m_pl_initialized) {
        if (m_pl_texIn) {
            pl_tex_destroy(m_pl_gpu, &m_pl_texIn);
        }
        if (m_pl_texOut) {
            pl_tex_destroy(m_pl_gpu, &m_pl_texOut);
        }
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
    m_pl_lastWidth = 0;
    m_pl_lastHeight = 0;
}

#else

// 当 libplacebo 未定义时的空实现
bool DecodeVideo::initLibPlacebo()
{
    qDebug() << "[libplacebo] libplacebo 未编译，跳过初始化";
    return false;
}

AVFrame* DecodeVideo::convertFrame(AVFrame *src)
{
    return src;
}

void DecodeVideo::uninitLibPlacebo()
{
    qDebug() << "[libplacebo] libplacebo 未编译，无资源需要释放";
}

#endif

DecodeVideo::~DecodeVideo() {
    uninitFilter();
    uninitLibPlacebo(); // 【新】释放 libplacebo 直接 API 资源
    if (m_hwDeviceCtx) {
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

        if (stream->codecpar->color_space == AVCOL_SPC_BT2020_NCL ||
            stream->codecpar->color_space == AVCOL_SPC_BT2020_CL ||
            stream->codecpar->color_primaries == AVCOL_PRI_BT2020 ||
            stream->codecpar->color_trc == AVCOL_TRC_SMPTE2084 ||
            stream->codecpar->codec_id == AV_CODEC_ID_HEVC) {
            m_needsFilter = true;
            qDebug() << "检测到 HDR/杜比视界视频，将使用 libplacebo 滤镜";
        }
    }

    // 【步骤 1】主动创建 Vulkan 硬件设备，仅供 libplacebo 使用（不挂载到解码器！）
    qDebug() << "正在创建 Vulkan 硬件设备上下文...";
    ret = av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0);
    if (ret >= 0 && m_hwDeviceCtx) {
        qDebug() << "Vulkan 设备创建成功，仅供 libplacebo 使用（不挂载到解码器）";
    } else {
        char errStr[256];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "Vulkan 设备创建失败，继续使用软件滤镜:" << errStr;
        m_hwDeviceCtx = nullptr;
    }

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "threads", threadNum <= 0 ? "auto" : std::to_string(threadNum).c_str(), 0);
    // 【运动向量】开启运动向量输出
    av_dict_set(&opts, "flags2", "+export_mvs", 0);

    ret = avcodec_open2(m_codecCtx, m_codec, &opts);
    av_dict_free(&opts);

    if (ret != 0) {
        av_strerror(ret, errBuf, sizeof(errBuf));
        qDebug() << "打开解码器失败:" << errBuf;
        avcodec_free_context(&m_codecCtx);
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
    
    // 【关键步骤】：首先尝试使用 libplacebo 直接 API，完全绕开 FFmpeg 滤镜！
    if (m_needsFilter && m_hwDeviceCtx) {
        if (initLibPlacebo()) {
            qDebug() << "🎉 libplacebo 直接 API 初始化成功！禁用 FFmpeg 滤镜，完全绕开 OBS 钩子！";
            m_needsFilter = false; // 不再尝试 FFmpeg 滤镜
        } else {
            qDebug() << "libplacebo 直接 API 初始化失败，将尝试 FFmpeg 滤镜（可能受 OBS 钩子影响）";
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

    qDebug() << "正在设置 libplacebo 滤镜图（复用 Vulkan 设备），格式:"
             << av_get_pix_fmt_name((AVPixelFormat)inFrame->format)
             << "分辨率:" << inFrame->width << "x" << inFrame->height;

    m_filterGraph = avfilter_graph_alloc();
    if (!m_filterGraph) {
        qDebug() << "无法创建滤镜图";
        return false;
    }

    // 【修正点 1】不强行关联设备到滤镜图（AVFilterGraph 没有 hw_device_ctx），让 libplacebo 自己处理
    if (m_hwDeviceCtx) {
        qDebug() << "外部 Vulkan 设备已准备就绪，libplacebo 将尝试使用";
    } else {
        qDebug() << "没有 Vulkan 设备，libplacebo 将尝试自动创建";
    }

    // 获取所有需要的滤镜（去除 hwupload）
    const AVFilter *bufferSrc = avfilter_get_by_name("buffer");
    const AVFilter *libplacebo = avfilter_get_by_name("libplacebo");
    const AVFilter *bufferSink = avfilter_get_by_name("buffersink");

    if (!bufferSrc || !libplacebo || !bufferSink) {
        qDebug() << "缺少必需滤镜（需要 buffer, libplacebo, buffersink）";
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

    // libplacebo 滤镜（参数完整）
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

    // 【修正点 2】简化滤镜连接：buffer -> libplacebo -> buffersink
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
    qDebug() << "libplacebo 滤镜图配置成功！使用外部 Vulkan 设备";
    return true;
}

AVFrame *DecodeVideo::processFilter(AVFrame *inFrame) {
    static bool firstFrame = true;

    // 【新】首先尝试使用 libplacebo 直接 API（如果可用）
    if (m_pl_initialized) {
        return convertFrame(inFrame);
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

    double startTime, sumTime;

    AVPktItem pktItem;
    AVFrmItem frmItem;
    bool needFlushBuffers = false;
    while (!m_stop.load(std::memory_order_relaxed)) {
        bool ok = getPkt(pktItem, needFlushBuffers);
        if (!ok) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (needFlushBuffers || m_serial != m_pktBuf->serial()) {
            m_serial = m_pktBuf->serial();
            avcodec_flush_buffers(m_codecCtx);
            needFlushBuffers = false;
        }

        startTime = getRelativeSeconds();
        int ret = avcodec_send_packet(m_codecCtx, pktItem.pkt);
        sumTime = getRelativeSeconds() - startTime;

        if (ret == 0) {
            av_packet_free(&pktItem.pkt);
        } else if (ret == AVERROR_EOF) {
            av_packet_free(&pktItem.pkt);
            m_isEOF = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        } else if (ret == AVERROR(EAGAIN)) {
            // nothing
        } else if (ret < 0) {
            av_strerror(ret, errBuf, sizeof(errBuf));
            qDebug() << "Video发送pkt错误:" << errBuf << pktItem.pkt->stream_index;
            goto end;
        }

        m_isEOF = false;
        while (true) {
            frmItem.frm = av_frame_alloc();
            frmItem.serial = pktItem.serial;

            startTime = getRelativeSeconds();
            ret = avcodec_receive_frame(m_codecCtx, frmItem.frm);
            sumTime += getRelativeSeconds() - startTime;

            // 完全消耗完解码后的帧
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&frmItem.frm);
                break;
            }

            // 写入缓冲区
            if (ret == 0) {
                // 优先检查 libplacebo 直接 API
                if (m_pl_initialized) {
                    // 使用我们自己的 convertFrame() 直接 API！完全绕开 FFmpeg 滤镜！
                    AVFrame *processedFrame = convertFrame(frmItem.frm);
                    if (processedFrame && processedFrame != frmItem.frm) {
                        // 使用处理后的帧
                        av_frame_unref(frmItem.frm);
                        av_frame_free(&frmItem.frm);
                        frmItem.frm = av_frame_clone(processedFrame);
                    }
                }
                // 然后再检查 FFmpeg 滤镜（作为后备方案）
                else if (m_needsFilter && !m_filterGraph) {
                    if (!setupFilter(frmItem.frm)) {
                        qDebug() << "无法设置滤镜，将正常播放";
                        m_needsFilter = false;
                    }
                } else if (m_needsFilter && m_filterGraph) {
                    // 滤镜处理
                    AVFrame *processedFrame = processFilter(frmItem.frm);
                    if (processedFrame == m_filteredFrame) {
                        // 使用处理后的帧
                        av_frame_unref(frmItem.frm);
                        av_frame_free(&frmItem.frm);
                        frmItem.frm = av_frame_clone(m_filteredFrame);
                    }
                }

                double timeBase = av_q2d(m_time_base);
                frmItem.pts = (frmItem.frm->best_effort_timestamp == AV_NOPTS_VALUE) ? frmItem.frm->pts * timeBase : frmItem.frm->best_effort_timestamp * timeBase;
                frmItem.duration = frmItem.frm->duration * timeBase;
                while (!m_frmBuf->push(frmItem)) {
                    if (m_stop.load(std::memory_order_relaxed)) {
                        goto end;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                frmItem.frm = nullptr;
            } else {
                qDebug() << "读取videofrm错误:" << ret;
                goto end;
            }
        }

        PlaybackStats::instance().updateVideoDecodeTime(sumTime * 1000);
    }

end:
    av_packet_free(&pktItem.pkt);
    av_frame_free(&frmItem.frm);
}
