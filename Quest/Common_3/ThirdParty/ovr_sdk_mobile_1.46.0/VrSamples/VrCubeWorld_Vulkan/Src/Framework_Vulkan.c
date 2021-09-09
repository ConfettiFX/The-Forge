/************************************************************************************

Filename	:	Framework_Vulkan.c
Content		:	Vulkan Framework
Created		:	October, 2017
Authors		:	J.M.P. van Waveren

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "Framework_Vulkan.h"
#include <sys/system_properties.h>

static void ParseExtensionString(
    char* extensionNames,
    uint32_t* numExtensions,
    const char* extensionArrayPtr[],
    const uint32_t arrayCount) {
    uint32_t extensionCount = 0;
    char* nextExtensionName = extensionNames;

    while (*nextExtensionName && (extensionCount < arrayCount)) {
        extensionArrayPtr[extensionCount++] = nextExtensionName;

        // Skip to a space or null
        while (*(++nextExtensionName)) {
            if (*nextExtensionName == ' ') {
                // Null-terminate and break out of the loop
                *nextExtensionName++ = '\0';
                break;
            }
        }
    }

    *numExtensions = extensionCount;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
    VkDebugReportFlagsEXT msgFlags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t srcObject,
    size_t location,
    int32_t msgCode,
    const char* pLayerPrefix,
    const char* pMsg,
    void* pUserData) {
    const char* reportType = "Unknown";
    if ((msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0) {
        reportType = "Error";
    } else if ((msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0) {
        reportType = "Warning";
    } else if ((msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0) {
        reportType = "Performance Warning";
    } else if ((msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0) {
        reportType = "Information";
    } else if ((msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0) {
        reportType = "Debug";
    }

    ALOGE("%s: [%s] Code %d : %s", reportType, pLayerPrefix, msgCode, pMsg);
    return VK_FALSE;
}

/*
================================================================================================================================

Vulkan Instance

================================================================================================================================
*/

#define GET_INSTANCE_PROC_ADDR_EXP(function)                                              \
    instance->function =                                                                  \
        (PFN_##function)(instance->vkGetInstanceProcAddr(instance->instance, #function)); \
    assert(instance->function != NULL);
#define GET_INSTANCE_PROC_ADDR(function) GET_INSTANCE_PROC_ADDR_EXP(function)

bool ovrVkInstance_Create(
    ovrVkInstance* instance,
    const char* requiredExtensionNames,
    uint32_t requiredExtensionNamesSize) {
    memset(instance, 0, sizeof(ovrVkInstance));

#if defined(_DEBUG)
    instance->validate = VK_TRUE;
#else
    instance->validate = VK_FALSE;
#endif

    instance->loader = dlopen(VULKAN_LOADER, RTLD_NOW | RTLD_LOCAL);
    if (instance->loader == NULL) {
        ALOGE("%s not available: %s", VULKAN_LOADER, dlerror());
        return false;
    }
    instance->vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)dlsym(instance->loader, "vkGetInstanceProcAddr");
    instance->vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)dlsym(
        instance->loader, "vkEnumerateInstanceLayerProperties");
    instance->vkEnumerateInstanceExtensionProperties =
        (PFN_vkEnumerateInstanceExtensionProperties)dlsym(
            instance->loader, "vkEnumerateInstanceExtensionProperties");
    instance->vkCreateInstance = (PFN_vkCreateInstance)dlsym(instance->loader, "vkCreateInstance");

    char* extensionNames = (char*)requiredExtensionNames;

    // Add any additional instance extensions.
    // TODO: Differentiate between required and validation/debug.
    if (instance->validate) {
        strcat(extensionNames, " " VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
    strcat(extensionNames, " " VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    const char* enabledExtensionNames[32] = {0};
    uint32_t enabledExtensionCount = 0;

    ParseExtensionString(extensionNames, &enabledExtensionCount, enabledExtensionNames, 32);
#if defined(_DEBUG)
    ALOGV("Enabled Extensions: ");
    for (uint32_t i = 0; i < enabledExtensionCount; i++) {
        ALOGV("\t(%d):%s", i, enabledExtensionNames[i]);
    }
#endif

    // TODO: Differentiate between required and validation/debug.
    static const char* requestedLayers[] = {"VK_LAYER_KHRONOS_validation"};
    const uint32_t requestedCount = sizeof(requestedLayers) / sizeof(requestedLayers[0]);

    // Request debug layers
    const char* enabledLayerNames[32] = {0};
    uint32_t enabledLayerCount = 0;
    if (instance->validate) {
        uint32_t availableLayerCount = 0;
        VK(instance->vkEnumerateInstanceLayerProperties(&availableLayerCount, NULL));

        VkLayerProperties* availableLayers =
            malloc(availableLayerCount * sizeof(VkLayerProperties));
        VK(instance->vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers));

        for (uint32_t i = 0; i < requestedCount; i++) {
            for (uint32_t j = 0; j < availableLayerCount; j++) {
                if (strcmp(requestedLayers[i], availableLayers[j].layerName) == 0) {
                    enabledLayerNames[enabledLayerCount++] = requestedLayers[i];
                    break;
                }
            }
        }

        free(availableLayers);
    }

#if defined(_DEBUG)
    ALOGV("Enabled Layers ");
    for (uint32_t i = 0; i < enabledLayerCount; i++) {
        ALOGV("\t(%d):%s", i, enabledLayerNames[i]);
    }
#endif

    ALOGV("--------------------------------\n");

    const uint32_t apiMajor = VK_VERSION_MAJOR(VK_API_VERSION_1_0);
    const uint32_t apiMinor = VK_VERSION_MINOR(VK_API_VERSION_1_0);
    const uint32_t apiPatch = VK_VERSION_PATCH(VK_API_VERSION_1_0);
    ALOGV("Instance API version : %d.%d.%d\n", apiMajor, apiMinor, apiPatch);

    ALOGV("--------------------------------\n");

    // Create the instance.

    VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo;
    debugReportCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    debugReportCallbackCreateInfo.pNext = NULL;
    debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
        VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debugReportCallbackCreateInfo.pfnCallback = DebugReportCallback;
    debugReportCallbackCreateInfo.pUserData = NULL;

    VkApplicationInfo app;
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pNext = NULL;
    app.pApplicationName = "Vulkan Cubeworld";
    app.applicationVersion = 1;
    app.pEngineName = "Oculus Mobile SDK";
    app.engineVersion = 1;
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    // if validation is enabled, pass the VkDebugReportCallbackCreateInfoEXT through
    // to vkCreateInstance so validation is enabled for the vkCreateInstance call.
    instanceCreateInfo.pNext = (instance->validate) ? &debugReportCallbackCreateInfo : NULL;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &app;
    instanceCreateInfo.enabledLayerCount = enabledLayerCount;
    instanceCreateInfo.ppEnabledLayerNames =
        (const char* const*)(enabledLayerCount != 0 ? enabledLayerNames : NULL);
    instanceCreateInfo.enabledExtensionCount = enabledExtensionCount;
    instanceCreateInfo.ppEnabledExtensionNames =
        (const char* const*)(enabledExtensionCount != 0 ? enabledExtensionNames : NULL);

    VK(instance->vkCreateInstance(&instanceCreateInfo, VK_ALLOCATOR, &instance->instance));

    // Get the instance functions
    GET_INSTANCE_PROC_ADDR(vkDestroyInstance);
    GET_INSTANCE_PROC_ADDR(vkEnumeratePhysicalDevices);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceFeatures);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceFeatures2KHR);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceProperties);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceProperties2KHR);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceMemoryProperties);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceQueueFamilyProperties);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceFormatProperties);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceImageFormatProperties);
    GET_INSTANCE_PROC_ADDR(vkEnumerateDeviceExtensionProperties);
    GET_INSTANCE_PROC_ADDR(vkEnumerateDeviceLayerProperties);
    GET_INSTANCE_PROC_ADDR(vkCreateDevice);
    GET_INSTANCE_PROC_ADDR(vkGetDeviceProcAddr);

    if (instance->validate) {
        GET_INSTANCE_PROC_ADDR(vkCreateDebugReportCallbackEXT);
        GET_INSTANCE_PROC_ADDR(vkDestroyDebugReportCallbackEXT);
        if (instance->vkCreateDebugReportCallbackEXT != NULL) {
            VK(instance->vkCreateDebugReportCallbackEXT(
                instance->instance,
                &debugReportCallbackCreateInfo,
                VK_ALLOCATOR,
                &instance->debugReportCallback));
        }
    }

    return true;
}

void ovrVkInstance_Destroy(ovrVkInstance* instance) {
    if (instance->validate && instance->vkDestroyDebugReportCallbackEXT != NULL) {
        VC(instance->vkDestroyDebugReportCallbackEXT(
            instance->instance, instance->debugReportCallback, VK_ALLOCATOR));
    }

    VC(instance->vkDestroyInstance(instance->instance, VK_ALLOCATOR));

    if (instance->loader != NULL) {
        dlclose(instance->loader);
        instance->loader = NULL;
    }
}

/*
================================================================================================================================

Vulkan device.

================================================================================================================================
*/

static uint32_t VkGetMemoryTypeIndex(
    ovrVkDevice* device,
    const uint32_t typeBits,
    const VkMemoryPropertyFlags requiredProperties) {
    // Search memory types to find the index with the requested properties.
    for (uint32_t type = 0; type < device->physicalDeviceMemoryProperties.memoryTypeCount; type++) {
        if ((typeBits & (1 << type)) != 0) {
            // Test if this memory type has the required properties.
            const VkFlags propertyFlags =
                device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
            if ((propertyFlags & requiredProperties) == requiredProperties) {
                return type;
            }
        }
    }
    ALOGE("Memory type %d with properties %d not found.", typeBits, requiredProperties);
    return 0;
}

// returns whether one or more memory types has the required properties or not
static bool VkSupportsMemoryProperties(
    ovrVkDevice* device,
    const VkMemoryPropertyFlags requiredProperties) {
    for (uint32_t type = 0; type < device->physicalDeviceMemoryProperties.memoryTypeCount; type++) {
        const VkFlags propertyFlags =
            device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
        if ((propertyFlags & requiredProperties) == requiredProperties) {
            return true;
        }
    }
    return false;
}

static const VkQueueFlags requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT;
static const int queueCount = 1;

#define GET_DEVICE_PROC_ADDR_EXP(function)                                                  \
    device->function =                                                                      \
        (PFN_##function)(device->instance->vkGetDeviceProcAddr(device->device, #function)); \
    assert(device->function != NULL);
#define GET_DEVICE_PROC_ADDR(function) GET_DEVICE_PROC_ADDR_EXP(function)

bool ovrVkDevice_SelectPhysicalDevice(
    ovrVkDevice* device,
    ovrVkInstance* instance,
    const char* requiredExtensionNames,
    uint32_t requiredExtensionNamesSize) {
    memset(device, 0, sizeof(ovrVkDevice));

    char value[128];
    bool enableMultiview = true;
    if (__system_property_get("debug.oculus.cube.mv", value) > 0) {
        enableMultiview = !(atoi(value) == 0);
    }

    device->instance = instance;

    char* extensionNames = (char*)requiredExtensionNames;

    ParseExtensionString(
        extensionNames, &device->enabledExtensionCount, device->enabledExtensionNames, 32);
#if defined(_DEBUG)
    ALOGV("Requested Extensions: ");
    for (uint32_t i = 0; i < device->enabledExtensionCount; i++) {
        ALOGV("\t(%d):%s", i, device->enabledExtensionNames[i]);
    }
#endif

    // TODO: Differentiate between required and validation/debug.
    static const char* requestedLayers[] = {"VK_LAYER_KHRONOS_validation"};
    const int requestedCount = sizeof(requestedLayers) / sizeof(requestedLayers[0]);

    device->enabledLayerCount = 0;

    uint32_t physicalDeviceCount = 0;
    VK(instance->vkEnumeratePhysicalDevices(instance->instance, &physicalDeviceCount, NULL));

    VkPhysicalDevice* physicalDevices =
        (VkPhysicalDevice*)malloc(physicalDeviceCount * sizeof(VkPhysicalDevice));
    VK(instance->vkEnumeratePhysicalDevices(
        instance->instance, &physicalDeviceCount, physicalDevices));

    for (uint32_t physicalDeviceIndex = 0; physicalDeviceIndex < physicalDeviceCount;
         physicalDeviceIndex++) {
        // Get the device properties.
        VkPhysicalDeviceProperties physicalDeviceProperties;
        VC(instance->vkGetPhysicalDeviceProperties(
            physicalDevices[physicalDeviceIndex], &physicalDeviceProperties));

        const uint32_t driverMajor = VK_VERSION_MAJOR(physicalDeviceProperties.driverVersion);
        const uint32_t driverMinor = VK_VERSION_MINOR(physicalDeviceProperties.driverVersion);
        const uint32_t driverPatch = VK_VERSION_PATCH(physicalDeviceProperties.driverVersion);

        const uint32_t apiMajor = VK_VERSION_MAJOR(physicalDeviceProperties.apiVersion);
        const uint32_t apiMinor = VK_VERSION_MINOR(physicalDeviceProperties.apiVersion);
        const uint32_t apiPatch = VK_VERSION_PATCH(physicalDeviceProperties.apiVersion);

        ALOGV("--------------------------------\n");
        ALOGV("Device Name          : %s\n", physicalDeviceProperties.deviceName);
        ALOGV(
            "Device Type          : %s\n",
            ((physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                 ? "integrated GPU"
                 : ((physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                        ? "discrete GPU"
                        : ((physicalDeviceProperties.deviceType ==
                            VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
                               ? "virtual GPU"
                               : ((physicalDeviceProperties.deviceType ==
                                   VK_PHYSICAL_DEVICE_TYPE_CPU)
                                      ? "CPU"
                                      : "unknown")))));
        ALOGV(
            "Vendor ID            : 0x%04X\n",
            physicalDeviceProperties.vendorID); // http://pcidatabase.com
        ALOGV("Device ID            : 0x%04X\n", physicalDeviceProperties.deviceID);
        ALOGV("Driver Version       : %d.%d.%d\n", driverMajor, driverMinor, driverPatch);
        ALOGV("API Version          : %d.%d.%d\n", apiMajor, apiMinor, apiPatch);

        // Get the queue families.
        uint32_t queueFamilyCount = 0;
        VC(instance->vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevices[physicalDeviceIndex], &queueFamilyCount, NULL));

        VkQueueFamilyProperties* queueFamilyProperties =
            (VkQueueFamilyProperties*)malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
        VC(instance->vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDevices[physicalDeviceIndex], &queueFamilyCount, queueFamilyProperties));

        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount;
             queueFamilyIndex++) {
            const VkQueueFlags queueFlags = queueFamilyProperties[queueFamilyIndex].queueFlags;
            ALOGV(
                "%-21s%c %d =%s%s (%d queues, %d priorities)\n",
                (queueFamilyIndex == 0 ? "Queue Families" : ""),
                (queueFamilyIndex == 0 ? ':' : ' '),
                queueFamilyIndex,
                (queueFlags & VK_QUEUE_GRAPHICS_BIT) ? " graphics" : "",
                (queueFlags & VK_QUEUE_TRANSFER_BIT) ? " transfer" : "",
                queueFamilyProperties[queueFamilyIndex].queueCount,
                physicalDeviceProperties.limits.discreteQueuePriorities);
        }

        // Check if this physical device supports the required queue families.
        int workQueueFamilyIndex = -1;
        int presentQueueFamilyIndex = -1;
        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount;
             queueFamilyIndex++) {
            if ((queueFamilyProperties[queueFamilyIndex].queueFlags & requiredQueueFlags) ==
                requiredQueueFlags) {
                if ((int)queueFamilyProperties[queueFamilyIndex].queueCount >= queueCount) {
                    workQueueFamilyIndex = queueFamilyIndex;
                }
            }
            if (workQueueFamilyIndex != -1 && (presentQueueFamilyIndex != -1)) {
                break;
            }
        }

        // On Android all devices must be able to present to the system compositor, and all queue
        // families must support the necessary image layout transitions and synchronization
        // operations.
        presentQueueFamilyIndex = workQueueFamilyIndex;

        if (workQueueFamilyIndex == -1) {
            ALOGV("Required work queue family not supported.\n");
            free(queueFamilyProperties);
            continue;
        }

        ALOGV("Work Queue Family    : %d\n", workQueueFamilyIndex);
        ALOGV("Present Queue Family : %d\n", presentQueueFamilyIndex);

        // Check the device extensions.
        bool requiredExtensionsAvailable = true;

        {
            uint32_t availableExtensionCount = 0;
            VK(instance->vkEnumerateDeviceExtensionProperties(
                physicalDevices[physicalDeviceIndex], NULL, &availableExtensionCount, NULL));

            VkExtensionProperties* availableExtensions = (VkExtensionProperties*)malloc(
                availableExtensionCount * sizeof(VkExtensionProperties));
            VK(instance->vkEnumerateDeviceExtensionProperties(
                physicalDevices[physicalDeviceIndex],
                NULL,
                &availableExtensionCount,
                availableExtensions));

            for (int extensionIdx = 0; extensionIdx < availableExtensionCount; extensionIdx++) {
                if (strcmp(
                        availableExtensions[extensionIdx].extensionName,
                        VK_KHR_MULTIVIEW_EXTENSION_NAME) == 0) {
                    device->supportsMultiview = true && enableMultiview;
                }
                if (strcmp(
                        availableExtensions[extensionIdx].extensionName,
                        VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME) == 0) {
                    device->supportsFragmentDensity = true;
                }
            }

            free(availableExtensions);
        }

        if (!requiredExtensionsAvailable) {
            ALOGV("Required device extensions not supported.\n");
            free(queueFamilyProperties);
            continue;
        }

        // Enable requested device layers, if available.
        if (instance->validate) {
            uint32_t availableLayerCount = 0;
            VK(instance->vkEnumerateDeviceLayerProperties(
                physicalDevices[physicalDeviceIndex], &availableLayerCount, NULL));

            VkLayerProperties* availableLayers =
                malloc(availableLayerCount * sizeof(VkLayerProperties));
            VK(instance->vkEnumerateDeviceLayerProperties(
                physicalDevices[physicalDeviceIndex], &availableLayerCount, availableLayers));

            for (uint32_t i = 0; i < requestedCount; i++) {
                for (uint32_t j = 0; j < availableLayerCount; j++) {
                    if (strcmp(requestedLayers[i], availableLayers[j].layerName) == 0) {
                        device->enabledLayerNames[device->enabledLayerCount++] = requestedLayers[i];
                        break;
                    }
                }
            }

            free(availableLayers);

#if defined(_DEBUG)
            ALOGV("Enabled Layers ");
            for (uint32_t i = 0; i < device->enabledLayerCount; i++) {
                ALOGV("\t(%d):%s", i, device->enabledLayerNames[i]);
            }
#endif
        }

        device->physicalDevice = physicalDevices[physicalDeviceIndex];
        device->queueFamilyCount = queueFamilyCount;
        device->queueFamilyProperties = queueFamilyProperties;
        device->workQueueFamilyIndex = workQueueFamilyIndex;
        device->presentQueueFamilyIndex = presentQueueFamilyIndex;
        device->physicalDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        device->physicalDeviceFeatures.pNext = NULL;
        device->physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        device->physicalDeviceProperties.pNext = NULL;

        VkPhysicalDeviceMultiviewFeatures deviceMultiviewFeatures;
        VkPhysicalDeviceMultiviewProperties deviceMultiviewProperties;

        if (device->supportsMultiview) {
            // device feature request, including sample extensions
            deviceMultiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
            deviceMultiviewFeatures.pNext = NULL;
            deviceMultiviewFeatures.multiview = VK_FALSE;
            deviceMultiviewFeatures.multiviewGeometryShader = VK_FALSE;
            deviceMultiviewFeatures.multiviewTessellationShader = VK_FALSE;
            device->physicalDeviceFeatures.pNext = &deviceMultiviewFeatures;

            // device properties request, including sample extensions
            deviceMultiviewProperties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
            deviceMultiviewProperties.pNext = NULL;
            deviceMultiviewProperties.maxMultiviewViewCount = 0;
            deviceMultiviewProperties.maxMultiviewInstanceIndex = 0;
            device->physicalDeviceProperties.pNext = &deviceMultiviewProperties;
        }

        VC(instance->vkGetPhysicalDeviceFeatures2KHR(
            physicalDevices[physicalDeviceIndex], &device->physicalDeviceFeatures));
        VC(instance->vkGetPhysicalDeviceProperties2KHR(
            physicalDevices[physicalDeviceIndex], &device->physicalDeviceProperties));
        VC(instance->vkGetPhysicalDeviceMemoryProperties(
            physicalDevices[physicalDeviceIndex], &device->physicalDeviceMemoryProperties));

        if (device->supportsMultiview) {
            ALOGV(
                "Device %s multiview rendering, with %d views and %u max instances",
                deviceMultiviewFeatures.multiview ? "supports" : "does not support",
                deviceMultiviewProperties.maxMultiviewViewCount,
                deviceMultiviewProperties.maxMultiviewInstanceIndex);

            // only enable multiview for the app if deviceMultiviewFeatures.multiview is 1.
            device->supportsMultiview = deviceMultiviewFeatures.multiview;
        }

        if (device->supportsMultiview) {
            device->enabledExtensionNames[device->enabledExtensionCount++] =
                VK_KHR_MULTIVIEW_EXTENSION_NAME;
        }

        if (device->supportsFragmentDensity) {
            device->enabledExtensionNames[device->enabledExtensionCount++] =
                VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME;
        }

        device->supportsLazyAllocate = VkSupportsMemoryProperties(
            device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
        if (device->supportsLazyAllocate) {
            ALOGV("Device supports lazy allocated memory pools");
        }

        break;
    }

    ALOGV("--------------------------------\n");

    if (device->physicalDevice == VK_NULL_HANDLE) {
        ALOGE("No capable Vulkan physical device found.");
        return false;
    }

    // Allocate a bit mask for the available queues per family.
    device->queueFamilyUsedQueues = (uint32_t*)malloc(device->queueFamilyCount * sizeof(uint32_t));
    for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < device->queueFamilyCount;
         queueFamilyIndex++) {
        device->queueFamilyUsedQueues[queueFamilyIndex] = 0xFFFFFFFF
            << device->queueFamilyProperties[queueFamilyIndex].queueCount;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&device->queueFamilyMutex, &attr);

    return true;
}

bool ovrVkDevice_Create(ovrVkDevice* device, ovrVkInstance* instance) {
    //
    // Create the logical device
    //
    const uint32_t discreteQueuePriorities =
        device->physicalDeviceProperties.properties.limits.discreteQueuePriorities;
    // One queue, at mid priority
    const float queuePriority = (discreteQueuePriorities <= 2) ? 0.0f : 0.5f;

    // Create the device.
    VkDeviceQueueCreateInfo deviceQueueCreateInfo[2];
    deviceQueueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo[0].pNext = NULL;
    deviceQueueCreateInfo[0].flags = 0;
    deviceQueueCreateInfo[0].queueFamilyIndex = device->workQueueFamilyIndex;
    deviceQueueCreateInfo[0].queueCount = queueCount;
    deviceQueueCreateInfo[0].pQueuePriorities = &queuePriority;

    deviceQueueCreateInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo[1].pNext = NULL;
    deviceQueueCreateInfo[1].flags = 0;
    deviceQueueCreateInfo[1].queueFamilyIndex = device->presentQueueFamilyIndex;
    deviceQueueCreateInfo[1].queueCount = 1;
    deviceQueueCreateInfo[1].pQueuePriorities = NULL;

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = NULL;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1 +
        (device->presentQueueFamilyIndex != -1 &&
         device->presentQueueFamilyIndex != device->workQueueFamilyIndex);
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfo;
    deviceCreateInfo.enabledLayerCount = device->enabledLayerCount;
    deviceCreateInfo.ppEnabledLayerNames =
        (const char* const*)(device->enabledLayerCount != 0 ? device->enabledLayerNames : NULL);
    deviceCreateInfo.enabledExtensionCount = device->enabledExtensionCount;
    deviceCreateInfo.ppEnabledExtensionNames =
        (const char* const*)(device->enabledExtensionCount != 0 ? device->enabledExtensionNames : NULL);
    deviceCreateInfo.pEnabledFeatures = NULL;

    VK(instance->vkCreateDevice(
        device->physicalDevice, &deviceCreateInfo, VK_ALLOCATOR, &device->device));

    // Get the device functions.
    GET_DEVICE_PROC_ADDR(vkDestroyDevice);
    GET_DEVICE_PROC_ADDR(vkGetDeviceQueue);
    GET_DEVICE_PROC_ADDR(vkQueueSubmit);
    GET_DEVICE_PROC_ADDR(vkQueueWaitIdle);
    GET_DEVICE_PROC_ADDR(vkDeviceWaitIdle);
    GET_DEVICE_PROC_ADDR(vkAllocateMemory);
    GET_DEVICE_PROC_ADDR(vkFreeMemory);
    GET_DEVICE_PROC_ADDR(vkMapMemory);
    GET_DEVICE_PROC_ADDR(vkUnmapMemory);
    GET_DEVICE_PROC_ADDR(vkFlushMappedMemoryRanges);
    GET_DEVICE_PROC_ADDR(vkBindBufferMemory);
    GET_DEVICE_PROC_ADDR(vkBindImageMemory);
    GET_DEVICE_PROC_ADDR(vkGetBufferMemoryRequirements);
    GET_DEVICE_PROC_ADDR(vkGetImageMemoryRequirements);
    GET_DEVICE_PROC_ADDR(vkCreateFence);
    GET_DEVICE_PROC_ADDR(vkDestroyFence);
    GET_DEVICE_PROC_ADDR(vkResetFences);
    GET_DEVICE_PROC_ADDR(vkGetFenceStatus);
    GET_DEVICE_PROC_ADDR(vkWaitForFences);
    GET_DEVICE_PROC_ADDR(vkCreateBuffer);
    GET_DEVICE_PROC_ADDR(vkDestroyBuffer);
    GET_DEVICE_PROC_ADDR(vkCreateImage);
    GET_DEVICE_PROC_ADDR(vkDestroyImage);
    GET_DEVICE_PROC_ADDR(vkCreateImageView);
    GET_DEVICE_PROC_ADDR(vkDestroyImageView);
    GET_DEVICE_PROC_ADDR(vkCreateShaderModule);
    GET_DEVICE_PROC_ADDR(vkDestroyShaderModule);
    GET_DEVICE_PROC_ADDR(vkCreatePipelineCache);
    GET_DEVICE_PROC_ADDR(vkDestroyPipelineCache);
    GET_DEVICE_PROC_ADDR(vkCreateGraphicsPipelines);
    GET_DEVICE_PROC_ADDR(vkDestroyPipeline);

    GET_DEVICE_PROC_ADDR(vkCreatePipelineLayout);
    GET_DEVICE_PROC_ADDR(vkDestroyPipelineLayout);
    GET_DEVICE_PROC_ADDR(vkCreateSampler);
    GET_DEVICE_PROC_ADDR(vkDestroySampler);
    GET_DEVICE_PROC_ADDR(vkCreateDescriptorSetLayout);
    GET_DEVICE_PROC_ADDR(vkDestroyDescriptorSetLayout);
    GET_DEVICE_PROC_ADDR(vkCreateDescriptorPool);
    GET_DEVICE_PROC_ADDR(vkDestroyDescriptorPool);
    GET_DEVICE_PROC_ADDR(vkResetDescriptorPool);
    GET_DEVICE_PROC_ADDR(vkAllocateDescriptorSets);
    GET_DEVICE_PROC_ADDR(vkFreeDescriptorSets);
    GET_DEVICE_PROC_ADDR(vkUpdateDescriptorSets);
    GET_DEVICE_PROC_ADDR(vkCreateFramebuffer);
    GET_DEVICE_PROC_ADDR(vkDestroyFramebuffer);
    GET_DEVICE_PROC_ADDR(vkCreateRenderPass);
    GET_DEVICE_PROC_ADDR(vkDestroyRenderPass);

    GET_DEVICE_PROC_ADDR(vkCreateCommandPool);
    GET_DEVICE_PROC_ADDR(vkDestroyCommandPool);
    GET_DEVICE_PROC_ADDR(vkResetCommandPool);
    GET_DEVICE_PROC_ADDR(vkAllocateCommandBuffers);
    GET_DEVICE_PROC_ADDR(vkFreeCommandBuffers);

    GET_DEVICE_PROC_ADDR(vkBeginCommandBuffer);
    GET_DEVICE_PROC_ADDR(vkEndCommandBuffer);
    GET_DEVICE_PROC_ADDR(vkResetCommandBuffer);

    GET_DEVICE_PROC_ADDR(vkCmdBindPipeline);
    GET_DEVICE_PROC_ADDR(vkCmdSetViewport);
    GET_DEVICE_PROC_ADDR(vkCmdSetScissor);

    GET_DEVICE_PROC_ADDR(vkCmdBindDescriptorSets);
    GET_DEVICE_PROC_ADDR(vkCmdBindIndexBuffer);
    GET_DEVICE_PROC_ADDR(vkCmdBindVertexBuffers);

    GET_DEVICE_PROC_ADDR(vkCmdDrawIndexed);

    GET_DEVICE_PROC_ADDR(vkCmdCopyBuffer);
    GET_DEVICE_PROC_ADDR(vkCmdResolveImage);
    GET_DEVICE_PROC_ADDR(vkCmdPipelineBarrier);

    GET_DEVICE_PROC_ADDR(vkCmdPushConstants);

    GET_DEVICE_PROC_ADDR(vkCmdBeginRenderPass);

    GET_DEVICE_PROC_ADDR(vkCmdEndRenderPass);

    return true;
}

void ovrVkDevice_Destroy(ovrVkDevice* device) {
    VK(device->vkDeviceWaitIdle(device->device));

    free(device->queueFamilyProperties);
    free(device->queueFamilyUsedQueues);

    pthread_mutex_destroy(&device->queueFamilyMutex);

    VC(device->vkDestroyDevice(device->device, VK_ALLOCATOR));
}

void ovrGpuDevice_CreateShader(
    ovrVkDevice* device,
    VkShaderModule* shaderModule,
    const VkShaderStageFlagBits stage,
    const void* code,
    size_t codeSize) {
    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext = NULL;
    moduleCreateInfo.flags = 0;
    moduleCreateInfo.codeSize = 0;
    moduleCreateInfo.pCode = NULL;

    if (*(uint32_t*)code == ICD_SPV_MAGIC) {
        moduleCreateInfo.codeSize = codeSize;
        moduleCreateInfo.pCode = (uint32_t*)code;

        VK(device->vkCreateShaderModule(
            device->device, &moduleCreateInfo, VK_ALLOCATOR, shaderModule));
    } else {
        // Create fake SPV structure to feed GLSL to the driver "under the covers".
        size_t tempCodeSize = 3 * sizeof(uint32_t) + codeSize + 1;
        uint32_t* tempCode = (uint32_t*)malloc(tempCodeSize);
        tempCode[0] = ICD_SPV_MAGIC;
        tempCode[1] = 0;
        tempCode[2] = stage;
        memcpy(tempCode + 3, code, codeSize + 1);

        moduleCreateInfo.codeSize = tempCodeSize;
        moduleCreateInfo.pCode = tempCode;

        VK(device->vkCreateShaderModule(
            device->device, &moduleCreateInfo, VK_ALLOCATOR, shaderModule));

        free(tempCode);
    }
}

/*
================================================================================================================================

GPU context.

A context encapsulates a queue that is used to submit command buffers.
A context can only be used by a single thread.
For optimal performance a context should only be created at load time, not at runtime.

================================================================================================================================
*/

bool ovrVkContext_Create(ovrVkContext* context, ovrVkDevice* device, const int queueIndex) {
    memset(context, 0, sizeof(ovrVkContext));

    if (pthread_mutex_trylock(&device->queueFamilyMutex) == EBUSY) {
        pthread_mutex_lock(&device->queueFamilyMutex);
    }
    assert((device->queueFamilyUsedQueues[device->workQueueFamilyIndex] & (1 << queueIndex)) == 0);
    device->queueFamilyUsedQueues[device->workQueueFamilyIndex] |= (1 << queueIndex);
    pthread_mutex_unlock(&device->queueFamilyMutex);

    context->device = device;
    context->queueFamilyIndex = device->workQueueFamilyIndex;
    context->queueIndex = queueIndex;

    VC(device->vkGetDeviceQueue(
        device->device, context->queueFamilyIndex, context->queueIndex, &context->queue));

    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = NULL;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = context->queueFamilyIndex;

    VK(device->vkCreateCommandPool(
        device->device, &commandPoolCreateInfo, VK_ALLOCATOR, &context->commandPool));

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo;
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheCreateInfo.pNext = NULL;
    pipelineCacheCreateInfo.flags = 0;
    pipelineCacheCreateInfo.initialDataSize = 0;
    pipelineCacheCreateInfo.pInitialData = NULL;

    VK(device->vkCreatePipelineCache(
        device->device, &pipelineCacheCreateInfo, VK_ALLOCATOR, &context->pipelineCache));

    return true;
}

void ovrVkContext_Destroy(ovrVkContext* context) {
    if (context->device == NULL) {
        return;
    }

    // Mark the queue as no longer in use.
    if (pthread_mutex_trylock(&context->device->queueFamilyMutex) == EBUSY) {
        pthread_mutex_lock(&context->device->queueFamilyMutex);
    }
    assert(
        (context->device->queueFamilyUsedQueues[context->queueFamilyIndex] &
         (1 << context->queueIndex)) != 0);
    context->device->queueFamilyUsedQueues[context->queueFamilyIndex] &=
        ~(1 << context->queueIndex);
    pthread_mutex_unlock(&context->device->queueFamilyMutex);

    if (context->setupCommandBuffer) {
        VC(context->device->vkFreeCommandBuffers(
            context->device->device, context->commandPool, 1, &context->setupCommandBuffer));
    }
    VC(context->device->vkDestroyCommandPool(
        context->device->device, context->commandPool, VK_ALLOCATOR));
    VC(context->device->vkDestroyPipelineCache(
        context->device->device, context->pipelineCache, VK_ALLOCATOR));
}

void ovrVkContext_WaitIdle(ovrVkContext* context) {
    VK(context->device->vkQueueWaitIdle(context->queue));
}

void ovrVkContext_CreateSetupCmdBuffer(ovrVkContext* context) {
    if (context->setupCommandBuffer != VK_NULL_HANDLE) {
        return;
    }

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = NULL;
    commandBufferAllocateInfo.commandPool = context->commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VK(context->device->vkAllocateCommandBuffers(
        context->device->device, &commandBufferAllocateInfo, &context->setupCommandBuffer));

    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = NULL;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBufferBeginInfo.pInheritanceInfo = NULL;

    VK(context->device->vkBeginCommandBuffer(context->setupCommandBuffer, &commandBufferBeginInfo));
}

void ovrVkContext_FlushSetupCmdBuffer(ovrVkContext* context) {
    if (context->setupCommandBuffer == VK_NULL_HANDLE) {
        return;
    }

    VK(context->device->vkEndCommandBuffer(context->setupCommandBuffer));

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = NULL;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = NULL;
    submitInfo.pWaitDstStageMask = NULL;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &context->setupCommandBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = NULL;

    VK(context->device->vkQueueSubmit(context->queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK(context->device->vkQueueWaitIdle(context->queue));

    VC(context->device->vkFreeCommandBuffers(
        context->device->device, context->commandPool, 1, &context->setupCommandBuffer));
    context->setupCommandBuffer = VK_NULL_HANDLE;
}

/*
================================================================================================================================

GPU depth buffer.

This encapsulates a platform agnostic depth buffer.
For optimal performance a depth buffer should only be created at load time, not at runtime.

================================================================================================================================
*/

static VkFormat ovrGpuDepthBuffer_InternalSurfaceDepthFormat(
    const ovrSurfaceDepthFormat depthFormat) {
    return (
        (depthFormat == OVR_SURFACE_DEPTH_FORMAT_D16)
            ? VK_FORMAT_D16_UNORM
            : ((depthFormat == OVR_SURFACE_DEPTH_FORMAT_D24) ? VK_FORMAT_D24_UNORM_S8_UINT
                                                             : VK_FORMAT_UNDEFINED));
}

void ovrVkDepthBuffer_Create(
    ovrVkContext* context,
    ovrVkDepthBuffer* depthBuffer,
    const ovrSurfaceDepthFormat depthFormat,
    const ovrSampleCount sampleCount,
    const int width,
    const int height,
    const int numLayers) {
    assert(width >= 1);
    assert(height >= 1);
    assert(numLayers >= 1);

    memset(depthBuffer, 0, sizeof(ovrVkDepthBuffer));

    depthBuffer->format = depthFormat;

    if (depthFormat == OVR_SURFACE_DEPTH_FORMAT_NONE) {
        depthBuffer->internalFormat = VK_FORMAT_UNDEFINED;
        return;
    }

    depthBuffer->internalFormat = ovrGpuDepthBuffer_InternalSurfaceDepthFormat(depthFormat);

    VkImageCreateInfo imageCreateInfo;
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = NULL;
    imageCreateInfo.flags = 0;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = depthBuffer->internalFormat;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = numLayers;
    imageCreateInfo.samples = (VkSampleCountFlagBits)sampleCount;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = NULL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK(context->device->vkCreateImage(
        context->device->device, &imageCreateInfo, VK_ALLOCATOR, &depthBuffer->image));

    VkMemoryRequirements memoryRequirements;
    VC(context->device->vkGetImageMemoryRequirements(
        context->device->device, depthBuffer->image, &memoryRequirements));

    const bool isLazyAlloc = context->device->supportsLazyAllocate;
    const VkMemoryPropertyFlags depthMemFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        (isLazyAlloc ? VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT : 0);

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = NULL;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex =
        VkGetMemoryTypeIndex(context->device, memoryRequirements.memoryTypeBits, depthMemFlags);

    VK(context->device->vkAllocateMemory(
        context->device->device, &memoryAllocateInfo, VK_ALLOCATOR, &depthBuffer->memory));
    VK(context->device->vkBindImageMemory(
        context->device->device, depthBuffer->image, depthBuffer->memory, 0));

    VkImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = NULL;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.image = depthBuffer->image;
    imageViewCreateInfo.viewType =
        numLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = depthBuffer->internalFormat;
    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = numLayers;

    VK(context->device->vkCreateImageView(
        context->device->device, &imageViewCreateInfo, VK_ALLOCATOR, &depthBuffer->view));

    //
    // Set optimal image layout
    //

    {
        ovrVkContext_CreateSetupCmdBuffer(context);

        VkImageMemoryBarrier imageMemoryBarrier;
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.pNext = NULL;
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image = depthBuffer->image;
        imageMemoryBarrier.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrier.subresourceRange.layerCount = numLayers;

        const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        const VkDependencyFlags flags = 0;

        VC(context->device->vkCmdPipelineBarrier(
            context->setupCommandBuffer,
            src_stages,
            dst_stages,
            flags,
            0,
            NULL,
            0,
            NULL,
            1,
            &imageMemoryBarrier));

        ovrVkContext_FlushSetupCmdBuffer(context);
    }

    depthBuffer->imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void ovrVkDepthBuffer_Destroy(ovrVkContext* context, ovrVkDepthBuffer* depthBuffer) {
    if (depthBuffer->internalFormat == VK_FORMAT_UNDEFINED) {
        return;
    }

    VC(context->device->vkDestroyImageView(
        context->device->device, depthBuffer->view, VK_ALLOCATOR));
    VC(context->device->vkDestroyImage(context->device->device, depthBuffer->image, VK_ALLOCATOR));
    VC(context->device->vkFreeMemory(context->device->device, depthBuffer->memory, VK_ALLOCATOR));
}

/*
================================================================================================================================

GPU buffer.

A buffer maintains a block of memory for a specific use by GPU programs (vertex, index, uniform,
storage). For optimal performance a buffer should only be created at load time, not at runtime. The
best performance is typically achieved when the buffer is not host visible.

================================================================================================================================
*/

static VkBufferUsageFlags ovrGpuBuffer_GetBufferUsage(const ovrVkBufferType type) {
    return (
        (type == OVR_BUFFER_TYPE_VERTEX)
            ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            : ((type == OVR_BUFFER_TYPE_INDEX)
                   ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                   : ((type == OVR_BUFFER_TYPE_UNIFORM) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0)));
}

static VkAccessFlags ovrGpuBuffer_GetBufferAccess(const ovrVkBufferType type) {
    return (
        (type == OVR_BUFFER_TYPE_INDEX)
            ? VK_ACCESS_INDEX_READ_BIT
            : ((type == OVR_BUFFER_TYPE_VERTEX)
                   ? VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                   : ((type == OVR_BUFFER_TYPE_UNIFORM) ? VK_ACCESS_UNIFORM_READ_BIT : 0)));
}

static VkPipelineStageFlags PipelineStagesForBufferUsage(const ovrVkBufferType type) {
    return (
        (type == OVR_BUFFER_TYPE_INDEX)
            ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
            : ((type == OVR_BUFFER_TYPE_VERTEX)
                   ? VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
                   : ((type == OVR_BUFFER_TYPE_UNIFORM) ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT : 0)));
}

bool ovrVkBuffer_Create(
    ovrVkContext* context,
    ovrVkBuffer* buffer,
    const ovrVkBufferType type,
    const size_t dataSize,
    const void* data,
    const bool hostVisible) {
    memset(buffer, 0, sizeof(ovrVkBuffer));

    assert(
        dataSize <=
        context->device->physicalDeviceProperties.properties.limits.maxStorageBufferRange);

    buffer->type = type;
    buffer->size = dataSize;
    buffer->owner = true;

    VkBufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = NULL;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = dataSize;
    bufferCreateInfo.usage = ovrGpuBuffer_GetBufferUsage(type) |
        (hostVisible ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 0;
    bufferCreateInfo.pQueueFamilyIndices = NULL;

    VK(context->device->vkCreateBuffer(
        context->device->device, &bufferCreateInfo, VK_ALLOCATOR, &buffer->buffer));

    buffer->flags =
        hostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkMemoryRequirements memoryRequirements;
    VC(context->device->vkGetBufferMemoryRequirements(
        context->device->device, buffer->buffer, &memoryRequirements));

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = NULL;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex =
        VkGetMemoryTypeIndex(context->device, memoryRequirements.memoryTypeBits, buffer->flags);

    VK(context->device->vkAllocateMemory(
        context->device->device, &memoryAllocateInfo, VK_ALLOCATOR, &buffer->memory));
    VK(context->device->vkBindBufferMemory(
        context->device->device, buffer->buffer, buffer->memory, 0));

    if (data != NULL) {
        if (hostVisible) {
            void* mapped;
            VK(context->device->vkMapMemory(
                context->device->device, buffer->memory, 0, memoryRequirements.size, 0, &mapped));
            memcpy(mapped, data, dataSize);
            VC(context->device->vkUnmapMemory(context->device->device, buffer->memory));

            VkMappedMemoryRange mappedMemoryRange;
            mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            mappedMemoryRange.pNext = NULL;
            mappedMemoryRange.memory = buffer->memory;
            mappedMemoryRange.offset = 0;
            mappedMemoryRange.size = VK_WHOLE_SIZE;
            VC(context->device->vkFlushMappedMemoryRanges(
                context->device->device, 1, &mappedMemoryRange));
        } else {
            VkBufferCreateInfo stagingBufferCreateInfo;
            stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingBufferCreateInfo.pNext = NULL;
            stagingBufferCreateInfo.flags = 0;
            stagingBufferCreateInfo.size = dataSize;
            stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            stagingBufferCreateInfo.queueFamilyIndexCount = 0;
            stagingBufferCreateInfo.pQueueFamilyIndices = NULL;

            VkBuffer srcBuffer;
            VK(context->device->vkCreateBuffer(
                context->device->device, &stagingBufferCreateInfo, VK_ALLOCATOR, &srcBuffer));

            VkMemoryRequirements stagingMemoryRequirements;
            VC(context->device->vkGetBufferMemoryRequirements(
                context->device->device, srcBuffer, &stagingMemoryRequirements));

            VkMemoryAllocateInfo stagingMemoryAllocateInfo;
            stagingMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            stagingMemoryAllocateInfo.pNext = NULL;
            stagingMemoryAllocateInfo.allocationSize = stagingMemoryRequirements.size;
            stagingMemoryAllocateInfo.memoryTypeIndex = VkGetMemoryTypeIndex(
                context->device,
                stagingMemoryRequirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

            VkDeviceMemory srcMemory;
            VK(context->device->vkAllocateMemory(
                context->device->device, &stagingMemoryAllocateInfo, VK_ALLOCATOR, &srcMemory));
            VK(context->device->vkBindBufferMemory(
                context->device->device, srcBuffer, srcMemory, 0));

            void* mapped;
            VK(context->device->vkMapMemory(
                context->device->device, srcMemory, 0, stagingMemoryRequirements.size, 0, &mapped));
            memcpy(mapped, data, dataSize);
            VC(context->device->vkUnmapMemory(context->device->device, srcMemory));

            VkMappedMemoryRange mappedMemoryRange;
            mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            mappedMemoryRange.pNext = NULL;
            mappedMemoryRange.memory = srcMemory;
            mappedMemoryRange.offset = 0;
            mappedMemoryRange.size = VK_WHOLE_SIZE;
            VC(context->device->vkFlushMappedMemoryRanges(
                context->device->device, 1, &mappedMemoryRange));

            ovrVkContext_CreateSetupCmdBuffer(context);

            VkBufferCopy bufferCopy;
            bufferCopy.srcOffset = 0;
            bufferCopy.dstOffset = 0;
            bufferCopy.size = dataSize;

            VC(context->device->vkCmdCopyBuffer(
                context->setupCommandBuffer, srcBuffer, buffer->buffer, 1, &bufferCopy));

            ovrVkContext_FlushSetupCmdBuffer(context);

            VC(context->device->vkDestroyBuffer(context->device->device, srcBuffer, VK_ALLOCATOR));
            VC(context->device->vkFreeMemory(context->device->device, srcMemory, VK_ALLOCATOR));
        }
    }

    return true;
}

void ovrGpuBuffer_CreateReference(
    ovrVkContext* context,
    ovrVkBuffer* buffer,
    const ovrVkBuffer* other) {
    buffer->next = NULL;
    buffer->unusedCount = 0;
    buffer->type = other->type;
    buffer->size = other->size;
    buffer->flags = other->flags;
    buffer->buffer = other->buffer;
    buffer->memory = other->memory;
    buffer->mapped = NULL;
    buffer->owner = false;
}

void ovrVkBuffer_Destroy(ovrVkDevice* device, ovrVkBuffer* buffer) {
    if (buffer->mapped != NULL) {
        VC(device->vkUnmapMemory(device->device, buffer->memory));
    }
    if (buffer->owner) {
        VC(device->vkDestroyBuffer(device->device, buffer->buffer, VK_ALLOCATOR));
        VC(device->vkFreeMemory(device->device, buffer->memory, VK_ALLOCATOR));
    }
}

/*
================================================================================================================================

Vulkan texture.

For optimal performance a texture should only be created or modified at load time, not at runtime.
Note that the geometry code assumes the texture origin 0,0 = left-top as opposed to left-bottom.
In other words, textures are expected to be stored top-down as opposed to bottom-up.

================================================================================================================================
*/

void ovrVkTexture_UpdateSampler(ovrVkContext* context, ovrVkTexture* texture) {
    if (texture->sampler != VK_NULL_HANDLE) {
        VC(context->device->vkDestroySampler(
            context->device->device, texture->sampler, VK_ALLOCATOR));
    }

    const VkSamplerMipmapMode mipmapMode =
        ((texture->filter == OVR_TEXTURE_FILTER_NEAREST)
             ? VK_SAMPLER_MIPMAP_MODE_NEAREST
             : ((texture->filter == OVR_TEXTURE_FILTER_LINEAR) ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                                                               : (VK_SAMPLER_MIPMAP_MODE_LINEAR)));
    const VkSamplerAddressMode addressMode =
        ((texture->wrapMode == OVR_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE)
             ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
             : ((texture->wrapMode == OVR_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER)
                    ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
                    : (VK_SAMPLER_ADDRESS_MODE_REPEAT)));

    VkSamplerCreateInfo samplerCreateInfo;
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = NULL;
    samplerCreateInfo.flags = 0;
    samplerCreateInfo.magFilter =
        (texture->filter == OVR_TEXTURE_FILTER_NEAREST) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter =
        (texture->filter == OVR_TEXTURE_FILTER_NEAREST) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = mipmapMode;
    samplerCreateInfo.addressModeU = addressMode;
    samplerCreateInfo.addressModeV = addressMode;
    samplerCreateInfo.addressModeW = addressMode;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.anisotropyEnable = (texture->maxAnisotropy > 1.0f);
    samplerCreateInfo.maxAnisotropy = texture->maxAnisotropy;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = (float)texture->mipCount;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    VK(context->device->vkCreateSampler(
        context->device->device, &samplerCreateInfo, VK_ALLOCATOR, &texture->sampler));
}

static VkImageLayout LayoutForTextureUsage(const ovrVkTextureUsage usage) {
    return (
        (usage == OVR_TEXTURE_USAGE_UNDEFINED)
            ? VK_IMAGE_LAYOUT_UNDEFINED
            : ((usage == OVR_TEXTURE_USAGE_GENERAL)
                   ? VK_IMAGE_LAYOUT_GENERAL
                   : ((usage == OVR_TEXTURE_USAGE_TRANSFER_SRC)
                          ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                          : ((usage == OVR_TEXTURE_USAGE_TRANSFER_DST)
                                 ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                                 : ((usage == OVR_TEXTURE_USAGE_SAMPLED)
                                        ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                        : ((usage == OVR_TEXTURE_USAGE_STORAGE)
                                               ? VK_IMAGE_LAYOUT_GENERAL
                                               : ((usage == OVR_TEXTURE_USAGE_COLOR_ATTACHMENT)
                                                      ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                      : ((usage == OVR_TEXTURE_USAGE_PRESENTATION)
                                                             ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                                             : (VkImageLayout)0))))))));
}

static VkAccessFlags AccessForTextureUsage(const ovrVkTextureUsage usage) {
    return (
        (usage == OVR_TEXTURE_USAGE_UNDEFINED)
            ? (0)
            : ((usage == OVR_TEXTURE_USAGE_GENERAL)
                   ? (0)
                   : ((usage == OVR_TEXTURE_USAGE_TRANSFER_SRC)
                          ? (VK_ACCESS_TRANSFER_READ_BIT)
                          : ((usage == OVR_TEXTURE_USAGE_TRANSFER_DST)
                                 ? (VK_ACCESS_TRANSFER_WRITE_BIT)
                                 : ((usage == OVR_TEXTURE_USAGE_SAMPLED)
                                        ? (VK_ACCESS_SHADER_READ_BIT)
                                        : ((usage == OVR_TEXTURE_USAGE_STORAGE)
                                               ? (VK_ACCESS_SHADER_READ_BIT |
                                                  VK_ACCESS_SHADER_WRITE_BIT)
                                               : ((usage == OVR_TEXTURE_USAGE_COLOR_ATTACHMENT)
                                                      ? (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                                                      : ((usage == OVR_TEXTURE_USAGE_PRESENTATION)
                                                             ? (VK_ACCESS_MEMORY_READ_BIT)
                                                             : 0))))))));
}

static VkPipelineStageFlags PipelineStagesForTextureUsage(
    const ovrVkTextureUsage usage,
    const bool from) {
    return (
        (usage == OVR_TEXTURE_USAGE_UNDEFINED)
            ? (VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
            : ((usage == OVR_TEXTURE_USAGE_GENERAL)
                   ? (VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
                   : ((usage == OVR_TEXTURE_USAGE_TRANSFER_SRC)
                          ? (VK_PIPELINE_STAGE_TRANSFER_BIT)
                          : ((usage == OVR_TEXTURE_USAGE_TRANSFER_DST)
                                 ? (VK_PIPELINE_STAGE_TRANSFER_BIT)
                                 : ((usage == OVR_TEXTURE_USAGE_SAMPLED)
                                        ? (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
                                        : ((usage == OVR_TEXTURE_USAGE_STORAGE)
                                               ? (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
                                               : ((usage == OVR_TEXTURE_USAGE_COLOR_ATTACHMENT)
                                                      ? (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
                                                      : ((usage == OVR_TEXTURE_USAGE_PRESENTATION)
                                                             ? (from
                                                                    ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                                                    : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
                                                             : 0))))))));
}

void ovrVkTexture_ChangeUsage(
    ovrVkContext* context,
    VkCommandBuffer cmdBuffer,
    ovrVkTexture* texture,
    const ovrVkTextureUsage usage) {
    assert((texture->usageFlags & usage) != 0);

    const VkImageLayout newImageLayout = LayoutForTextureUsage(usage);

    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = NULL;
    imageMemoryBarrier.srcAccessMask = AccessForTextureUsage(texture->usage);
    imageMemoryBarrier.dstAccessMask = AccessForTextureUsage(usage);
    imageMemoryBarrier.oldLayout = texture->imageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = texture->image;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = texture->mipCount;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = texture->layerCount;

    const VkPipelineStageFlags src_stages = PipelineStagesForTextureUsage(texture->usage, true);
    const VkPipelineStageFlags dst_stages = PipelineStagesForTextureUsage(usage, false);
    const VkDependencyFlags flags = 0;

    VC(context->device->vkCmdPipelineBarrier(
        cmdBuffer, src_stages, dst_stages, flags, 0, NULL, 0, NULL, 1, &imageMemoryBarrier));

    texture->usage = usage;
    texture->imageLayout = newImageLayout;
}

static int IntegerLog2(int i) {
    int r = 0;
    int t;
    t = ((~((i >> 16) + ~0U)) >> 27) & 0x10;
    r |= t;
    i >>= t;
    t = ((~((i >> 8) + ~0U)) >> 28) & 0x08;
    r |= t;
    i >>= t;
    t = ((~((i >> 4) + ~0U)) >> 29) & 0x04;
    r |= t;
    i >>= t;
    t = ((~((i >> 2) + ~0U)) >> 30) & 0x02;
    r |= t;
    i >>= t;
    return (r | (i >> 1));
}

// 'width' must be >= 1 and <= 32768.
// 'height' must be >= 1 and <= 32768.
// 'depth' must be >= 0 and <= 32768.
// 'layerCount' must be >= 0.
// 'faceCount' must be either 1 or 6.
// 'mipCount' must be -1 or >= 1.
// 'mipCount' includes the finest level.
// 'mipCount' set to -1 will allocate the full mip chain.
bool ovrGpuTexture_CreateInternal(
    ovrVkContext* context,
    ovrVkTexture* texture,
    const char* fileName,
    const VkFormat format,
    const ovrSampleCount sampleCount,
    const int width,
    const int height,
    const int depth,
    const int layerCount,
    const int faceCount,
    const int mipCount,
    const ovrVkTextureUsageFlags usageFlags) {
    memset(texture, 0, sizeof(ovrVkTexture));

    assert(depth >= 0);
    assert(layerCount >= 0);
    assert(faceCount == 1 || faceCount == 6);

    if (width < 1 || width > 32768 || height < 1 || height > 32768 || depth < 0 || depth > 32768) {
        ALOGE("%s: Invalid texture size (%dx%dx%d)", fileName, width, height, depth);
        return false;
    }

    if (faceCount != 1 && faceCount != 6) {
        ALOGE("%s: Cube maps must have 6 faces (%d)", fileName, faceCount);
        return false;
    }

    if (faceCount == 6 && width != height) {
        ALOGE("%s: Cube maps must be square (%dx%d)", fileName, width, height);
        return false;
    }

    if (depth > 0 && layerCount > 0) {
        ALOGE("%s: 3D array textures not supported", fileName);
        return false;
    }

    const int maxDimension =
        width > height ? (width > depth ? width : depth) : (height > depth ? height : depth);
    const int maxMipLevels = (1 + IntegerLog2(maxDimension));

    if (mipCount > maxMipLevels) {
        ALOGE("%s: Too many mip levels (%d > %d)", fileName, mipCount, maxMipLevels);
        return false;
    }

    VkFormatProperties props;
    VC(context->device->instance->vkGetPhysicalDeviceFormatProperties(
        context->device->physicalDevice, format, &props));

    // If this image is sampled.
    if ((usageFlags & OVR_TEXTURE_USAGE_SAMPLED) != 0) {
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0) {
            ALOGE("%s: Texture format %d cannot be sampled", fileName, format);
            return false;
        }
    }
    // If this image is rendered to.
    if ((usageFlags & OVR_TEXTURE_USAGE_COLOR_ATTACHMENT) != 0) {
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0) {
            ALOGE("%s: Texture format %d cannot be rendered to", fileName, format);
            return false;
        }
    }
    // If this image is used for storage.
    if ((usageFlags & OVR_TEXTURE_USAGE_STORAGE) != 0) {
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0) {
            ALOGE("%s: Texture format %d cannot be used for storage", fileName, format);
            return false;
        }
    }

    const int numStorageLevels = (mipCount >= 1) ? mipCount : maxMipLevels;
    const int arrayLayerCount = faceCount * MAX(layerCount, 1);

    texture->width = width;
    texture->height = height;
    texture->depth = depth;
    texture->layerCount = arrayLayerCount;
    texture->mipCount = numStorageLevels;
    texture->sampleCount = sampleCount;
    texture->usage = OVR_TEXTURE_USAGE_UNDEFINED;
    texture->usageFlags = usageFlags;
    texture->wrapMode = OVR_TEXTURE_WRAP_MODE_REPEAT;
    texture->filter =
        (numStorageLevels > 1) ? OVR_TEXTURE_FILTER_BILINEAR : OVR_TEXTURE_FILTER_LINEAR;
    texture->maxAnisotropy = 1.0f;
    texture->format = format;

    const VkImageUsageFlags usage =
        // Must be able to copy to the image for initialization.
        ((usageFlags & OVR_TEXTURE_USAGE_TRANSFER_DST) != 0) |
        // Must be able to blit from the image to create mip maps.
        ((usageFlags & OVR_TEXTURE_USAGE_TRANSFER_SRC) != 0) |
        // If this image is sampled.
        ((usageFlags & OVR_TEXTURE_USAGE_SAMPLED) != 0 ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
        // If this image is rendered to.
        ((usageFlags & OVR_TEXTURE_USAGE_COLOR_ATTACHMENT) != 0
             ? (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
             : 0) |
        // If this image is used for storage.
        ((usageFlags & OVR_TEXTURE_USAGE_STORAGE) != 0 ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
        // if MSAA then transient
        (sampleCount > OVR_SAMPLE_COUNT_1 ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0);

    // Create tiled image.
    VkImageCreateInfo imageCreateInfo;
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = NULL;
    imageCreateInfo.flags = (faceCount == 6) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    imageCreateInfo.imageType = (depth > 0) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = MAX(depth, 1);
    imageCreateInfo.mipLevels = numStorageLevels;
    imageCreateInfo.arrayLayers = arrayLayerCount;
    imageCreateInfo.samples = (VkSampleCountFlagBits)sampleCount;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = NULL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK(context->device->vkCreateImage(
        context->device->device, &imageCreateInfo, VK_ALLOCATOR, &texture->image));

    VkMemoryRequirements memoryRequirements;
    VC(context->device->vkGetImageMemoryRequirements(
        context->device->device, texture->image, &memoryRequirements));

    const bool isLazyAlloc =
        context->device->supportsLazyAllocate && sampleCount > OVR_SAMPLE_COUNT_1;
    const VkMemoryPropertyFlags texMemFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        (isLazyAlloc ? VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT : 0);

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = NULL;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex =
        VkGetMemoryTypeIndex(context->device, memoryRequirements.memoryTypeBits, texMemFlags);

    VK(context->device->vkAllocateMemory(
        context->device->device, &memoryAllocateInfo, VK_ALLOCATOR, &texture->memory));
    VK(context->device->vkBindImageMemory(
        context->device->device, texture->image, texture->memory, 0));

    ovrVkContext_CreateSetupCmdBuffer(context);

    // Set optimal image layout for shader read access.
    {
        VkImageMemoryBarrier imageMemoryBarrier;
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.pNext = NULL;
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image = texture->image;
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrier.subresourceRange.levelCount = numStorageLevels;
        imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrier.subresourceRange.layerCount = arrayLayerCount;

        const VkPipelineStageFlags src_stages = PipelineStagesForTextureUsage(texture->usage, true);
        const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        const VkDependencyFlags flags = 0;

        VC(context->device->vkCmdPipelineBarrier(
            context->setupCommandBuffer,
            src_stages,
            dst_stages,
            flags,
            0,
            NULL,
            0,
            NULL,
            1,
            &imageMemoryBarrier));
    }

    ovrVkContext_FlushSetupCmdBuffer(context);

    texture->usage = OVR_TEXTURE_USAGE_SAMPLED;
    texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const VkImageViewType viewType =
        ((depth > 0)
             ? VK_IMAGE_VIEW_TYPE_3D
             : ((faceCount == 6)
                    ? ((layerCount > 0) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE)
                    : ((layerCount > 0) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D)));

    VkImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = NULL;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.image = texture->image;
    imageViewCreateInfo.viewType = viewType;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = numStorageLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = arrayLayerCount;

    VK(context->device->vkCreateImageView(
        context->device->device, &imageViewCreateInfo, VK_ALLOCATOR, &texture->view));

    ovrVkTexture_UpdateSampler(context, texture);

    return true;
}

bool ovrVkTexture_Create2D(
    ovrVkContext* context,
    ovrVkTexture* texture,
    const ovrVkTextureFormat format,
    const ovrSampleCount sampleCount,
    const int width,
    const int height,
    const int mipCount,
    const int numLayers,
    const ovrVkTextureUsageFlags usageFlags) {
    const int depth = 0;
    const int faceCount = 1;
    return ovrGpuTexture_CreateInternal(
        context,
        texture,
        "data",
        (VkFormat)format,
        sampleCount,
        width,
        height,
        depth,
        numLayers,
        faceCount,
        mipCount,
        usageFlags);
}

void ovrVkTexture_Destroy(ovrVkContext* context, ovrVkTexture* texture) {
    VC(context->device->vkDestroySampler(context->device->device, texture->sampler, VK_ALLOCATOR));
    // A texture created from a swapchain does not own the view, image or memory.
    if (texture->memory != VK_NULL_HANDLE) {
        VC(context->device->vkDestroyImageView(
            context->device->device, texture->view, VK_ALLOCATOR));
        VC(context->device->vkDestroyImage(context->device->device, texture->image, VK_ALLOCATOR));
        VC(context->device->vkFreeMemory(context->device->device, texture->memory, VK_ALLOCATOR));
    }
    memset(texture, 0, sizeof(ovrVkTexture));
}

/*
================================================================================================================================

Indices and vertex attributes.

================================================================================================================================
*/

void ovrVkTriangleIndexArray_Alloc(
    ovrVkTriangleIndexArray* indices,
    const int indexCount,
    const ovrTriangleIndex* data) {
    indices->indexCount = indexCount;
    indices->indexArray = (unsigned short*)malloc(indexCount * sizeof(unsigned short));
    if (data != NULL) {
        memcpy(indices->indexArray, data, indexCount * sizeof(unsigned short));
    }
    indices->buffer = NULL;
}

void ovrVkTriangleIndexArray_Free(ovrVkTriangleIndexArray* indices) {
    free(indices->indexArray);
    memset(indices, 0, sizeof(unsigned short));
}

size_t ovrGpuVertexAttributeArrays_GetDataSize(
    const ovrVkVertexAttribute* layout,
    const int vertexCount,
    const int attribsFlags) {
    size_t totalSize = 0;
    for (int i = 0; layout[i].attributeFlag != 0; i++) {
        const ovrVkVertexAttribute* v = &layout[i];
        if ((v->attributeFlag & attribsFlags) != 0) {
            totalSize += v->attributeSize;
        }
    }
    return vertexCount * totalSize;
}

void ovrVkVertexAttributeArrays_Map(
    ovrVkVertexAttributeArrays* attribs,
    void* data,
    const size_t dataSize,
    const int vertexCount,
    const int attribsFlags) {
    unsigned char* dataBytePtr = (unsigned char*)data;
    size_t offset = 0;

    for (int i = 0; attribs->layout[i].attributeFlag != 0; i++) {
        const ovrVkVertexAttribute* v = &attribs->layout[i];
        void** attribPtr = (void**)(((char*)attribs) + v->attributeOffset);
        if ((v->attributeFlag & attribsFlags) != 0) {
            *attribPtr = (dataBytePtr + offset);
            offset += vertexCount * v->attributeSize;
        } else {
            *attribPtr = NULL;
        }
    }

    assert(offset == dataSize);
}

void ovrVkVertexAttributeArrays_Alloc(
    ovrVkVertexAttributeArrays* attribs,
    const ovrVkVertexAttribute* layout,
    const int vertexCount,
    const int attribsFlags) {
    const size_t dataSize =
        ovrGpuVertexAttributeArrays_GetDataSize(layout, vertexCount, attribsFlags);
    void* data = malloc(dataSize);
    attribs->buffer = NULL;
    attribs->layout = layout;
    attribs->data = data;
    attribs->dataSize = dataSize;
    attribs->vertexCount = vertexCount;
    attribs->attribsFlags = attribsFlags;
    ovrVkVertexAttributeArrays_Map(attribs, data, dataSize, vertexCount, attribsFlags);
}

void ovrVkVertexAttributeArrays_Free(ovrVkVertexAttributeArrays* attribs) {
    free(attribs->data);
    memset(attribs, 0, sizeof(ovrVkVertexAttributeArrays));
}

/*
================================================================================================================================

Geometry.

For optimal performance geometry should only be created at load time, not at runtime.
The vertex, index and instance buffers are placed in device memory for optimal performance.
The vertex attributes are not packed. Each attribute is stored in a separate array for
optimal binning on tiling GPUs that only transform the vertex position for the binning pass.
Storing each attribute in a saparate array is preferred even on immediate-mode GPUs to avoid
wasting cache space for attributes that are not used by a particular vertex shader.

================================================================================================================================
*/

void ovrVkGeometry_Create(
    ovrVkContext* context,
    ovrVkGeometry* geometry,
    const ovrVkVertexAttributeArrays* attribs,
    const ovrVkTriangleIndexArray* indices) {
    memset(geometry, 0, sizeof(ovrVkGeometry));

    geometry->layout = attribs->layout;
    geometry->vertexAttribsFlags = attribs->attribsFlags;
    geometry->vertexCount = attribs->vertexCount;
    geometry->indexCount = indices->indexCount;

    if (attribs->buffer != NULL) {
        ovrGpuBuffer_CreateReference(context, &geometry->vertexBuffer, attribs->buffer);
    } else {
        ovrVkBuffer_Create(
            context,
            &geometry->vertexBuffer,
            OVR_BUFFER_TYPE_VERTEX,
            attribs->dataSize,
            attribs->data,
            false);
    }
    if (indices->buffer != NULL) {
        ovrGpuBuffer_CreateReference(context, &geometry->indexBuffer, indices->buffer);
    } else {
        ovrVkBuffer_Create(
            context,
            &geometry->indexBuffer,
            OVR_BUFFER_TYPE_INDEX,
            indices->indexCount * sizeof(indices->indexArray[0]),
            indices->indexArray,
            false);
    }
}

void ovrVkGeometry_Destroy(ovrVkDevice* device, ovrVkGeometry* geometry) {
    ovrVkBuffer_Destroy(device, &geometry->indexBuffer);
    ovrVkBuffer_Destroy(device, &geometry->vertexBuffer);
    if (geometry->instanceBuffer.size != 0) {
        ovrVkBuffer_Destroy(device, &geometry->instanceBuffer);
    }

    memset(geometry, 0, sizeof(ovrVkGeometry));
}

void ovrVkGeometry_AddInstanceAttributes(
    ovrVkContext* context,
    ovrVkGeometry* geometry,
    const int numInstances,
    const int instanceAttribsFlags) {
    assert(geometry->layout != NULL);
    assert((geometry->vertexAttribsFlags & instanceAttribsFlags) == 0);

    geometry->instanceCount = numInstances;
    geometry->instanceAttribsFlags = instanceAttribsFlags;

    const size_t dataSize = ovrGpuVertexAttributeArrays_GetDataSize(
        geometry->layout, numInstances, geometry->instanceAttribsFlags);

    ovrVkBuffer_Create(
        context, &geometry->instanceBuffer, OVR_BUFFER_TYPE_VERTEX, dataSize, NULL, false);
}

/*
================================================================================================================================

Vulkan render pass.

A render pass encapsulates a sequence of graphics commands that can be executed in a single tiling
pass. For optimal performance a render pass should only be created at load time, not at runtime.
Render passes cannot overlap and cannot be nested.

================================================================================================================================
*/

static VkFormat ovrGpuColorBuffer_InternalSurfaceColorFormat(
    const ovrSurfaceColorFormat colorFormat) {
    return (
        (colorFormat == OVR_SURFACE_COLOR_FORMAT_R8G8B8A8)
            ? VK_FORMAT_R8G8B8A8_UNORM
            : ((colorFormat == OVR_SURFACE_COLOR_FORMAT_B8G8R8A8)
                   ? VK_FORMAT_B8G8R8A8_UNORM
                   : ((colorFormat == OVR_SURFACE_COLOR_FORMAT_R5G6B5)
                          ? VK_FORMAT_R5G6B5_UNORM_PACK16
                          : ((colorFormat == OVR_SURFACE_COLOR_FORMAT_B5G6R5)
                                 ? VK_FORMAT_B5G6R5_UNORM_PACK16
                                 : ((VK_FORMAT_UNDEFINED))))));
}

bool ovrVkRenderPass_Create(
    ovrVkContext* context,
    ovrVkRenderPass* renderPass,
    const ovrSurfaceColorFormat colorFormat,
    const ovrSurfaceDepthFormat depthFormat,
    const ovrSampleCount sampleCount,
    const ovrVkRenderPassType type,
    const int flags,
    const ovrVector4f* clearColor,
    bool isMultiview) {
    assert(
        (context->device->physicalDeviceProperties.properties.limits.framebufferColorSampleCounts &
         (VkSampleCountFlags)sampleCount) != 0);
    assert(
        (context->device->physicalDeviceProperties.properties.limits.framebufferDepthSampleCounts &
         (VkSampleCountFlags)sampleCount) != 0);

    renderPass->type = type;
    renderPass->flags = flags;
    renderPass->colorFormat = colorFormat;
    renderPass->depthFormat = depthFormat;
    renderPass->sampleCount = sampleCount;
    renderPass->internalColorFormat = ovrGpuColorBuffer_InternalSurfaceColorFormat(colorFormat);
    renderPass->internalDepthFormat = ovrGpuDepthBuffer_InternalSurfaceDepthFormat(depthFormat);
    renderPass->internalFragmentDensityFormat = VK_FORMAT_R8G8_UNORM;
    renderPass->clearColor = *clearColor;

    uint32_t attachmentCount = 0;
    VkAttachmentDescription attachments[4];

    // Optionally use a multi-sampled attachment.
    if (sampleCount > OVR_SAMPLE_COUNT_1) {
        attachments[attachmentCount].flags = 0;
        attachments[attachmentCount].format = renderPass->internalColorFormat;
        attachments[attachmentCount].samples = (VkSampleCountFlagBits)sampleCount;
        attachments[attachmentCount].loadOp =
            ((flags & OVR_RENDERPASS_FLAG_CLEAR_COLOR_BUFFER) != 0)
            ? VK_ATTACHMENT_LOAD_OP_CLEAR
            : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].storeOp = (EXPLICIT_RESOLVE != 0)
            ? VK_ATTACHMENT_STORE_OP_STORE
            : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentCount++;
    }
    // Either render directly to, or resolve to the single-sample attachment.
    if (sampleCount <= OVR_SAMPLE_COUNT_1 || EXPLICIT_RESOLVE == 0) {
        attachments[attachmentCount].flags = 0;
        attachments[attachmentCount].format = renderPass->internalColorFormat;
        attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[attachmentCount].loadOp =
            ((flags & OVR_RENDERPASS_FLAG_CLEAR_COLOR_BUFFER) != 0 &&
             sampleCount <= OVR_SAMPLE_COUNT_1)
            ? VK_ATTACHMENT_LOAD_OP_CLEAR
            : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentCount++;
    }
    // Optionally use a depth buffer.
    if (renderPass->internalDepthFormat != VK_FORMAT_UNDEFINED) {
        attachments[attachmentCount].flags = 0;
        attachments[attachmentCount].format = renderPass->internalDepthFormat;
        attachments[attachmentCount].samples = (VkSampleCountFlagBits)sampleCount;
        attachments[attachmentCount].loadOp =
            ((flags & OVR_RENDERPASS_FLAG_CLEAR_DEPTH_BUFFER) != 0)
            ? VK_ATTACHMENT_LOAD_OP_CLEAR
            : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].initialLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachmentCount++;
    }

    uint32_t sampleMapAttachment = 0;
    if ((flags & OVR_RENDERPASS_FLAG_INCLUDE_FRAG_DENSITY) != 0) {
        sampleMapAttachment = attachmentCount;
        attachments[attachmentCount].flags = 0;
        attachments[attachmentCount].format = renderPass->internalFragmentDensityFormat;
        attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].initialLayout =
            VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
        attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
        attachmentCount++;
    }

    VkAttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference resolveAttachmentReference;
    resolveAttachmentReference.attachment = 1;
    resolveAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference;
    depthAttachmentReference.attachment =
        (sampleCount > OVR_SAMPLE_COUNT_1 && EXPLICIT_RESOLVE == 0) ? 2 : 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference fragmentDensityAttachmentReference;
    if ((flags & OVR_RENDERPASS_FLAG_INCLUDE_FRAG_DENSITY) != 0) {
        fragmentDensityAttachmentReference.attachment = sampleMapAttachment;
        fragmentDensityAttachmentReference.layout =
            VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
    }

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = NULL;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pResolveAttachments =
        (sampleCount > OVR_SAMPLE_COUNT_1 && EXPLICIT_RESOLVE == 0) ? &resolveAttachmentReference
                                                                    : NULL;
    subpassDescription.pDepthStencilAttachment =
        (renderPass->internalDepthFormat != VK_FORMAT_UNDEFINED) ? &depthAttachmentReference : NULL;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = NULL;

    VkRenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = NULL;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = attachmentCount;
    renderPassCreateInfo.pAttachments = attachments;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = NULL;

    VkRenderPassMultiviewCreateInfo multiviewCreateInfo;
    const uint32_t viewMask = 0b00000011;
    if (isMultiview) {
        multiviewCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
        multiviewCreateInfo.pNext = NULL;
        multiviewCreateInfo.subpassCount = 1;
        multiviewCreateInfo.pViewMasks = &viewMask;
        multiviewCreateInfo.dependencyCount = 0;
        multiviewCreateInfo.correlationMaskCount = 1;
        multiviewCreateInfo.pCorrelationMasks = &viewMask;

        renderPassCreateInfo.pNext = &multiviewCreateInfo;
    }

    VkRenderPassFragmentDensityMapCreateInfoEXT fragmentDensityMapCreateInfoEXT;
    if ((flags & OVR_RENDERPASS_FLAG_INCLUDE_FRAG_DENSITY) != 0) {
        fragmentDensityMapCreateInfoEXT.sType =
            VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
        fragmentDensityMapCreateInfoEXT.fragmentDensityMapAttachment =
            fragmentDensityAttachmentReference;

        fragmentDensityMapCreateInfoEXT.pNext = renderPassCreateInfo.pNext;
        renderPassCreateInfo.pNext = &fragmentDensityMapCreateInfoEXT;
    }

    VK(context->device->vkCreateRenderPass(
        context->device->device, &renderPassCreateInfo, VK_ALLOCATOR, &renderPass->renderPass));

    return true;
}

void ovrVkRenderPass_Destroy(ovrVkContext* context, ovrVkRenderPass* renderPass) {
    VC(context->device->vkDestroyRenderPass(
        context->device->device, renderPass->renderPass, VK_ALLOCATOR));
}

/*
================================================================================================================================

Vulkan framebuffer.

A framebuffer encapsulates a buffered set of textures.
For optimal performance a framebuffer should only be created at load time, not at runtime.

================================================================================================================================
*/

ovrScreenRect ovrVkFramebuffer_GetRect(const ovrVkFramebuffer* framebuffer) {
    ovrScreenRect rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = framebuffer->width;
    rect.height = framebuffer->height;
    return rect;
}

int ovrVkFramebuffer_GetBufferCount(const ovrVkFramebuffer* framebuffer) {
    return framebuffer->numBuffers;
}

ovrVkTexture* ovrVkFramebuffer_GetColorTexture(const ovrVkFramebuffer* framebuffer) {
    assert(framebuffer->colorTextures != NULL);
    return &framebuffer->colorTextures[framebuffer->currentBuffer];
}

/*
================================================================================================================================

GPU program parms and layout.

================================================================================================================================
*/

static bool ovrGpuProgramParm_IsOpaqueBinding(const ovrVkProgramParmType type) {
    return (
        (type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED)
            ? true
            : ((type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE)
                   ? true
                   : ((type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM) ? true : false)));
}

static VkDescriptorType ovrGpuProgramParm_GetDescriptorType(const ovrVkProgramParmType type) {
    return (
        (type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED)
            ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
            : ((type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE)
                   ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                   : ((type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM)
                          ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                          : VK_DESCRIPTOR_TYPE_MAX_ENUM)));
}

int ovrVkProgramParm_GetPushConstantSize(ovrVkProgramParmType type) {
    static const int parmSize[OVR_PROGRAM_PARM_TYPE_MAX] = {
        (unsigned int)0,
        (unsigned int)0,
        (unsigned int)0,
        (unsigned int)sizeof(int),
        (unsigned int)sizeof(int[2]),
        (unsigned int)sizeof(int[3]),
        (unsigned int)sizeof(int[4]),
        (unsigned int)sizeof(float),
        (unsigned int)sizeof(float[2]),
        (unsigned int)sizeof(float[3]),
        (unsigned int)sizeof(float[4]),
        (unsigned int)sizeof(float[2][2]),
        (unsigned int)sizeof(float[2][3]),
        (unsigned int)sizeof(float[2][4]),
        (unsigned int)sizeof(float[3][2]),
        (unsigned int)sizeof(float[3][3]),
        (unsigned int)sizeof(float[3][4]),
        (unsigned int)sizeof(float[4][2]),
        (unsigned int)sizeof(float[4][3]),
        (unsigned int)sizeof(float[4][4])};
    assert((sizeof(parmSize) / sizeof(parmSize[0])) == OVR_PROGRAM_PARM_TYPE_MAX);
    return parmSize[type];
}

static VkShaderStageFlags ovrGpuProgramParm_GetShaderStageFlags(const int stageFlags) {
    return ((stageFlags & OVR_PROGRAM_STAGE_FLAG_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0) |
        ((stageFlags & OVR_PROGRAM_STAGE_FLAG_FRAGMENT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0);
}

void ovrVkProgramParmLayout_Create(
    ovrVkContext* context,
    ovrVkProgramParmLayout* layout,
    const ovrVkProgramParm* parms,
    const int numParms) {
    memset(layout, 0, sizeof(ovrVkProgramParmLayout));

    layout->numParms = numParms;
    layout->parms = parms;

    int numSampledTextureBindings[OVR_PROGRAM_STAGE_MAX] = {0};
    int numStorageTextureBindings[OVR_PROGRAM_STAGE_MAX] = {0};
    int numUniformBufferBindings[OVR_PROGRAM_STAGE_MAX] = {0};
    int numStorageBufferBindings[OVR_PROGRAM_STAGE_MAX] = {0};

    int offset = 0;
    memset(layout->offsetForIndex, -1, sizeof(layout->offsetForIndex));

    for (int i = 0; i < numParms; i++) {
        if (parms[i].type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED ||
            parms[i].type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE ||
            parms[i].type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM) {
            for (int stageIndex = 0; stageIndex < OVR_PROGRAM_STAGE_MAX; stageIndex++) {
                if ((parms[i].stageFlags & (1 << stageIndex)) != 0) {
                    numSampledTextureBindings[stageIndex] +=
                        (parms[i].type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED);
                    numStorageTextureBindings[stageIndex] +=
                        (parms[i].type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE);
                    numUniformBufferBindings[stageIndex] +=
                        (parms[i].type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM);
                }
            }

            assert(parms[i].binding >= 0 && parms[i].binding < MAX_PROGRAM_PARMS);

            // Make sure each binding location is only used once.
            assert(layout->bindings[parms[i].binding] == NULL);

            layout->bindings[parms[i].binding] = &parms[i];
            if ((int)parms[i].binding >= layout->numBindings) {
                layout->numBindings = (int)parms[i].binding + 1;
            }
        } else {
            assert(layout->numPushConstants < MAX_PROGRAM_PARMS);
            layout->pushConstants[layout->numPushConstants++] = &parms[i];

            layout->offsetForIndex[parms[i].index] = offset;
            offset += ovrVkProgramParm_GetPushConstantSize(parms[i].type);
        }
    }

    // Make sure the descriptor bindings are packed.
    for (int binding = 0; binding < layout->numBindings; binding++) {
        assert(layout->bindings[binding] != NULL);
    }

    // Verify the push constant layout.
    for (int push0 = 0; push0 < layout->numPushConstants; push0++) {
        // The push constants for a pipeline cannot use more than 'maxPushConstantsSize' bytes.
        assert(
            layout->pushConstants[push0]->binding +
                ovrVkProgramParm_GetPushConstantSize(layout->pushConstants[push0]->type) <=
            (int)context->device->physicalDeviceProperties.properties.limits.maxPushConstantsSize);
        // Make sure no push constants overlap.
        for (int push1 = push0 + 1; push1 < layout->numPushConstants; push1++) {
            assert(
                layout->pushConstants[push0]->binding >= layout->pushConstants[push1]->binding +
                        ovrVkProgramParm_GetPushConstantSize(layout->pushConstants[push1]->type) ||
                layout->pushConstants[push0]->binding +
                        ovrVkProgramParm_GetPushConstantSize(layout->pushConstants[push0]->type) <=
                    layout->pushConstants[push1]->binding);
        }
    }

    // Check the descriptor limits.
    int numTotalSampledTextureBindings = 0;
    int numTotalStorageTextureBindings = 0;
    int numTotalUniformBufferBindings = 0;
    int numTotalStorageBufferBindings = 0;
    for (int stage = 0; stage < OVR_PROGRAM_STAGE_MAX; stage++) {
        assert(
            numSampledTextureBindings[stage] <=
            (int)context->device->physicalDeviceProperties.properties.limits
                .maxPerStageDescriptorSampledImages);
        assert(
            numStorageTextureBindings[stage] <=
            (int)context->device->physicalDeviceProperties.properties.limits
                .maxPerStageDescriptorStorageImages);
        assert(
            numUniformBufferBindings[stage] <=
            (int)context->device->physicalDeviceProperties.properties.limits
                .maxPerStageDescriptorUniformBuffers);
        assert(
            numStorageBufferBindings[stage] <=
            (int)context->device->physicalDeviceProperties.properties.limits
                .maxPerStageDescriptorStorageBuffers);

        numTotalSampledTextureBindings += numSampledTextureBindings[stage];
        numTotalStorageTextureBindings += numStorageTextureBindings[stage];
        numTotalUniformBufferBindings += numUniformBufferBindings[stage];
        numTotalStorageBufferBindings += numStorageBufferBindings[stage];
    }

    assert(
        numTotalSampledTextureBindings <= (int)context->device->physicalDeviceProperties.properties
                                              .limits.maxDescriptorSetSampledImages);
    assert(
        numTotalStorageTextureBindings <= (int)context->device->physicalDeviceProperties.properties
                                              .limits.maxDescriptorSetStorageImages);
    assert(
        numTotalUniformBufferBindings <= (int)context->device->physicalDeviceProperties.properties
                                             .limits.maxDescriptorSetUniformBuffers);
    assert(
        numTotalStorageBufferBindings <= (int)context->device->physicalDeviceProperties.properties
                                             .limits.maxDescriptorSetStorageBuffers);

    //
    // Create descriptor set layout and pipeline layout
    //

    {
        VkDescriptorSetLayoutBinding descriptorSetBindings[MAX_PROGRAM_PARMS];
        VkPushConstantRange pushConstantRanges[MAX_PROGRAM_PARMS];

        int numDescriptorSetBindings = 0;
        int numPushConstantRanges = 0;
        for (int i = 0; i < numParms; i++) {
            if (ovrGpuProgramParm_IsOpaqueBinding(parms[i].type)) {
                descriptorSetBindings[numDescriptorSetBindings].binding = parms[i].binding;
                descriptorSetBindings[numDescriptorSetBindings].descriptorType =
                    ovrGpuProgramParm_GetDescriptorType(parms[i].type);
                descriptorSetBindings[numDescriptorSetBindings].descriptorCount = 1;
                descriptorSetBindings[numDescriptorSetBindings].stageFlags =
                    ovrGpuProgramParm_GetShaderStageFlags(parms[i].stageFlags);
                descriptorSetBindings[numDescriptorSetBindings].pImmutableSamplers = NULL;
                numDescriptorSetBindings++;
            } else // push constant
            {
                pushConstantRanges[numPushConstantRanges].stageFlags =
                    ovrGpuProgramParm_GetShaderStageFlags(parms[i].stageFlags);
                pushConstantRanges[numPushConstantRanges].offset = parms[i].binding;
                pushConstantRanges[numPushConstantRanges].size =
                    ovrVkProgramParm_GetPushConstantSize(parms[i].type);
                numPushConstantRanges++;
            }
        }

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.pNext = NULL;
        descriptorSetLayoutCreateInfo.flags = 0;
        descriptorSetLayoutCreateInfo.bindingCount = numDescriptorSetBindings;
        descriptorSetLayoutCreateInfo.pBindings =
            (numDescriptorSetBindings != 0) ? descriptorSetBindings : NULL;

        VK(context->device->vkCreateDescriptorSetLayout(
            context->device->device,
            &descriptorSetLayoutCreateInfo,
            VK_ALLOCATOR,
            &layout->descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.pNext = NULL;
        pipelineLayoutCreateInfo.flags = 0;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &layout->descriptorSetLayout;
        pipelineLayoutCreateInfo.pushConstantRangeCount = numPushConstantRanges;
        pipelineLayoutCreateInfo.pPushConstantRanges =
            (numPushConstantRanges != 0) ? pushConstantRanges : NULL;

        VK(context->device->vkCreatePipelineLayout(
            context->device->device,
            &pipelineLayoutCreateInfo,
            VK_ALLOCATOR,
            &layout->pipelineLayout));
    }

    // Calculate a hash of the layout.
    unsigned int hash = 5381;
    for (int i = 0; i < numParms * (int)sizeof(parms[0]); i++) {
        hash = ((hash << 5) - hash) + ((const char*)parms)[i];
    }
    layout->hash = hash;
}

void ovrVkProgramParmLayout_Destroy(ovrVkContext* context, ovrVkProgramParmLayout* layout) {
    VC(context->device->vkDestroyPipelineLayout(
        context->device->device, layout->pipelineLayout, VK_ALLOCATOR));
    VC(context->device->vkDestroyDescriptorSetLayout(
        context->device->device, layout->descriptorSetLayout, VK_ALLOCATOR));
}

/*
================================================================================================================================

Vulkan graphics program.

A graphics program encapsulates a vertex and fragment program that are used to render geometry.
For optimal performance a graphics program should only be created at load time, not at runtime.

================================================================================================================================
*/

bool ovrVkGraphicsProgram_Create(
    ovrVkContext* context,
    ovrVkGraphicsProgram* program,
    const void* vertexSourceData,
    const size_t vertexSourceSize,
    const void* fragmentSourceData,
    const size_t fragmentSourceSize,
    const ovrVkProgramParm* parms,
    const int numParms,
    const ovrVkVertexAttribute* vertexLayout,
    const int vertexAttribsFlags) {
    program->vertexAttribsFlags = vertexAttribsFlags;

    ovrGpuDevice_CreateShader(
        context->device,
        &program->vertexShaderModule,
        VK_SHADER_STAGE_VERTEX_BIT,
        vertexSourceData,
        vertexSourceSize);
    ovrGpuDevice_CreateShader(
        context->device,
        &program->fragmentShaderModule,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        fragmentSourceData,
        fragmentSourceSize);

    program->pipelineStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    program->pipelineStages[0].pNext = NULL;
    program->pipelineStages[0].flags = 0;
    program->pipelineStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    program->pipelineStages[0].module = program->vertexShaderModule;
    program->pipelineStages[0].pName = "main";
    program->pipelineStages[0].pSpecializationInfo = NULL;

    program->pipelineStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    program->pipelineStages[1].pNext = NULL;
    program->pipelineStages[1].flags = 0;
    program->pipelineStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    program->pipelineStages[1].module = program->fragmentShaderModule;
    program->pipelineStages[1].pName = "main";
    program->pipelineStages[1].pSpecializationInfo = NULL;

    ovrVkProgramParmLayout_Create(context, &program->parmLayout, parms, numParms);

    return true;
}

void ovrVkGraphicsProgram_Destroy(ovrVkContext* context, ovrVkGraphicsProgram* program) {
    ovrVkProgramParmLayout_Destroy(context, &program->parmLayout);

    VC(context->device->vkDestroyShaderModule(
        context->device->device, program->vertexShaderModule, VK_ALLOCATOR));
    VC(context->device->vkDestroyShaderModule(
        context->device->device, program->fragmentShaderModule, VK_ALLOCATOR));
}

/*
================================================================================================================================

Vulkan graphics pipeline.

A graphics pipeline encapsulates the geometry, program and ROP state that is used to render.
For optimal performance a graphics pipeline should only be created at load time, not at runtime.
The vertex attribute locations are assigned here, when both the geometry and program are known,
to avoid binding vertex attributes that are not used by the vertex shader, and to avoid binding
to a discontinuous set of vertex attribute locations.

================================================================================================================================
*/

void ovrVkGraphicsPipelineParms_Init(ovrVkGraphicsPipelineParms* parms) {
    parms->rop.blendEnable = false;
    parms->rop.redWriteEnable = true;
    parms->rop.blueWriteEnable = true;
    parms->rop.greenWriteEnable = true;
    parms->rop.alphaWriteEnable = false;
    parms->rop.depthTestEnable = true;
    parms->rop.depthWriteEnable = true;
    parms->rop.frontFace = OVR_FRONT_FACE_COUNTER_CLOCKWISE;
    parms->rop.cullMode = OVR_CULL_MODE_BACK;
    parms->rop.depthCompare = OVR_COMPARE_OP_LESS_OR_EQUAL;
    parms->rop.blendColor.x = 0.0f;
    parms->rop.blendColor.y = 0.0f;
    parms->rop.blendColor.z = 0.0f;
    parms->rop.blendColor.w = 0.0f;
    parms->rop.blendOpColor = OVR_BLEND_OP_ADD;
    parms->rop.blendSrcColor = OVR_BLEND_FACTOR_ONE;
    parms->rop.blendDstColor = OVR_BLEND_FACTOR_ZERO;
    parms->rop.blendOpAlpha = OVR_BLEND_OP_ADD;
    parms->rop.blendSrcAlpha = OVR_BLEND_FACTOR_ONE;
    parms->rop.blendDstAlpha = OVR_BLEND_FACTOR_ZERO;
    parms->renderPass = NULL;
    parms->program = NULL;
    parms->geometry = NULL;
}

void InitVertexAttributes(
    const bool instance,
    const ovrVkVertexAttribute* vertexLayout,
    const int numAttribs,
    const int storedAttribsFlags,
    const int usedAttribsFlags,
    VkVertexInputAttributeDescription* attributes,
    int* attributeCount,
    VkVertexInputBindingDescription* bindings,
    int* bindingCount,
    VkDeviceSize* bindingOffsets) {
    size_t offset = 0;
    for (int i = 0; vertexLayout[i].attributeFlag != 0; i++) {
        const ovrVkVertexAttribute* v = &vertexLayout[i];
        if ((v->attributeFlag & storedAttribsFlags) != 0) {
            if ((v->attributeFlag & usedAttribsFlags) != 0) {
                for (int location = 0; location < v->locationCount; location++) {
                    attributes[*attributeCount + location].location = *attributeCount + location;
                    attributes[*attributeCount + location].binding = *bindingCount;
                    attributes[*attributeCount + location].format = (VkFormat)v->attributeFormat;
                    attributes[*attributeCount + location].offset = (uint32_t)(
                        location * v->attributeSize /
                        v->locationCount); // limited offset used for packed vertex data
                }

                bindings[*bindingCount].binding = *bindingCount;
                bindings[*bindingCount].stride = (uint32_t)v->attributeSize;
                bindings[*bindingCount].inputRate =
                    instance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;

                bindingOffsets[*bindingCount] =
                    (VkDeviceSize)offset; // memory offset within vertex buffer

                *attributeCount += v->locationCount;
                *bindingCount += 1;
            }
            offset += numAttribs * v->attributeSize;
        }
    }
}

bool ovrVkGraphicsPipeline_Create(
    ovrVkContext* context,
    ovrVkGraphicsPipeline* pipeline,
    const ovrVkGraphicsPipelineParms* parms) {
    // Make sure the geometry provides all the attributes needed by the program.
    assert(
        ((parms->geometry->vertexAttribsFlags | parms->geometry->instanceAttribsFlags) &
         parms->program->vertexAttribsFlags) == parms->program->vertexAttribsFlags);

    pipeline->rop = parms->rop;
    pipeline->program = parms->program;
    pipeline->geometry = parms->geometry;
    pipeline->vertexAttributeCount = 0;
    pipeline->vertexBindingCount = 0;

    InitVertexAttributes(
        false,
        parms->geometry->layout,
        parms->geometry->vertexCount,
        parms->geometry->vertexAttribsFlags,
        parms->program->vertexAttribsFlags,
        pipeline->vertexAttributes,
        &pipeline->vertexAttributeCount,
        pipeline->vertexBindings,
        &pipeline->vertexBindingCount,
        pipeline->vertexBindingOffsets);

    pipeline->firstInstanceBinding = pipeline->vertexBindingCount;

    InitVertexAttributes(
        true,
        parms->geometry->layout,
        parms->geometry->instanceCount,
        parms->geometry->instanceAttribsFlags,
        parms->program->vertexAttribsFlags,
        pipeline->vertexAttributes,
        &pipeline->vertexAttributeCount,
        pipeline->vertexBindings,
        &pipeline->vertexBindingCount,
        pipeline->vertexBindingOffsets);

    pipeline->vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipeline->vertexInputState.pNext = NULL;
    pipeline->vertexInputState.flags = 0;
    pipeline->vertexInputState.vertexBindingDescriptionCount = pipeline->vertexBindingCount;
    pipeline->vertexInputState.pVertexBindingDescriptions = pipeline->vertexBindings;
    pipeline->vertexInputState.vertexAttributeDescriptionCount = pipeline->vertexAttributeCount;
    pipeline->vertexInputState.pVertexAttributeDescriptions = pipeline->vertexAttributes;

    pipeline->inputAssemblyState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline->inputAssemblyState.pNext = NULL;
    pipeline->inputAssemblyState.flags = 0;
    pipeline->inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline->inputAssemblyState.primitiveRestartEnable = VK_FALSE;

    VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo;
    tessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellationStateCreateInfo.pNext = NULL;
    tessellationStateCreateInfo.flags = 0;
    tessellationStateCreateInfo.patchControlPoints = 0;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = NULL;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = NULL;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = NULL;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = NULL;
    rasterizationStateCreateInfo.flags = 0;
    // NOTE: If the depth clamping feature is not enabled, depthClampEnable must be VK_FALSE.
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = (VkCullModeFlags)parms->rop.cullMode;
    rasterizationStateCreateInfo.frontFace = (VkFrontFace)parms->rop.frontFace;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pNext = NULL;
    multisampleStateCreateInfo.flags = 0;
    multisampleStateCreateInfo.rasterizationSamples =
        (VkSampleCountFlagBits)parms->renderPass->sampleCount;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.minSampleShading = 1.0f;
    multisampleStateCreateInfo.pSampleMask = NULL;
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo;
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.pNext = NULL;
    depthStencilStateCreateInfo.flags = 0;
    depthStencilStateCreateInfo.depthTestEnable = parms->rop.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencilStateCreateInfo.depthWriteEnable = parms->rop.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencilStateCreateInfo.depthCompareOp = (VkCompareOp)parms->rop.depthCompare;
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.front.failOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.front.passOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.back.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilStateCreateInfo.minDepthBounds = 0.0f;
    depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachementState[1];
    colorBlendAttachementState[0].blendEnable = parms->rop.blendEnable ? VK_TRUE : VK_FALSE;
    colorBlendAttachementState[0].srcColorBlendFactor = (VkBlendFactor)parms->rop.blendSrcColor;
    colorBlendAttachementState[0].dstColorBlendFactor = (VkBlendFactor)parms->rop.blendDstColor;
    colorBlendAttachementState[0].colorBlendOp = (VkBlendOp)parms->rop.blendOpColor;
    colorBlendAttachementState[0].srcAlphaBlendFactor = (VkBlendFactor)parms->rop.blendSrcAlpha;
    colorBlendAttachementState[0].dstAlphaBlendFactor = (VkBlendFactor)parms->rop.blendDstAlpha;
    colorBlendAttachementState[0].alphaBlendOp = (VkBlendOp)parms->rop.blendOpAlpha;
    colorBlendAttachementState[0].colorWriteMask =
        (parms->rop.redWriteEnable ? VK_COLOR_COMPONENT_R_BIT : 0) |
        (parms->rop.blueWriteEnable ? VK_COLOR_COMPONENT_G_BIT : 0) |
        (parms->rop.greenWriteEnable ? VK_COLOR_COMPONENT_B_BIT : 0) |
        (parms->rop.alphaWriteEnable ? VK_COLOR_COMPONENT_A_BIT : 0);

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = NULL;
    colorBlendStateCreateInfo.flags = 0;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_CLEAR;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = colorBlendAttachementState;
    colorBlendStateCreateInfo.blendConstants[0] = parms->rop.blendColor.x;
    colorBlendStateCreateInfo.blendConstants[1] = parms->rop.blendColor.y;
    colorBlendStateCreateInfo.blendConstants[2] = parms->rop.blendColor.z;
    colorBlendStateCreateInfo.blendConstants[3] = parms->rop.blendColor.w;

    VkDynamicState dynamicStateEnables[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo;
    pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCreateInfo.pNext = NULL;
    pipelineDynamicStateCreateInfo.flags = 0;
    pipelineDynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStateEnables);
    pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo;
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.pNext = NULL;
    graphicsPipelineCreateInfo.flags = 0;
    graphicsPipelineCreateInfo.stageCount = 2;
    graphicsPipelineCreateInfo.pStages = parms->program->pipelineStages;
    graphicsPipelineCreateInfo.pVertexInputState = &pipeline->vertexInputState;
    graphicsPipelineCreateInfo.pInputAssemblyState = &pipeline->inputAssemblyState;
    graphicsPipelineCreateInfo.pTessellationState = &tessellationStateCreateInfo;
    graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState =
        (parms->renderPass->internalDepthFormat != VK_FORMAT_UNDEFINED)
        ? &depthStencilStateCreateInfo
        : NULL;
    graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
    graphicsPipelineCreateInfo.layout = parms->program->parmLayout.pipelineLayout;
    graphicsPipelineCreateInfo.renderPass = parms->renderPass->renderPass;
    graphicsPipelineCreateInfo.subpass = 0;
    graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    graphicsPipelineCreateInfo.basePipelineIndex = 0;

    VK(context->device->vkCreateGraphicsPipelines(
        context->device->device,
        context->pipelineCache,
        1,
        &graphicsPipelineCreateInfo,
        VK_ALLOCATOR,
        &pipeline->pipeline));

    return true;
}

void ovrVkGraphicsPipeline_Destroy(ovrVkContext* context, ovrVkGraphicsPipeline* pipeline) {
    VC(context->device->vkDestroyPipeline(
        context->device->device, pipeline->pipeline, VK_ALLOCATOR));

    memset(pipeline, 0, sizeof(ovrVkGraphicsPipeline));
}

/*
================================================================================================================================

Vulkan fence.

A fence is used to notify completion of a command buffer.
For optimal performance a fence should only be created at load time, not at runtime.

================================================================================================================================
*/

void ovrVkFence_Create(ovrVkContext* context, ovrVkFence* fence) {
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = NULL;
    fenceCreateInfo.flags = 0;

    VK(context->device->vkCreateFence(
        context->device->device, &fenceCreateInfo, VK_ALLOCATOR, &fence->fence));

    fence->submitted = false;
}

void ovrVkFence_Destroy(ovrVkContext* context, ovrVkFence* fence) {
    VC(context->device->vkDestroyFence(context->device->device, fence->fence, VK_ALLOCATOR));
    fence->fence = VK_NULL_HANDLE;
    fence->submitted = false;
}

void ovrVkFence_Submit(ovrVkContext* context, ovrVkFence* fence) {
    fence->submitted = true;
}

bool ovrVkFence_IsSignalled(ovrVkContext* context, ovrVkFence* fence) {
    if (fence == NULL || !fence->submitted) {
        return false;
    }
    VC(VkResult res = context->device->vkGetFenceStatus(context->device->device, fence->fence));
    return (res == VK_SUCCESS);
}

/*
================================================================================================================================

Vulkan program parm state.

================================================================================================================================
*/

void ovrVkProgramParmState_SetParm(
    ovrVkProgramParmState* parmState,
    const ovrVkProgramParmLayout* parmLayout,
    const int index,
    const ovrVkProgramParmType parmType,
    const void* pointer) {
    assert(index >= 0 && index < MAX_PROGRAM_PARMS);
    if (pointer != NULL) {
        bool found = false;
        for (int i = 0; i < parmLayout->numParms; i++) {
            if (parmLayout->parms[i].index == index) {
                assert(parmLayout->parms[i].type == parmType);
                found = true;
                break;
            }
        }
        // Currently parms can be set even if they are not used by the program.
        // assert( found );
    }

    parmState->parms[index] = pointer;

    const int pushConstantSize = ovrVkProgramParm_GetPushConstantSize(parmType);
    if (pushConstantSize > 0) {
        assert(parmLayout->offsetForIndex[index] >= 0);
        assert(
            parmLayout->offsetForIndex[index] + pushConstantSize <= MAX_SAVED_PUSH_CONSTANT_BYTES);
        memcpy(&parmState->data[parmLayout->offsetForIndex[index]], pointer, pushConstantSize);
    }
}

const void* ovrVkProgramParmState_NewPushConstantData(
    const ovrVkProgramParmLayout* newLayout,
    const int newPushConstantIndex,
    const ovrVkProgramParmState* newParmState,
    const ovrVkProgramParmLayout* oldLayout,
    const int oldPushConstantIndex,
    const ovrVkProgramParmState* oldParmState,
    const bool force) {
    const ovrVkProgramParm* newParm = newLayout->pushConstants[newPushConstantIndex];
    const unsigned char* newData = &newParmState->data[newLayout->offsetForIndex[newParm->index]];
    if (force || oldLayout == NULL || oldPushConstantIndex >= oldLayout->numPushConstants) {
        return newData;
    }
    const ovrVkProgramParm* oldParm = oldLayout->pushConstants[oldPushConstantIndex];
    const unsigned char* oldData = &oldParmState->data[oldLayout->offsetForIndex[oldParm->index]];
    if (newParm->type != oldParm->type || newParm->binding != oldParm->binding) {
        return newData;
    }
    const int pushConstantSize = ovrVkProgramParm_GetPushConstantSize(newParm->type);
    if (memcmp(newData, oldData, pushConstantSize) != 0) {
        return newData;
    }
    return NULL;
}

bool ovrVkProgramParmState_DescriptorsMatch(
    const ovrVkProgramParmLayout* layout1,
    const ovrVkProgramParmState* parmState1,
    const ovrVkProgramParmLayout* layout2,
    const ovrVkProgramParmState* parmState2) {
    if (layout1 == NULL || layout2 == NULL) {
        return false;
    }
    if (layout1->hash != layout2->hash) {
        return false;
    }
    for (int i = 0; i < layout1->numBindings; i++) {
        if (parmState1->parms[layout1->bindings[i]->index] !=
            parmState2->parms[layout2->bindings[i]->index]) {
            return false;
        }
    }
    return true;
}

/*
================================================================================================================================

Vulkan graphics commands.

A graphics command encapsulates all GPU state associated with a single draw call.
The pointers passed in as parameters are expected to point to unique objects that persist
at least past the submission of the command buffer into which the graphics command is
submitted. Because pointers are maintained as state, DO NOT use pointers to local
variables that will go out of scope before the command buffer is submitted.

================================================================================================================================
*/

void ovrVkGraphicsCommand_Init(ovrVkGraphicsCommand* command) {
    command->pipeline = NULL;
    command->vertexBuffer = NULL;
    command->instanceBuffer = NULL;
    memset((void*)&command->parmState, 0, sizeof(command->parmState));
    command->numInstances = 1;
}

void ovrVkGraphicsCommand_SetPipeline(
    ovrVkGraphicsCommand* command,
    const ovrVkGraphicsPipeline* pipeline) {
    command->pipeline = pipeline;
}

void ovrVkGraphicsCommand_SetParmBufferUniform(
    ovrVkGraphicsCommand* command,
    const int index,
    const ovrVkBuffer* buffer) {
    ovrVkProgramParmState_SetParm(
        &command->parmState,
        &command->pipeline->program->parmLayout,
        index,
        OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM,
        buffer);
}

void ovrVkGraphicsCommand_SetNumInstances(ovrVkGraphicsCommand* command, const int numInstances) {
    command->numInstances = numInstances;
}

/*
================================================================================================================================

Vulkan pipeline resources.

Resources, like texture and uniform buffer descriptions, that are used by a graphics or compute
pipeline.

================================================================================================================================
*/

void ovrVkPipelineResources_Create(
    ovrVkContext* context,
    ovrVkPipelineResources* resources,
    const ovrVkProgramParmLayout* parmLayout,
    const ovrVkProgramParmState* parms) {
    memset(resources, 0, sizeof(ovrVkPipelineResources));

    resources->parmLayout = parmLayout;
    memcpy((void*)&resources->parms, parms, sizeof(ovrVkProgramParmState));

    //
    // Create descriptor pool.
    //

    {
        VkDescriptorPoolSize typeCounts[MAX_PROGRAM_PARMS];

        int count = 0;
        for (int i = 0; i < parmLayout->numBindings; i++) {
            VkDescriptorType type =
                ovrGpuProgramParm_GetDescriptorType(parmLayout->bindings[i]->type);
            for (int j = 0; j < count; j++) {
                if (typeCounts[j].type == type) {
                    typeCounts[j].descriptorCount++;
                    type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
                    break;
                }
            }
            if (type != VK_DESCRIPTOR_TYPE_MAX_ENUM) {
                typeCounts[count].type = type;
                typeCounts[count].descriptorCount = 1;
                count++;
            }
        }
        if (count == 0) {
            typeCounts[count].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            typeCounts[count].descriptorCount = 1;
            count++;
        }

        VkDescriptorPoolCreateInfo destriptorPoolCreateInfo;
        destriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        destriptorPoolCreateInfo.pNext = NULL;
        destriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        destriptorPoolCreateInfo.maxSets = 1;
        destriptorPoolCreateInfo.poolSizeCount = count;
        destriptorPoolCreateInfo.pPoolSizes = (count != 0) ? typeCounts : NULL;

        VK(context->device->vkCreateDescriptorPool(
            context->device->device,
            &destriptorPoolCreateInfo,
            VK_ALLOCATOR,
            &resources->descriptorPool));
    }

    //
    // Allocated and update a descriptor set.
    //

    {
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.pNext = NULL;
        descriptorSetAllocateInfo.descriptorPool = resources->descriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.pSetLayouts = &parmLayout->descriptorSetLayout;

        VK(context->device->vkAllocateDescriptorSets(
            context->device->device, &descriptorSetAllocateInfo, &resources->descriptorSet));

        VkWriteDescriptorSet writes[MAX_PROGRAM_PARMS];
        memset(writes, 0, sizeof(writes));

        VkDescriptorImageInfo imageInfo[MAX_PROGRAM_PARMS] = {{0}};
        VkDescriptorBufferInfo bufferInfo[MAX_PROGRAM_PARMS] = {{0}};

        int numWrites = 0;
        for (int i = 0; i < parmLayout->numBindings; i++) {
            const ovrVkProgramParm* binding = parmLayout->bindings[i];

            writes[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[numWrites].pNext = NULL;
            writes[numWrites].dstSet = resources->descriptorSet;
            writes[numWrites].dstBinding = binding->binding;
            writes[numWrites].dstArrayElement = 0;
            writes[numWrites].descriptorCount = 1;
            writes[numWrites].descriptorType =
                ovrGpuProgramParm_GetDescriptorType(parmLayout->bindings[i]->type);
            writes[numWrites].pImageInfo = &imageInfo[numWrites];
            writes[numWrites].pBufferInfo = &bufferInfo[numWrites];
            writes[numWrites].pTexelBufferView = NULL;

            if (binding->type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED) {
                const ovrVkTexture* texture = (const ovrVkTexture*)parms->parms[binding->index];
                assert(texture->usage == OVR_TEXTURE_USAGE_SAMPLED);
                assert(texture->imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                imageInfo[numWrites].sampler = texture->sampler;
                imageInfo[numWrites].imageView = texture->view;
                imageInfo[numWrites].imageLayout = texture->imageLayout;
            } else if (binding->type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE) {
                const ovrVkTexture* texture = (const ovrVkTexture*)parms->parms[binding->index];
                assert(texture->usage == OVR_TEXTURE_USAGE_STORAGE);
                assert(texture->imageLayout == VK_IMAGE_LAYOUT_GENERAL);

                imageInfo[numWrites].sampler = VK_NULL_HANDLE;
                imageInfo[numWrites].imageView = texture->view;
                imageInfo[numWrites].imageLayout = texture->imageLayout;
            } else if (binding->type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM) {
                const ovrVkBuffer* buffer = (const ovrVkBuffer*)parms->parms[binding->index];
                assert(buffer->type == OVR_BUFFER_TYPE_UNIFORM);

                bufferInfo[numWrites].buffer = buffer->buffer;
                bufferInfo[numWrites].offset = 0;
                bufferInfo[numWrites].range = buffer->size;
            }

            numWrites++;
        }

        if (numWrites > 0) {
            VC(context->device->vkUpdateDescriptorSets(
                context->device->device, numWrites, writes, 0, NULL));
        }
    }
}

void ovrVkPipelineResources_Destroy(ovrVkContext* context, ovrVkPipelineResources* resources) {
    VC(context->device->vkFreeDescriptorSets(
        context->device->device, resources->descriptorPool, 1, &resources->descriptorSet));
    VC(context->device->vkDestroyDescriptorPool(
        context->device->device, resources->descriptorPool, VK_ALLOCATOR));

    memset(resources, 0, sizeof(ovrVkPipelineResources));
}

/*
================================================================================================================================

Vulkan command buffer.

A command buffer is used to record graphics and compute commands.
For optimal performance a command buffer should only be created at load time, not at runtime.
When a command is submitted, the state of the command is compared with the currently saved state,
and only the state that has changed translates into graphics API function calls.

================================================================================================================================
*/

void ovrVkCommandBuffer_Create(
    ovrVkContext* context,
    ovrVkCommandBuffer* commandBuffer,
    const ovrVkCommandBufferType type,
    const int numBuffers) {
    memset(commandBuffer, 0, sizeof(ovrVkCommandBuffer));

    commandBuffer->type = type;
    commandBuffer->numBuffers = numBuffers;
    commandBuffer->currentBuffer = 0;
    commandBuffer->context = context;
    commandBuffer->cmdBuffers = (VkCommandBuffer*)malloc(numBuffers * sizeof(VkCommandBuffer));
    commandBuffer->fences = (ovrVkFence*)malloc(numBuffers * sizeof(ovrVkFence));
    commandBuffer->mappedBuffers = (ovrVkBuffer**)malloc(numBuffers * sizeof(ovrVkBuffer*));
    commandBuffer->oldMappedBuffers = (ovrVkBuffer**)malloc(numBuffers * sizeof(ovrVkBuffer*));
    commandBuffer->pipelineResources =
        (ovrVkPipelineResources**)malloc(numBuffers * sizeof(ovrVkPipelineResources*));

    for (int i = 0; i < numBuffers; i++) {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo;
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.pNext = NULL;
        commandBufferAllocateInfo.commandPool = context->commandPool;
        commandBufferAllocateInfo.level = (type == OVR_COMMAND_BUFFER_TYPE_PRIMARY)
            ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
            : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        commandBufferAllocateInfo.commandBufferCount = 1;

        VK(context->device->vkAllocateCommandBuffers(
            context->device->device, &commandBufferAllocateInfo, &commandBuffer->cmdBuffers[i]));

        ovrVkFence_Create(context, &commandBuffer->fences[i]);

        commandBuffer->mappedBuffers[i] = NULL;
        commandBuffer->oldMappedBuffers[i] = NULL;
        commandBuffer->pipelineResources[i] = NULL;
    }
}

void ovrVkCommandBuffer_Destroy(ovrVkContext* context, ovrVkCommandBuffer* commandBuffer) {
    assert(context == commandBuffer->context);

    for (int i = 0; i < commandBuffer->numBuffers; i++) {
        VC(context->device->vkFreeCommandBuffers(
            context->device->device, context->commandPool, 1, &commandBuffer->cmdBuffers[i]));

        ovrVkFence_Destroy(context, &commandBuffer->fences[i]);

        for (ovrVkBuffer *b = commandBuffer->mappedBuffers[i], *next = NULL; b != NULL; b = next) {
            next = b->next;
            ovrVkBuffer_Destroy(context->device, b);
            free(b);
        }
        commandBuffer->mappedBuffers[i] = NULL;

        for (ovrVkBuffer *b = commandBuffer->oldMappedBuffers[i], *next = NULL; b != NULL;
             b = next) {
            next = b->next;
            ovrVkBuffer_Destroy(context->device, b);
            free(b);
        }
        commandBuffer->oldMappedBuffers[i] = NULL;

        for (ovrVkPipelineResources *r = commandBuffer->pipelineResources[i], *next = NULL;
             r != NULL;
             r = next) {
            next = r->next;
            ovrVkPipelineResources_Destroy(context, r);
            free(r);
        }
        commandBuffer->pipelineResources[i] = NULL;
    }

    free(commandBuffer->pipelineResources);
    free(commandBuffer->oldMappedBuffers);
    free(commandBuffer->mappedBuffers);
    free(commandBuffer->fences);
    free(commandBuffer->cmdBuffers);

    memset(commandBuffer, 0, sizeof(ovrVkCommandBuffer));
}

void ovrVkCommandBuffer_ManageBuffers(ovrVkCommandBuffer* commandBuffer) {
    //
    // Manage buffers.
    //

    {
        // Free any old buffers that were not reused for a number of frames.
        for (ovrVkBuffer** b = &commandBuffer->oldMappedBuffers[commandBuffer->currentBuffer];
             *b != NULL;) {
            if ((*b)->unusedCount++ >= MAX_VERTEX_BUFFER_UNUSED_COUNT) {
                ovrVkBuffer* next = (*b)->next;
                ovrVkBuffer_Destroy(commandBuffer->context->device, *b);
                free(*b);
                *b = next;
            } else {
                b = &(*b)->next;
            }
        }

        // Move the last used buffers to the list with old buffers.
        for (ovrVkBuffer *b = commandBuffer->mappedBuffers[commandBuffer->currentBuffer],
                         *next = NULL;
             b != NULL;
             b = next) {
            next = b->next;
            b->next = commandBuffer->oldMappedBuffers[commandBuffer->currentBuffer];
            commandBuffer->oldMappedBuffers[commandBuffer->currentBuffer] = b;
        }
        commandBuffer->mappedBuffers[commandBuffer->currentBuffer] = NULL;
    }

    //
    // Manage pipeline resources.
    //

    {
        // Free any pipeline resources that were not reused for a number of frames.
        for (ovrVkPipelineResources** r =
                 &commandBuffer->pipelineResources[commandBuffer->currentBuffer];
             *r != NULL;) {
            if ((*r)->unusedCount++ >= MAX_PIPELINE_RESOURCES_UNUSED_COUNT) {
                ovrVkPipelineResources* next = (*r)->next;
                ovrVkPipelineResources_Destroy(commandBuffer->context, *r);
                free(*r);
                *r = next;
            } else {
                r = &(*r)->next;
            }
        }
    }
}

void ovrVkCommandBuffer_BeginPrimary(ovrVkCommandBuffer* commandBuffer) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == NULL);
    assert(commandBuffer->currentRenderPass == NULL);

    ovrVkDevice* device = commandBuffer->context->device;

    commandBuffer->currentBuffer = (commandBuffer->currentBuffer + 1) % commandBuffer->numBuffers;

    ovrVkFence* fence = &commandBuffer->fences[commandBuffer->currentBuffer];
    if (fence->submitted) {
        VK(device->vkWaitForFences(
            device->device, 1, &fence->fence, VK_TRUE, 1ULL * 1000 * 1000 * 1000));
        VK(device->vkResetFences(device->device, 1, &fence->fence));
        fence->submitted = false;
    }

    ovrVkCommandBuffer_ManageBuffers(commandBuffer);

    ovrVkGraphicsCommand_Init(&commandBuffer->currentGraphicsState);

    VK(device->vkResetCommandBuffer(commandBuffer->cmdBuffers[commandBuffer->currentBuffer], 0));

    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = NULL;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = NULL;

    VK(device->vkBeginCommandBuffer(
        commandBuffer->cmdBuffers[commandBuffer->currentBuffer], &commandBufferBeginInfo));

    // Make sure any CPU writes are flushed.
    {
        VkMemoryBarrier memoryBarrier;
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.pNext = NULL;
        memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        memoryBarrier.dstAccessMask = 0;

        const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_HOST_BIT;
        const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const VkDependencyFlags flags = 0;

        VC(device->vkCmdPipelineBarrier(
            commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
            src_stages,
            dst_stages,
            flags,
            1,
            &memoryBarrier,
            0,
            NULL,
            0,
            NULL));
    }
}

void ovrVkCommandBuffer_EndPrimary(ovrVkCommandBuffer* commandBuffer) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == NULL);
    assert(commandBuffer->currentRenderPass == NULL);

    ovrVkDevice* device = commandBuffer->context->device;
    VK(device->vkEndCommandBuffer(commandBuffer->cmdBuffers[commandBuffer->currentBuffer]));
}

ovrVkFence* ovrVkCommandBuffer_SubmitPrimary(ovrVkCommandBuffer* commandBuffer) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == NULL);
    assert(commandBuffer->currentRenderPass == NULL);

    ovrVkDevice* device = commandBuffer->context->device;

    const VkPipelineStageFlags stageFlags[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = NULL;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = NULL;
    submitInfo.pWaitDstStageMask = NULL;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = NULL;

    ovrVkFence* fence = &commandBuffer->fences[commandBuffer->currentBuffer];
    VK(device->vkQueueSubmit(commandBuffer->context->queue, 1, &submitInfo, fence->fence));
    ovrVkFence_Submit(commandBuffer->context, fence);

    return fence;
}

void ovrVkCommandBuffer_ChangeTextureUsage(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkTexture* texture,
    const ovrVkTextureUsage usage) {
    ovrVkTexture_ChangeUsage(
        commandBuffer->context,
        commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
        texture,
        usage);
}

void ovrVkCommandBuffer_BeginFramebuffer(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkFramebuffer* framebuffer,
    const int arrayLayer,
    const ovrVkTextureUsage usage) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == NULL);
    assert(commandBuffer->currentRenderPass == NULL);
    assert(arrayLayer >= 0 && arrayLayer < framebuffer->numLayers);

    // Only advance when rendering to the first layer.
    if (arrayLayer == 0) {
        framebuffer->currentBuffer = (framebuffer->currentBuffer + 1) % framebuffer->numBuffers;
    }
    framebuffer->currentLayer = arrayLayer;

    assert(
        framebuffer->depthBuffer.internalFormat == VK_FORMAT_UNDEFINED ||
        framebuffer->depthBuffer.imageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    ovrVkCommandBuffer_ChangeTextureUsage(
        commandBuffer, &framebuffer->colorTextures[framebuffer->currentBuffer], usage);

    commandBuffer->currentFramebuffer = framebuffer;
}

void ovrVkCommandBuffer_EndFramebuffer(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkFramebuffer* framebuffer,
    const int arrayLayer,
    const ovrVkTextureUsage usage) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == framebuffer);
    assert(commandBuffer->currentRenderPass == NULL);
    assert(arrayLayer >= 0 && arrayLayer < framebuffer->numLayers);

#if EXPLICIT_RESOLVE != 0
    if (framebuffer->renderTexture.image != VK_NULL_HANDLE) {
        ovrVkCommandBuffer_ChangeTextureUsage(
            commandBuffer, &framebuffer->renderTexture, OVR_TEXTURE_USAGE_TRANSFER_SRC);
        ovrVkCommandBuffer_ChangeTextureUsage(
            commandBuffer,
            &framebuffer->colorTextures[framebuffer->currentBuffer],
            OVR_TEXTURE_USAGE_TRANSFER_DST);

        VkImageResolve region;
        region.srcOffset.x = 0;
        region.srcOffset.y = 0;
        region.srcOffset.z = 0;
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.mipLevel = 0;
        region.srcSubresource.baseArrayLayer = arrayLayer;
        region.srcSubresource.layerCount = 1;
        region.dstOffset.x = 0;
        region.dstOffset.y = 0;
        region.dstOffset.z = 0;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.mipLevel = 0;
        region.dstSubresource.baseArrayLayer = arrayLayer;
        region.dstSubresource.layerCount = 1;
        region.extent.width = framebuffer->renderTexture.width;
        region.extent.height = framebuffer->renderTexture.height;
        region.extent.depth = framebuffer->renderTexture.depth;

        commandBuffer->context->device->vkCmdResolveImage(
            commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
            framebuffer->renderTexture.image,
            framebuffer->renderTexture.imageLayout,
            framebuffer->colorTextures[framebuffer->currentBuffer].image,
            framebuffer->colorTextures[framebuffer->currentBuffer].imageLayout,
            1,
            &region);

        ovrVkCommandBuffer_ChangeTextureUsage(
            commandBuffer, &framebuffer->renderTexture, OVR_TEXTURE_USAGE_COLOR_ATTACHMENT);
    }
#endif

    ovrVkCommandBuffer_ChangeTextureUsage(
        commandBuffer, &framebuffer->colorTextures[framebuffer->currentBuffer], usage);

    commandBuffer->currentFramebuffer = NULL;
}

void ovrVkCommandBuffer_BeginRenderPass(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkRenderPass* renderPass,
    ovrVkFramebuffer* framebuffer,
    const ovrScreenRect* rect) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentRenderPass == NULL);
    assert(commandBuffer->currentFramebuffer == framebuffer);

    ovrVkDevice* device = commandBuffer->context->device;

    VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];

    uint32_t clearValueCount = 0;
    VkClearValue clearValues[3];
    memset(clearValues, 0, sizeof(clearValues));

    clearValues[clearValueCount].color.float32[0] = renderPass->clearColor.x;
    clearValues[clearValueCount].color.float32[1] = renderPass->clearColor.y;
    clearValues[clearValueCount].color.float32[2] = renderPass->clearColor.z;
    clearValues[clearValueCount].color.float32[3] = renderPass->clearColor.w;
    clearValueCount++;

    if (renderPass->sampleCount > OVR_SAMPLE_COUNT_1) {
        clearValues[clearValueCount].color.float32[0] = renderPass->clearColor.x;
        clearValues[clearValueCount].color.float32[1] = renderPass->clearColor.y;
        clearValues[clearValueCount].color.float32[2] = renderPass->clearColor.z;
        clearValues[clearValueCount].color.float32[3] = renderPass->clearColor.w;
        clearValueCount++;
    }

    if (renderPass->internalDepthFormat != VK_FORMAT_UNDEFINED) {
        clearValues[clearValueCount].depthStencil.depth = 1.0f;
        clearValues[clearValueCount].depthStencil.stencil = 0;
        clearValueCount++;
    }

    VkRenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = NULL;
    renderPassBeginInfo.renderPass = renderPass->renderPass;
    renderPassBeginInfo.framebuffer =
        framebuffer->framebuffers[framebuffer->currentBuffer + framebuffer->currentLayer];
    renderPassBeginInfo.renderArea.offset.x = rect->x;
    renderPassBeginInfo.renderArea.offset.y = rect->y;
    renderPassBeginInfo.renderArea.extent.width = rect->width;
    renderPassBeginInfo.renderArea.extent.height = rect->height;
    renderPassBeginInfo.clearValueCount = clearValueCount;
    renderPassBeginInfo.pClearValues = clearValues;

    VkSubpassContents contents = (renderPass->type == OVR_RENDERPASS_TYPE_INLINE)
        ? VK_SUBPASS_CONTENTS_INLINE
        : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;

    VC(device->vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, contents));

    commandBuffer->currentRenderPass = renderPass;
}

void ovrVkCommandBuffer_EndRenderPass(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkRenderPass* renderPass) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentRenderPass == renderPass);

    ovrVkDevice* device = commandBuffer->context->device;

    VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];

    VC(device->vkCmdEndRenderPass(cmdBuffer));

    commandBuffer->currentRenderPass = NULL;
}

void ovrVkCommandBuffer_SetViewport(ovrVkCommandBuffer* commandBuffer, const ovrScreenRect* rect) {
    ovrVkDevice* device = commandBuffer->context->device;

    VkViewport viewport;
    viewport.x = (float)rect->x;
    viewport.y = (float)rect->y;
    viewport.width = (float)rect->width;
    viewport.height = (float)rect->height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
    VC(device->vkCmdSetViewport(cmdBuffer, 0, 1, &viewport));
}

void ovrVkCommandBuffer_SetScissor(ovrVkCommandBuffer* commandBuffer, const ovrScreenRect* rect) {
    ovrVkDevice* device = commandBuffer->context->device;

    VkRect2D scissor;
    scissor.offset.x = rect->x;
    scissor.offset.y = rect->y;
    scissor.extent.width = rect->width;
    scissor.extent.height = rect->height;

    VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
    VC(device->vkCmdSetScissor(cmdBuffer, 0, 1, &scissor));
}

void ovrVkCommandBuffer_UpdateProgramParms(
    ovrVkCommandBuffer* commandBuffer,
    const ovrVkProgramParmLayout* newLayout,
    const ovrVkProgramParmLayout* oldLayout,
    const ovrVkProgramParmState* newParmState,
    const ovrVkProgramParmState* oldParmState,
    VkPipelineBindPoint bindPoint) {
    VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
    ovrVkDevice* device = commandBuffer->context->device;

    const bool descriptorsMatch =
        ovrVkProgramParmState_DescriptorsMatch(newLayout, newParmState, oldLayout, oldParmState);
    if (!descriptorsMatch) {
        // Try to find existing resources that match.
        ovrVkPipelineResources* resources = NULL;
        for (ovrVkPipelineResources* r =
                 commandBuffer->pipelineResources[commandBuffer->currentBuffer];
             r != NULL;
             r = r->next) {
            if (ovrVkProgramParmState_DescriptorsMatch(
                    newLayout, newParmState, r->parmLayout, &r->parms)) {
                r->unusedCount = 0;
                resources = r;
                break;
            }
        }

        // Create new resources if none were found.
        if (resources == NULL) {
            resources = (ovrVkPipelineResources*)malloc(sizeof(ovrVkPipelineResources));
            ovrVkPipelineResources_Create(
                commandBuffer->context, resources, newLayout, newParmState);
            resources->next = commandBuffer->pipelineResources[commandBuffer->currentBuffer];
            commandBuffer->pipelineResources[commandBuffer->currentBuffer] = resources;
        }

        VC(device->vkCmdBindDescriptorSets(
            cmdBuffer,
            bindPoint,
            newLayout->pipelineLayout,
            0,
            1,
            &resources->descriptorSet,
            0,
            NULL));
    }

    for (int i = 0; i < newLayout->numPushConstants; i++) {
        const void* data = ovrVkProgramParmState_NewPushConstantData(
            newLayout, i, newParmState, oldLayout, i, oldParmState, false);
        if (data != NULL) {
            const ovrVkProgramParm* newParm = newLayout->pushConstants[i];
            const VkShaderStageFlags stageFlags =
                ovrGpuProgramParm_GetShaderStageFlags(newParm->stageFlags);
            const uint32_t offset = (uint32_t)newParm->binding;
            const uint32_t size = (uint32_t)ovrVkProgramParm_GetPushConstantSize(newParm->type);
            VC(device->vkCmdPushConstants(
                cmdBuffer, newLayout->pipelineLayout, stageFlags, offset, size, data));
        }
    }
}

void ovrVkCommandBuffer_SubmitGraphicsCommand(
    ovrVkCommandBuffer* commandBuffer,
    const ovrVkGraphicsCommand* command) {
    assert(commandBuffer->currentRenderPass != NULL);

    ovrVkDevice* device = commandBuffer->context->device;

    VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
    const ovrVkGraphicsCommand* state = &commandBuffer->currentGraphicsState;

    // If the pipeline has changed.
    if (command->pipeline != state->pipeline) {
        VC(device->vkCmdBindPipeline(
            cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, command->pipeline->pipeline));
    }

    const ovrVkProgramParmLayout* commandLayout = &command->pipeline->program->parmLayout;
    const ovrVkProgramParmLayout* stateLayout =
        (state->pipeline != NULL) ? &state->pipeline->program->parmLayout : NULL;

    ovrVkCommandBuffer_UpdateProgramParms(
        commandBuffer,
        commandLayout,
        stateLayout,
        &command->parmState,
        &state->parmState,
        VK_PIPELINE_BIND_POINT_GRAPHICS);

    const ovrVkGeometry* geometry = command->pipeline->geometry;

    // If the geometry has changed.
    if (state->pipeline == NULL || geometry != state->pipeline->geometry ||
        command->vertexBuffer != state->vertexBuffer ||
        command->instanceBuffer != state->instanceBuffer) {
        const VkBuffer vertexBuffer = (command->vertexBuffer != NULL)
            ? command->vertexBuffer->buffer
            : geometry->vertexBuffer.buffer;
        for (int i = 0; i < command->pipeline->firstInstanceBinding; i++) {
            VC(device->vkCmdBindVertexBuffers(
                cmdBuffer, i, 1, &vertexBuffer, &command->pipeline->vertexBindingOffsets[i]));
        }

        const VkBuffer instanceBuffer = (command->instanceBuffer != NULL)
            ? command->instanceBuffer->buffer
            : geometry->instanceBuffer.buffer;
        for (int i = command->pipeline->firstInstanceBinding;
             i < command->pipeline->vertexBindingCount;
             i++) {
            VC(device->vkCmdBindVertexBuffers(
                cmdBuffer, i, 1, &instanceBuffer, &command->pipeline->vertexBindingOffsets[i]));
        }

        const VkIndexType indexType = (sizeof(ovrTriangleIndex) == sizeof(unsigned int))
            ? VK_INDEX_TYPE_UINT32
            : VK_INDEX_TYPE_UINT16;
        VC(device->vkCmdBindIndexBuffer(cmdBuffer, geometry->indexBuffer.buffer, 0, indexType));
    }

    VC(device->vkCmdDrawIndexed(cmdBuffer, geometry->indexCount, command->numInstances, 0, 0, 0));

    commandBuffer->currentGraphicsState = *command;
}

ovrVkBuffer*
ovrVkCommandBuffer_MapBuffer(ovrVkCommandBuffer* commandBuffer, ovrVkBuffer* buffer, void** data) {
    assert(commandBuffer->currentRenderPass == NULL);

    ovrVkDevice* device = commandBuffer->context->device;

    ovrVkBuffer* newBuffer = NULL;
    for (ovrVkBuffer** b = &commandBuffer->oldMappedBuffers[commandBuffer->currentBuffer];
         *b != NULL;
         b = &(*b)->next) {
        if ((*b)->size == buffer->size && (*b)->type == buffer->type) {
            newBuffer = *b;
            *b = (*b)->next;
            break;
        }
    }
    if (newBuffer == NULL) {
        newBuffer = (ovrVkBuffer*)malloc(sizeof(ovrVkBuffer));
        ovrVkBuffer_Create(
            commandBuffer->context, newBuffer, buffer->type, buffer->size, NULL, true);
    }

    newBuffer->unusedCount = 0;
    newBuffer->next = commandBuffer->mappedBuffers[commandBuffer->currentBuffer];
    commandBuffer->mappedBuffers[commandBuffer->currentBuffer] = newBuffer;

    assert(newBuffer->mapped == NULL);
    VK(device->vkMapMemory(
        commandBuffer->context->device->device,
        newBuffer->memory,
        0,
        newBuffer->size,
        0,
        &newBuffer->mapped));

    *data = newBuffer->mapped;

    return newBuffer;
}

void ovrVkCommandBuffer_UnmapBuffer(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkBuffer* buffer,
    ovrVkBuffer* mappedBuffer,
    const ovrVkBufferUnmapType type) {
    // Can only copy or issue memory barrier outside a render pass.
    assert(commandBuffer->currentRenderPass == NULL);

    ovrVkDevice* device = commandBuffer->context->device;

    VC(device->vkUnmapMemory(commandBuffer->context->device->device, mappedBuffer->memory));
    mappedBuffer->mapped = NULL;

    // Optionally copy the mapped buffer back to the original buffer. While the copy is not for
    // free, there may be a performance benefit from using the original buffer if it lives in device
    // local memory.
    if (type == OVR_BUFFER_UNMAP_TYPE_COPY_BACK) {
        assert(buffer->size == mappedBuffer->size);

        {
            // Add a memory barrier for the mapped buffer from host write to DMA read.
            VkBufferMemoryBarrier bufferMemoryBarrier;
            bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferMemoryBarrier.pNext = NULL;
            bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            bufferMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferMemoryBarrier.buffer = mappedBuffer->buffer;
            bufferMemoryBarrier.offset = 0;
            bufferMemoryBarrier.size = mappedBuffer->size;

            const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_HOST_BIT;
            const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
            const VkDependencyFlags flags = 0;

            VC(device->vkCmdPipelineBarrier(
                commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
                src_stages,
                dst_stages,
                flags,
                0,
                NULL,
                1,
                &bufferMemoryBarrier,
                0,
                NULL));
        }

        {
            // Copy back to the original buffer.
            VkBufferCopy bufferCopy;
            bufferCopy.srcOffset = 0;
            bufferCopy.dstOffset = 0;
            bufferCopy.size = buffer->size;

            VC(device->vkCmdCopyBuffer(
                commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
                mappedBuffer->buffer,
                buffer->buffer,
                1,
                &bufferCopy));
        }

        {
            // Add a memory barrier for the original buffer from DMA write to the buffer access.
            VkBufferMemoryBarrier bufferMemoryBarrier;
            bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferMemoryBarrier.pNext = NULL;
            bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufferMemoryBarrier.dstAccessMask = ovrGpuBuffer_GetBufferAccess(buffer->type);
            bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferMemoryBarrier.buffer = buffer->buffer;
            bufferMemoryBarrier.offset = 0;
            bufferMemoryBarrier.size = buffer->size;

            const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
            const VkPipelineStageFlags dst_stages = PipelineStagesForBufferUsage(buffer->type);
            const VkDependencyFlags flags = 0;

            VC(device->vkCmdPipelineBarrier(
                commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
                src_stages,
                dst_stages,
                flags,
                0,
                NULL,
                1,
                &bufferMemoryBarrier,
                0,
                NULL));
        }
    } else {
        {
            // Add a memory barrier for the mapped buffer from host write to buffer access.
            VkBufferMemoryBarrier bufferMemoryBarrier;
            bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferMemoryBarrier.pNext = NULL;
            bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            bufferMemoryBarrier.dstAccessMask = ovrGpuBuffer_GetBufferAccess(mappedBuffer->type);
            bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferMemoryBarrier.buffer = mappedBuffer->buffer;
            bufferMemoryBarrier.offset = 0;
            bufferMemoryBarrier.size = mappedBuffer->size;

            const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            const VkDependencyFlags flags = 0;

            VC(device->vkCmdPipelineBarrier(
                commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
                src_stages,
                dst_stages,
                flags,
                0,
                NULL,
                1,
                &bufferMemoryBarrier,
                0,
                NULL));
        }
    }
}

ovrVkBuffer* ovrVkCommandBuffer_MapInstanceAttributes(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkGeometry* geometry,
    ovrVkVertexAttributeArrays* attribs) {
    void* data = NULL;
    ovrVkBuffer* buffer =
        ovrVkCommandBuffer_MapBuffer(commandBuffer, &geometry->instanceBuffer, &data);

    attribs->layout = geometry->layout;
    ovrVkVertexAttributeArrays_Map(
        attribs, data, buffer->size, geometry->instanceCount, geometry->instanceAttribsFlags);

    return buffer;
}

void ovrVkCommandBuffer_UnmapInstanceAttributes(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkGeometry* geometry,
    ovrVkBuffer* mappedInstanceBuffer,
    const ovrVkBufferUnmapType type) {
    ovrVkCommandBuffer_UnmapBuffer(
        commandBuffer, &geometry->instanceBuffer, mappedInstanceBuffer, type);
}
