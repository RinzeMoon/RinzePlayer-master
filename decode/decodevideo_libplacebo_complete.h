// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DECODEVIDEO_H
#define DECODEVIDEO_H
#include "decodebase.h"

AZ_EXTERN_C_BEGIN
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>

// 【条件编译】libplacebo 头文件
#ifdef LIBPLACEBO_AVAILABLE
#include <libplacebo/common.h>
#include <libplacebo/vulkan.h>
#include <libplacebo/renderer.h>
#include <libplacebo/log.h>
#include <libplacebo/utils.h>
#endif
AZ_EXTERN_C_END

class DecodeVideo : public DecodeBase {
    Q_OBJECT
public:
    using DecodeBase::DecodeBase;
    bool init(AVStream *stream, sharedPktQueue pktBuf, sharedFrmQueue frmBuf, int threadNum);
    bool uninit() override;
    ~DecodeVideo();

private:
    void decodingLoop() override;
    void uninitFilter();
    bool setupFilter(AVFrame *inFrame);
    AVFrame *processFilter(AVFrame *inFrame);

    // 【新】直接使用 libplacebo C API 的函数
    bool initLibPlacebo();
    AVFrame *processLibPlacebo(AVFrame *inFrame);
    void uninitLibPlacebo();

    // 旧的 FFmpeg 滤镜变量（保持兼容性）
    AVFilterGraph *m_filterGraph = nullptr;
    AVFilterContext *m_buffersrcCtx = nullptr;
    AVFilterContext *m_buffersinkCtx = nullptr;
    AVFrame *m_filteredFrame = nullptr;
    AVBufferRef *m_hwDeviceCtx = nullptr;

    // 【新】libplacebo 直接 API 相关变量
    bool m_pl_initialized = false;
    AVFrame *m_pl_outputFrame = nullptr;
    
#ifdef LIBPLACEBO_AVAILABLE
    pl_log m_pl_log = nullptr;
    pl_vulkan_t *m_pl_vk = nullptr;
    pl_gpu *m_pl_gpu = nullptr;
    pl_renderer *m_pl_render = nullptr;
    
    // 用于临时帧存储
    struct pl_frame m_pl_input_frame;
    struct pl_frame m_pl_output_frame;
#endif

    bool m_needsFilter = false;
};

#endif // DECODEVIDEO_H