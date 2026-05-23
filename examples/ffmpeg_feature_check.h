// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FFMPEG_FEATURE_CHECK_H
#define FFMPEG_FEATURE_CHECK_H

#include <string>

#ifdef __cplusplus
#define AZ_EXTERN_C_BEGIN extern "C" {
#define AZ_EXTERN_C_END }
#else
#define AZ_EXTERN_C_BEGIN
#define AZ_EXTERN_C_END
#endif

struct FFmpegFeatureStatus {
    bool hasVulkanDeviceCreate = false;
    bool hasLibplaceboFilter = false;
    bool hasHwuploadFilter = false;
    
    std::string getSummary() const;
};

FFmpegFeatureStatus checkFFmpegFeatures();

#endif // FFMPEG_FEATURE_CHECK_H
