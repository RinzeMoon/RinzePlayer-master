// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ffmpeg_feature_check.h"
#include <cstdio>
#include <sstream>

AZ_EXTERN_C_BEGIN
#include <libavfilter/avfilter.h>
#include <libavutil/hwcontext.h>
AZ_EXTERN_C_END

std::string FFmpegFeatureStatus::getSummary() const {
    std::ostringstream oss;
    oss << "=== FFmpeg Feature Check Summary ===\n";
    oss << "av_hwdevice_ctx_create (Vulkan): " << (hasVulkanDeviceCreate ? "YES" : "NO") << "\n";
    oss << "libplacebo filter: " << (hasLibplaceboFilter ? "YES" : "NO") << "\n";
    oss << "hwupload filter: " << (hasHwuploadFilter ? "YES" : "NO") << "\n";
    oss << "All features available: " << ((hasVulkanDeviceCreate && hasLibplaceboFilter && hasHwuploadFilter) ? "YES" : "NO") << "\n";
    return oss.str();
}

FFmpegFeatureStatus checkFFmpegFeatures() {
    FFmpegFeatureStatus status;

    printf("[Check] Testing av_hwdevice_ctx_create (Vulkan)...\n");
    AVBufferRef* hwDeviceCtx = nullptr;
    int ret = av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0);
    if (ret >= 0) {
        status.hasVulkanDeviceCreate = true;
        printf("[OK] av_hwdevice_ctx_create works with Vulkan\n");
        av_buffer_unref(&hwDeviceCtx);
    } else {
        char errStr[256];
        av_strerror(ret, errStr, sizeof(errStr));
        printf("[FAIL] av_hwdevice_ctx_create failed: %s\n", errStr);
    }

    printf("[Check] Testing libplacebo filter...\n");
    const AVFilter* libplacebo = avfilter_get_by_name("libplacebo");
    if (libplacebo) {
        status.hasLibplaceboFilter = true;
        printf("[OK] libplacebo filter found: %s\n", libplacebo->name);
        if (libplacebo->description) {
            printf("     Description: %s\n", libplacebo->description);
        }
    } else {
        printf("[FAIL] libplacebo filter not found\n");
    }

    printf("[Check] Testing hwupload filter...\n");
    const AVFilter* hwupload = avfilter_get_by_name("hwupload");
    if (hwupload) {
        status.hasHwuploadFilter = true;
        printf("[OK] hwupload filter found: %s\n", hwupload->name);
    } else {
        printf("[FAIL] hwupload filter not found\n");
    }

    printf("\n%s", status.getSummary().c_str());

    return status;
}
