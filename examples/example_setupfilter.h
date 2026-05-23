// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EXAMPLE_SETUPFILTER_H
#define EXAMPLE_SETUPFILTER_H

#include <string>

#ifdef __cplusplus
#define AZ_EXTERN_C_BEGIN extern "C" {
#define AZ_EXTERN_C_END }
#else
#define AZ_EXTERN_C_BEGIN
#define AZ_EXTERN_C_END
#endif

AZ_EXTERN_C_BEGIN
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
AZ_EXTERN_C_END

class ExampleDecodeVideo {
public:
    ExampleDecodeVideo() = default;
    ~ExampleDecodeVideo();

    bool setupFilterWithVulkan(AVFrame* inFrame, AVRational timeBase);
    void uninitFilter();

private:
    AVFilterGraph* m_filterGraph = nullptr;
    AVFilterContext* m_buffersrcCtx = nullptr;
    AVFilterContext* m_buffersinkCtx = nullptr;
    AVFilterContext* m_hwuploadCtx = nullptr;
    AVFrame* m_filteredFrame = nullptr;
    AVBufferRef* m_hwDeviceCtx = nullptr;
};

#endif // EXAMPLE_SETUPFILTER_H
