// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "example_setupfilter.h"
#include &lt;cstdio&gt;
#include &lt;cstring&gt;

ExampleDecodeVideo::~ExampleDecodeVideo() {
    uninitFilter();
}

void ExampleDecodeVideo::uninitFilter() {
    if (m_filteredFrame) {
        av_frame_free(&amp;m_filteredFrame);
        m_filteredFrame = nullptr;
    }

    if (m_filterGraph) {
        avfilter_graph_free(&amp;m_filterGraph);
        m_filterGraph = nullptr;
        m_buffersrcCtx = nullptr;
        m_buffersinkCtx = nullptr;
        m_hwuploadCtx = nullptr;
    }

    if (m_hwDeviceCtx) {
        av_buffer_unref(&amp;m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }
}

bool ExampleDecodeVideo::setupFilterWithVulkan(AVFrame *inFrame, AVRational timeBase) {
    if (!inFrame) {
        printf("[Error] setupFilterWithVulkan: inFrame is null\n");
        return false;
    }

    uninitFilter();

    printf("[Info] Setting up libplacebo filter with Vulkan\n");
    printf("[Info] Input format: %s, resolution: %dx%d\n",
           av_get_pix_fmt_name((AVPixelFormat)inFrame-&gt;format),
           inFrame-&gt;width, inFrame-&gt;height);

    m_filterGraph = avfilter_graph_alloc();
    if (!m_filterGraph) {
        printf("[Error] Failed to allocate filter graph\n");
        return false;
    }

    int ret = av_hwdevice_ctx_create(&amp;m_hwDeviceCtx, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to create Vulkan device: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] Vulkan device created successfully\n");

    m_filterGraph-&gt;hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);

    const AVFilter *bufferSrc = avfilter_get_by_name("buffer");
    const AVFilter *hwupload = avfilter_get_by_name("hwupload");
    const AVFilter *libplacebo = avfilter_get_by_name("libplacebo");
    const AVFilter *formatFilter = avfilter_get_by_name("format");
    const AVFilter *bufferSink = avfilter_get_by_name("buffersink");

    if (!bufferSrc) {
        printf("[Error] Filter 'buffer' not found\n");
        uninitFilter();
        return false;
    }
    if (!hwupload) {
        printf("[Error] Filter 'hwupload' not found\n");
        uninitFilter();
        return false;
    }
    if (!libplacebo) {
        printf("[Error] Filter 'libplacebo' not found\n");
        uninitFilter();
        return false;
    }
    if (!formatFilter) {
        printf("[Error] Filter 'format' not found\n");
        uninitFilter();
        return false;
    }
    if (!bufferSink) {
        printf("[Error] Filter 'buffersink' not found\n");
        uninitFilter();
        return false;
    }
    printf("[Info] All required filters found\n");

    AVRational sar = inFrame-&gt;sample_aspect_ratio;
    if (sar.num == 0 || sar.den == 0) {
        sar = {1, 1};
    }

    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             inFrame-&gt;width, inFrame-&gt;height, inFrame-&gt;format,
             timeBase.num, timeBase.den, sar.num, sar.den);

    ret = avfilter_graph_create_filter(&amp;m_buffersrcCtx, bufferSrc, "in", args, nullptr, m_filterGraph);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to create buffer source filter: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] Buffer source filter created\n");

    ret = avfilter_graph_create_filter(&amp;m_hwuploadCtx, hwupload, "hwupload", nullptr, nullptr, m_filterGraph);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to create hwupload filter: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] hwupload filter created\n");

    AVFilterContext *libplaceboCtx = nullptr;
    const char *plArgs = "format=rgba"
                         ":tonemapping=auto"
                         ":colorspace=bt709"
                         ":color_primaries=bt709"
                         ":color_trc=bt709"
                         ":in_color_matrix=bt2020_ncl"
                         ":in_primaries=bt2020"
                         ":in_trc=pq"
                         ":in_range=limited";

    ret = avfilter_graph_create_filter(&amp;libplaceboCtx, libplacebo, "libplacebo", plArgs, nullptr, m_filterGraph);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to create libplacebo filter: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] libplacebo filter created with args: %s\n", plArgs);

    AVFilterContext *formatCtx = nullptr;
    ret = avfilter_graph_create_filter(&amp;formatCtx, formatFilter, "format", "rgba", nullptr, m_filterGraph);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to create format filter: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] format filter created\n");

    ret = avfilter_graph_create_filter(&amp;m_buffersinkCtx, bufferSink, "out", nullptr, nullptr, m_filterGraph);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to create buffersink filter: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] buffersink filter created\n");

    ret = avfilter_link(m_buffersrcCtx, 0, m_hwuploadCtx, 0);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to link buffer to hwupload: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] buffer -&gt; hwupload linked\n");

    ret = avfilter_link(m_hwuploadCtx, 0, libplaceboCtx, 0);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to link hwupload to libplacebo: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] hwupload -&gt; libplacebo linked\n");

    ret = avfilter_link(libplaceboCtx, 0, formatCtx, 0);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to link libplacebo to format: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] libplacebo -&gt; format linked\n");

    ret = avfilter_link(formatCtx, 0, m_buffersinkCtx, 0);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to link format to buffersink: %s\n", errStr);
        uninitFilter();
        return false;
    }
    printf("[Info] format -&gt; buffersink linked\n");

    ret = avfilter_graph_config(m_filterGraph, nullptr);
    if (ret &lt; 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[Error] Failed to configure filter graph: %s\n", errStr);
        uninitFilter();
        return false;
    }

    m_filteredFrame = av_frame_alloc();
    if (!m_filteredFrame) {
        printf("[Error] Failed to allocate filtered frame\n");
        uninitFilter();
        return false;
    }

    printf("[Info] Filter graph configured successfully with Vulkan support\n");
    return true;
}
