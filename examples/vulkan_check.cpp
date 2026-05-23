// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vulkan_check.h"
#include <cstdio>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef void* PFN_vkGetInstanceProcAddr;
typedef void* PFN_vkCreateInstance;
typedef void* VkInstance;
typedef void* VkAllocationCallbacks;

struct VkApplicationInfo {
    int sType;
    const void* pNext;
    const char* pApplicationName;
    unsigned int applicationVersion;
    const char* pEngineName;
    unsigned int engineVersion;
    unsigned int apiVersion;
};

struct VkInstanceCreateInfo {
    int sType;
    const void* pNext;
    unsigned int flags;
    const VkApplicationInfo* pApplicationInfo;
    unsigned int enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    unsigned int enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

std::string VulkanStatus::getSummary() const {
    std::ostringstream oss;
    oss << "=== Vulkan Runtime Check Summary ===\n";
    oss << "vulkan-1.dll loaded: " << (dllLoaded ? "YES" : "NO") << "\n";
    oss << "vkGetInstanceProcAddr found: " << (vkGetInstanceProcAddrFound ? "YES" : "NO") << "\n";
    oss << "Can create Vulkan instance: " << (canCreateInstance ? "YES" : "NO") << "\n";
    if (!errorMessage.empty()) {
        oss << "Error: " << errorMessage << "\n";
    }
    return oss.str();
}

VulkanStatus checkVulkanRuntime() {
    VulkanStatus status;

    printf("[Check] Looking for vulkan-1.dll...\n");

#ifdef _WIN32
    HMODULE vulkanDll = LoadLibraryA("vulkan-1.dll");
    if (!vulkanDll) {
        DWORD err = GetLastError();
        char errBuf[256];
        sprintf(errBuf, "LoadLibrary failed with error code: %lu", err);
        status.errorMessage = errBuf;
        printf("[FAIL] %s\n", status.errorMessage.c_str());
        printf("[INFO] This usually means Vulkan Runtime is not installed.\n");
        printf("[INFO] You can download it from: https://vulkan.lunarg.com/sdk/home\n");
        return status;
    }
    status.dllLoaded = true;
    printf("[OK] vulkan-1.dll loaded successfully\n");

    printf("[Check] Looking for vkGetInstanceProcAddr...\n");
    PFN_vkGetInstanceProcAddr pfnGetProcAddr =
        (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkanDll, "vkGetInstanceProcAddr");
#else
    void* vulkanDll = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!vulkanDll) {
        status.errorMessage = dlerror();
        printf("[FAIL] %s\n", status.errorMessage.c_str());
        return status;
    }
    status.dllLoaded = true;
    printf("[OK] libvulkan.so.1 loaded successfully\n");

    printf("[Check] Looking for vkGetInstanceProcAddr...\n");
    PFN_vkGetInstanceProcAddr pfnGetProcAddr =
        (PFN_vkGetInstanceProcAddr)dlsym(vulkanDll, "vkGetInstanceProcAddr");
#endif

    if (!pfnGetProcAddr) {
#ifdef _WIN32
        DWORD err = GetLastError();
        char errBuf[256];
        sprintf(errBuf, "GetProcAddress failed with error code: %lu", err);
        status.errorMessage = errBuf;
#else
        status.errorMessage = dlerror();
#endif
        printf("[FAIL] %s\n", status.errorMessage.c_str());

#ifdef _WIN32
        FreeLibrary(vulkanDll);
#else
        dlclose(vulkanDll);
#endif
        return status;
    }
    status.vkGetInstanceProcAddrFound = true;
    printf("[OK] vkGetInstanceProcAddr found\n");

    printf("[Check] Testing Vulkan instance creation...\n");

    PFN_vkCreateInstance pfnCreateInstance = nullptr;
    typedef void* (*PFN_vkGetInstanceProcAddrFunc)(void*, const char*);
    pfnCreateInstance = (PFN_vkCreateInstance)((PFN_vkGetInstanceProcAddrFunc)pfnGetProcAddr)(nullptr, "vkCreateInstance");

    if (!pfnCreateInstance) {
        status.errorMessage = "Failed to get vkCreateInstance function pointer";
        printf("[FAIL] %s\n", status.errorMessage.c_str());
    } else {
        VkApplicationInfo appInfo = {};
        appInfo.sType = 0;
        appInfo.pApplicationName = "VulkanCheck";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = ((1 << 22) | (0 << 12) | 0);

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = 1;
        createInfo.pApplicationInfo = &appInfo;

        VkInstance instance = nullptr;
        typedef int (*PFN_vkCreateInstanceFunc)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
        int result = ((PFN_vkCreateInstanceFunc)pfnCreateInstance)(&createInfo, nullptr, &instance);

        if (result == 0 && instance) {
            status.canCreateInstance = true;
            printf("[OK] Vulkan instance created successfully\n");

            typedef void (*PFN_vkDestroyInstanceFunc)(VkInstance, const VkAllocationCallbacks*);
            PFN_vkDestroyInstanceFunc pfnDestroyInstance =
                (PFN_vkDestroyInstanceFunc)((PFN_vkGetInstanceProcAddrFunc)pfnGetProcAddr)(instance, "vkDestroyInstance");
            if (pfnDestroyInstance) {
                pfnDestroyInstance(instance, nullptr);
            }
        } else {
            char errBuf[256];
            sprintf(errBuf, "vkCreateInstance failed with result: %d", result);
            status.errorMessage = errBuf;
            printf("[WARN] %s\n", status.errorMessage.c_str());
            printf("[WARN] This might mean no Vulkan-capable GPU is available,\n");
            printf("[WARN] but the Vulkan Runtime is still installed.\n");
        }
    }

#ifdef _WIN32
    FreeLibrary(vulkanDll);
#else
    dlclose(vulkanDll);
#endif

    printf("\n%s", status.getSummary().c_str());

    return status;
}
