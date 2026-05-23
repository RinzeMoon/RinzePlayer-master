// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VULKAN_CHECK_H
#define VULKAN_CHECK_H

#include <string>

struct VulkanStatus {
    bool dllLoaded = false;
    bool vkGetInstanceProcAddrFound = false;
    bool canCreateInstance = false;
    std::string errorMessage;
    
    std::string getSummary() const;
};

VulkanStatus checkVulkanRuntime();

#endif // VULKAN_CHECK_H
