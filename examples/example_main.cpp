// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ffmpeg_feature_check.h"
#include "vulkan_check.h"
#include <cstdio>

int main() {
    printf("========================================\n");
    printf("  RinzePlayer - Feature Check Utility\n");
    printf("========================================\n\n");

    printf("--- Vulkan Runtime Check ---\n\n");
    VulkanStatus vulkanStatus = checkVulkanRuntime();

    printf("\n--- FFmpeg Feature Check ---\n\n");
    FFmpegFeatureStatus ffmpegStatus = checkFFmpegFeatures();

    printf("\n========================================\n");
    printf("  Final Recommendation\n");
    printf("========================================\n\n");

    bool vulkanReady = vulkanStatus.dllLoaded && vulkanStatus.vkGetInstanceProcAddrFound;
    bool ffmpegReady = ffmpegStatus.hasVulkanDeviceCreate &&
                       ffmpegStatus.hasLibplaceboFilter &&
                       ffmpegStatus.hasHwuploadFilter;

    if (vulkanReady && ffmpegReady) {
        printf("[OK] All requirements met! You can use Vulkan-accelerated libplacebo.\n");
    } else {
        printf("[WARN] Some features are missing:\n");
        if (!vulkanStatus.dllLoaded) {
            printf("  - Please install Vulkan Runtime from https://vulkan.lunarg.com/sdk/home\n");
        }
        if (!ffmpegStatus.hasLibplaceboFilter) {
            printf("  - FFmpeg was not compiled with libplacebo support\n");
        }
        if (!ffmpegStatus.hasVulkanDeviceCreate) {
            printf("  - FFmpeg was not compiled with Vulkan hwdevice support\n");
        }
    }

    printf("\nPress Enter to exit...");
    getchar();

    return 0;
}
