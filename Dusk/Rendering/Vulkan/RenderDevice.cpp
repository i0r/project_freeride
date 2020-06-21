/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_VULKAN

#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include <Core/Display/DisplaySurface.h>
#include <Core/Allocators/LinearAllocator.h>

#include <Maths/Helpers.h>
#include "RenderDevice.h"
#include "CommandList.h"
#include "Image.h"

#if DUSK_WINAPI
#include <Core/Display/DisplaySurfaceWin32.h>

#include <vulkan/vulkan_win32.h>
#define VK_USE_PLATFORM_WIN32_KHR 1
#define DUSK_VK_SURF_EXT VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif DUSK_XCB
#include <Core/Display/DisplaySurfaceXcb.h>

#include <vulkan/vulkan_xcb.h>
#define VK_USE_PLATFORM_XCB_KHR 1
#define DUSK_VK_SURF_EXT VK_KHR_XCB_SURFACE_EXTENSION_NAME
#else
#define DUSK_VK_SURF_EXT
#endif

#include <vulkan/vulkan.h>

#define DUSK_GET_INSTANCE_PROC_ADDR( inst, entrypoint )\
        PFN_vk##entrypoint vk##entrypoint = reinterpret_cast<PFN_vk##entrypoint>( vkGetInstanceProcAddr( inst, "vk"#entrypoint ) );\
        DUSK_ASSERT( ( vk##entrypoint != nullptr ), "vkGetInstanceProcAddr failed to find %hs!\n", "vk"#entrypoint );

VkResult CreateVkInstance( VkInstance& instance )
{
    constexpr const char* EXTENSIONS[4] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        DUSK_VK_SURF_EXT,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME
    };

    constexpr VkApplicationInfo appInfo = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        nullptr,
        "DuskEd",
        VK_MAKE_VERSION( 1, 0, 0 ),
        "Dusk_GameEngine",
        VK_MAKE_VERSION( 0, 0, 1 ),
        VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo instanceInfo;
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext = nullptr;
    instanceInfo.flags = 0;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledLayerCount = 0;
    instanceInfo.ppEnabledLayerNames = nullptr;
    instanceInfo.enabledExtensionCount = 4;
    instanceInfo.ppEnabledExtensionNames = EXTENSIONS;
    instanceInfo.pApplicationInfo = &appInfo;

#if DUSK_DEVBUILD
    constexpr const char* validationLayers[1] = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    instanceInfo.enabledLayerCount = 1u;
    instanceInfo.ppEnabledLayerNames = validationLayers;
#endif

    return vkCreateInstance( &instanceInfo, nullptr, &instance );
}

bool IsInstanceExtensionAvailable( const dkStringHash_t* extensions, const u32 extensionCount, const dkStringHash_t extensionHashcode )
{
    for ( u32 i = 0; i < extensionCount; i++ ) {
        if ( extensions[i] == extensionHashcode ) {
            return true;
        }
    }

    return false;
}

bool IsDeviceExtAvailable( RenderContext* renderContext, const dkStringHash_t extNameHashcode )
{
    if ( renderContext->deviceExtensionList.empty() ) {
        DUSK_LOG_INFO( "Device extension list is empty!\n" );

        u32 devExtCount = 0;
        vkEnumerateDeviceExtensionProperties( renderContext->physicalDevice, nullptr, &devExtCount, nullptr );

        if ( devExtCount == 0 ) {
            return false;
        }

        renderContext->deviceExtensionList.resize( devExtCount );
        vkEnumerateDeviceExtensionProperties( renderContext->physicalDevice, nullptr, &devExtCount, &renderContext->deviceExtensionList[0] );

        DUSK_LOG_INFO( "Found %i device extension(s)\n", devExtCount );
        for ( VkExtensionProperties& extension : renderContext->deviceExtensionList ) {
            DUSK_LOG_RAW( "\t-%hs (version %u)\n", extension.extensionName, extension.specVersion );
        }
    }

    for ( VkExtensionProperties& extension : renderContext->deviceExtensionList ) {
        dkStringHash_t hashcode = dk::core::CRC32( extension.extensionName );
        DUSK_LOG_RAW( "\t-%hs (version %u)\n", extension.extensionName, extension.specVersion );
        if ( hashcode == extNameHashcode ) {
            return true;
        }
    }

    return false;
}

VkExtent2D ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities )
{
    if ( capabilities.currentExtent.width != std::numeric_limits<u32>::max() ) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = { 640, 480 };
    actualExtent.width = Max( capabilities.minImageExtent.width, Min( capabilities.maxImageExtent.width, actualExtent.width ) );
    actualExtent.height = Max( capabilities.minImageExtent.height, Min( capabilities.maxImageExtent.height, actualExtent.height ) );
    return actualExtent;
}

VkPresentModeKHR ChooseSwapPresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes )
{
    VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

    for ( const VkPresentModeKHR& availablePresentMode : availablePresentModes ) {
        if ( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR ) {
            return availablePresentMode;
        } else if ( availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ) {
            bestMode = availablePresentMode;
        }
    }

    return bestMode;
}

#if DUSK_DEVBUILD
static VKAPI_ATTR VkBool32 VKAPI_CALL VkDebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData )
{
    DUSK_UNUSED_VARIABLE( messageType );
    DUSK_UNUSED_VARIABLE( pUserData );

    if ( messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
       || messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ) {
        DUSK_LOG_INFO( "%hs : %hs\n", pCallbackData->pMessageIdName, pCallbackData->pMessage );
    } else if ( messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) {
        DUSK_LOG_WARN( "%hs : %hs\n", pCallbackData->pMessageIdName, pCallbackData->pMessage );
    } else if ( messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ) {
        DUSK_LOG_ERROR( "%hs : %hs\n", pCallbackData->pMessageIdName, pCallbackData->pMessage );
    }

    return VK_FALSE;
}
#endif

RenderDevice::~RenderDevice()
{
    dk::core::free( memoryAllocator, pipelineStateCacheAllocator );
    dk::core::free( memoryAllocator, renderContext );
}

void RenderDevice::create( DisplaySurface& displaySurface, const bool useDebugContext )
{
    DUSK_UNUSED_VARIABLE( useDebugContext );

    pipelineStateCacheAllocator = dk::core::allocate<LinearAllocator>( memoryAllocator, 4 << 20, memoryAllocator->allocate( 4 << 20 ) );
    renderContext = dk::core::allocate<RenderContext>( memoryAllocator );

    VkExtensionProperties* instanceExtensionList;
    vkEnumerateInstanceExtensionProperties( nullptr, &renderContext->instanceExtensionCount, nullptr );

    instanceExtensionList = dk::core::allocateArray<VkExtensionProperties>( memoryAllocator, renderContext->instanceExtensionCount );
    vkEnumerateInstanceExtensionProperties( nullptr, &renderContext->instanceExtensionCount, instanceExtensionList );

    renderContext->instanceExtensionHashes = dk::core::allocateArray<dkStringHash_t>( memoryAllocator, renderContext->instanceExtensionCount );
    DUSK_LOG_INFO( "Found %i instance extension(s)\n", renderContext->instanceExtensionCount );
    for ( u32 i = 0; i < renderContext->instanceExtensionCount; i++ ) {
        DUSK_LOG_RAW( "\t-%hs\n", instanceExtensionList[i].extensionName );

        renderContext->instanceExtensionHashes[i] = dk::core::CRC32( instanceExtensionList[i].extensionName );
    }
    dk::core::freeArray( memoryAllocator, instanceExtensionList );

    DUSK_RAISE_FATAL_ERROR( 
        IsInstanceExtensionAvailable( renderContext->instanceExtensionHashes, renderContext->instanceExtensionCount, DUSK_STRING_HASH( VK_KHR_SURFACE_EXTENSION_NAME ) ),
        "Missing extension '%hs'!",
        VK_KHR_SURFACE_EXTENSION_NAME );

    DUSK_RAISE_FATAL_ERROR( 
        IsInstanceExtensionAvailable( renderContext->instanceExtensionHashes, renderContext->instanceExtensionCount, DUSK_STRING_HASH( DUSK_VK_SURF_EXT ) ),
        "Missing extension '%hs'!",
        DUSK_VK_SURF_EXT );

    // Create Vulkan Instance
    VkInstance vulkanInstance;
    VkResult instCreationResult = CreateVkInstance( vulkanInstance );
    DUSK_RAISE_FATAL_ERROR( ( instCreationResult == VK_SUCCESS ), "Failed to create Vulkan instance! (error code %i)", instCreationResult );

    // Enumerate and select a physical device
    DUSK_LOG_INFO( "Created Vulkan Instance! Enumerating devices...\n" );

    u32 devCount = 0;
    vkEnumeratePhysicalDevices( vulkanInstance, &devCount, nullptr );
    DUSK_RAISE_FATAL_ERROR( ( devCount != 0 ), "No device available! (devCount = %i)", devCount );

    DUSK_LOG_INFO( "Found %i device(s)\n", devCount );

    std::vector<VkPhysicalDevice> devList( devCount );
    vkEnumeratePhysicalDevices( vulkanInstance, &devCount, &devList[0] );

    VkPhysicalDeviceProperties tmpProperties = {};
    u32 deviceIdx = 0, bestDeviceIdx = 0;

    static constexpr const char* DEVICE_TYPE_TO_STRING_LUT[VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE] = {
        "Other",
        "Integrated GPU",
        "Discrete GPU",
        "Virtual GPU",
        "Software (CPU)"
    };

    u32 bestMemoryAllocationCount = 0, bestUniformBufferRange = 0, bestQueueCount = 0;
    bool bestHasCompute = false, bestHasGraphics = false;
    for ( VkPhysicalDevice& dev : devList ) {
        vkGetPhysicalDeviceProperties( dev, &tmpProperties );
        DUSK_LOG_RAW( "\t-Device[%i] = { %u, %hs, %hs }\n", deviceIdx, tmpProperties.deviceID, tmpProperties.deviceName, DEVICE_TYPE_TO_STRING_LUT[tmpProperties.deviceType] );

        // Check device queues
        u32 queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties( dev, &queueCount, nullptr );

        if ( queueCount == 0 ) {
            DUSK_LOG_ERROR( "No physical queue available! Skipping device...\n" );
            continue;
        }

        std::vector<VkQueueFamilyProperties> queueList( queueCount );
        vkGetPhysicalDeviceQueueFamilyProperties( dev, &queueCount, &queueList[0] );

        bool hasComputeQueue = false, hasGraphicsQueue = false;
        for ( VkQueueFamilyProperties& queue : queueList ) {
            if ( queue.queueCount > 0 && queue.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
                hasGraphicsQueue = true;
            } else if ( queue.queueCount > 0 && queue.queueFlags & VK_QUEUE_COMPUTE_BIT ) {
                hasComputeQueue = true;
            }
        }

        if ( tmpProperties.limits.maxMemoryAllocationCount > bestMemoryAllocationCount
             && tmpProperties.limits.maxUniformBufferRange > bestUniformBufferRange ) {
            // If best device has more render queues than the current one; skip it
            if ( ( bestHasGraphics && !hasGraphicsQueue )
              || ( bestHasCompute && !hasComputeQueue ) ) {
                continue;
            }

            bestMemoryAllocationCount = tmpProperties.limits.maxMemoryAllocationCount;
            bestUniformBufferRange = tmpProperties.limits.maxUniformBufferRange;

            bestQueueCount = queueCount;
            bestDeviceIdx = deviceIdx;
        }
        deviceIdx++;
    }

    DUSK_LOG_INFO( "Selected: Device[%i]\n", bestDeviceIdx );

    const VkPhysicalDevice physicalDevice = devList[bestDeviceIdx];
    renderContext->physicalDevice = physicalDevice;

    // Create display surface
    DUSK_RAISE_FATAL_ERROR( IsDeviceExtAvailable( renderContext, DUSK_STRING_HASH( VK_KHR_SWAPCHAIN_EXTENSION_NAME ) ), "Missing extension: %hs", VK_KHR_SWAPCHAIN_EXTENSION_NAME );
    VkSurfaceKHR displaySurf = nullptr;

#if DUSK_WINAPI
    const NativeDisplaySurface* nativeDispSurf = displaySurface.getNativeDisplaySurface();

    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.hinstance = nativeDispSurf->Instance;
    createInfo.hwnd = nativeDispSurf->Handle;

    DUSK_GET_INSTANCE_PROC_ADDR( vulkanInstance, CreateWin32SurfaceKHR );
    VkResult surfCreateResult = vkCreateWin32SurfaceKHR( vulkanInstance, &createInfo, nullptr, &displaySurf );
#elif DUSK_XCB
        const NativeDisplaySurface* nativeDispSurf = displaySurface.getNativeDisplaySurface();

        VkXcbSurfaceCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.window = nativeDispSurf->WindowInstance;
        createInfo.connection = nativeDispSurf->Connection;

        DUSK_GET_INSTANCE_PROC_ADDR( vulkanInstance, CreateXcbSurfaceKHR )
        VkResult surfCreateResult = vkCreateXcbSurfaceKHR( vulkanInstance, &createInfo, nullptr, &displaySurf );
#else
#error "Missing API Implementation!"
#endif

    DUSK_RAISE_FATAL_ERROR( ( surfCreateResult == VK_SUCCESS ), "Failed to create display surface! (error code: %i)", surfCreateResult );

    DUSK_GET_INSTANCE_PROC_ADDR( vulkanInstance, GetPhysicalDeviceSurfaceSupportKHR )

#if DUSK_DEVBUILD
    DUSK_GET_INSTANCE_PROC_ADDR( vulkanInstance, DebugMarkerSetObjectNameEXT )
    renderContext->debugObjectMarker = vkDebugMarkerSetObjectNameEXT;

    DUSK_GET_INSTANCE_PROC_ADDR( vulkanInstance, CreateDebugUtilsMessengerEXT )

    VkDebugUtilsMessengerCreateInfoEXT dbgMessengerCreateInfos = {};
    dbgMessengerCreateInfos.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dbgMessengerCreateInfos.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbgMessengerCreateInfos.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbgMessengerCreateInfos.pfnUserCallback = VkDebugCallback;
    VkResult dbgMessengerCreateResult = vkCreateDebugUtilsMessengerEXT( vulkanInstance, &dbgMessengerCreateInfos, nullptr, &renderContext->debugCallback );

    DUSK_ASSERT( ( dbgMessengerCreateResult == VK_SUCCESS ), "Failed to create debug callback! (error code: %i)", dbgMessengerCreateResult )
#endif

    // TODO A vector of boolean is garbage! :(
    std::vector<VkBool32> presentSupport( bestQueueCount );
    for ( u32 i = 0; i < bestQueueCount; i++ ) {
        vkGetPhysicalDeviceSurfaceSupportKHR( physicalDevice, i, displaySurf, &presentSupport[i] );
    }

    std::vector<VkQueueFamilyProperties> queueList( bestQueueCount );
    vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, &bestQueueCount, &queueList[0] );

    u32 computeQueueFamilyIndex = UINT32_MAX;
    u32 graphicsQueueFamilyIndex = UINT32_MAX;
    u32 presentQueueFamilyIndex = UINT32_MAX;
    for ( u32 i = 0; i < queueList.size(); i++ ) {
        if ( queueList[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
            if ( graphicsQueueFamilyIndex == UINT32_MAX ) {
                graphicsQueueFamilyIndex = i;
            }

            if ( presentSupport[i] == VK_TRUE ) {
                graphicsQueueFamilyIndex = i;
                presentQueueFamilyIndex = i;
            }
        }
        else if ( queueList[i].queueFlags & VK_QUEUE_COMPUTE_BIT ) {
            if ( computeQueueFamilyIndex == UINT32_MAX ) {
                computeQueueFamilyIndex = i;
            }
        }
    }

    if ( presentQueueFamilyIndex == UINT32_MAX ) {
        // If didn't find a queue that supports both graphics and present, then
        // find a separate present queue.
        for ( u32 i = 0; i < queueList.size(); i++ ) {
            if ( presentSupport[i] == VK_TRUE ) {
                presentQueueFamilyIndex = i;
                break;
            }
        }
    }

    DUSK_RAISE_FATAL_ERROR( ( graphicsQueueFamilyIndex != UINT32_MAX && presentQueueFamilyIndex != UINT32_MAX ),
                            "Could not find both graphics queue, present queue or graphics/present queue (graphicsQueueFamilyIndex %i, presentQueueFamilyIndex %i)",
                            graphicsQueueFamilyIndex, presentQueueFamilyIndex );

    // Create queues
    constexpr f32 queuePrios = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queues;

    const VkDeviceQueueCreateInfo graphicsQueueInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        0,
        graphicsQueueFamilyIndex,
        1,
        &queuePrios,
    };

    queues.push_back( graphicsQueueInfo );

    if ( computeQueueFamilyIndex != UINT32_MAX ) {
        const VkDeviceQueueCreateInfo computeQueueInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            nullptr,
            0,
            computeQueueFamilyIndex,
            1,
            &queuePrios,
        };

        queues.push_back( computeQueueInfo );
    }

    constexpr const char* const DEVICE_EXTENSIONS[3] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
    };

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.imageCubeArray = VK_TRUE;
    deviceFeatures.shaderStorageImageExtendedFormats = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo deviceCreationInfos;
    deviceCreationInfos.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreationInfos.pNext = nullptr;
    deviceCreationInfos.flags = 0;
    deviceCreationInfos.queueCreateInfoCount = static_cast< u32 >( queues.size() );
    deviceCreationInfos.pQueueCreateInfos = queues.data();
    deviceCreationInfos.enabledLayerCount = 0u;
    deviceCreationInfos.ppEnabledLayerNames = nullptr;
    deviceCreationInfos.ppEnabledExtensionNames = DEVICE_EXTENSIONS;
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    deviceCreationInfos.enabledExtensionCount = 3;
#else
    deviceCreationInfos.enabledExtensionCount = 2;
#endif
    deviceCreationInfos.pEnabledFeatures = &deviceFeatures;

    VkDevice device;
    VkResult deviceCreationResult = vkCreateDevice( physicalDevice, &deviceCreationInfos, nullptr, &device );
    DUSK_RAISE_FATAL_ERROR( ( deviceCreationResult == VK_SUCCESS ), "Device creation failed! (error code: %i)", deviceCreationResult );

    vkGetDeviceQueue( device, graphicsQueueFamilyIndex, 0, &renderContext->graphicsQueue );
    vkGetDeviceQueue( device, presentQueueFamilyIndex, 0, &renderContext->presentQueue );
    vkGetDeviceQueue( device, ( computeQueueFamilyIndex == UINT32_MAX ) ? graphicsQueueFamilyIndex : computeQueueFamilyIndex, 0, & renderContext->computeQueue );

    u32 surfFormatsCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, displaySurf, &surfFormatsCount, VK_NULL_HANDLE );
    DUSK_LOG_INFO( "Found %i surface format(s)\n", surfFormatsCount );

    std::vector<VkSurfaceFormatKHR> formatList( surfFormatsCount );
    vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, displaySurf, &surfFormatsCount, formatList.data() );

    // Select surface format
    // TODO Let the user pick the format? (if the user has a fancy display)
    VkSurfaceFormatKHR selectedFormat;
    for ( VkSurfaceFormatKHR& format : formatList ) {
        if ( format.format != VK_FORMAT_UNDEFINED ) {
            // TODO Create a lookup table to have stringified format and have something more explicit in logging
            DUSK_LOG_INFO( "Selected Format: %i (colorspace: %i)\n", format.format, format.colorSpace );
            selectedFormat = format;
            break;
        }
    }

    VkSurfaceCapabilitiesKHR surfCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, displaySurf, &surfCapabilities );

    // Create swapchain
    VkSwapchainCreateInfoKHR swapChainCreation = {};
    swapChainCreation.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreation.pNext = nullptr;
    swapChainCreation.surface = displaySurf;

    u32 imageCount = surfCapabilities.minImageCount + 1;
    if ( surfCapabilities.maxImageCount > 0 && imageCount > surfCapabilities.maxImageCount ) {
        imageCount = surfCapabilities.maxImageCount;
    }

    VkExtent2D extent = ChooseSwapExtent( surfCapabilities );
    swapChainCreation.minImageCount = imageCount;
    swapChainCreation.imageFormat = selectedFormat.format;
    swapChainCreation.imageColorSpace = selectedFormat.colorSpace;
    swapChainCreation.imageExtent = extent;
    swapChainCreation.imageArrayLayers = 1;
    swapChainCreation.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    vkGetDeviceQueue( device, graphicsQueueFamilyIndex, 0, &renderContext->graphicsQueue );
    vkGetDeviceQueue( device, presentQueueFamilyIndex, 0, &renderContext->presentQueue );

    u32 queueIndices[2] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
    if ( graphicsQueueFamilyIndex != presentQueueFamilyIndex ) {
        swapChainCreation.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreation.queueFamilyIndexCount = 2;
        swapChainCreation.pQueueFamilyIndices = queueIndices;
    } else {
        swapChainCreation.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreation.queueFamilyIndexCount = 0;
        swapChainCreation.pQueueFamilyIndices = nullptr;
    }
    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, displaySurf, &presentModeCount, nullptr );

    std::vector<VkPresentModeKHR> presentModes;
    if ( presentModeCount != 0 ) {
        presentModes.resize( presentModeCount );
        vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, displaySurf, &presentModeCount, presentModes.data() );
    }

    swapChainCreation.preTransform = surfCapabilities.currentTransform;
    swapChainCreation.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreation.presentMode = ChooseSwapPresentMode( presentModes );
    swapChainCreation.clipped = VK_TRUE;

    swapChainCreation.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain = {};
    VkResult swapChainCreationResult = vkCreateSwapchainKHR( device, &swapChainCreation, nullptr, &swapChain );
    DUSK_RAISE_FATAL_ERROR( ( swapChainCreationResult == VK_SUCCESS ), "Failed to create swapchain! (error code: %i)", swapChainCreationResult );

    vkGetSwapchainImagesKHR( device, swapChain, &imageCount, nullptr );

    std::vector<VkImage> swapChainImages( imageCount );
    vkGetSwapchainImagesKHR( device, swapChain, &imageCount, swapChainImages.data() );

    // We create the data structure but don't allocate anything (since we'll retrieve the backbuffer memory from the swapchain)
    swapChainImage = dk::core::allocate<Image>( memoryAllocator );

    i32 internalBufferIdx = 0;
    for ( VkImage& vkSwapChainImage : swapChainImages ) {
        swapChainImage->resource[internalBufferIdx] = vkSwapChainImage;

        // Create default render target view
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = vkSwapChainImage;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = selectedFormat.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        vkCreateImageView( device, &createInfo, nullptr, &swapChainImage->renderTargetView[VIEW_FORMAT_B8G8R8A8_UNORM][internalBufferIdx] );

        swapChainImage->currentStage[internalBufferIdx] = VK_PIPELINE_STAGE_HOST_BIT;

        internalBufferIdx++;
    }

    // Set RenderContext pointers
    renderContext->instance = vulkanInstance;
    renderContext->device = device;
    renderContext->displaySurface = displaySurf;
    renderContext->swapChain = swapChain;
    renderContext->swapChainExtent = extent;
    renderContext->swapChainFormat = selectedFormat.format;

    renderContext->computeQueueIndex = computeQueueFamilyIndex;
    renderContext->graphicsQueueIndex = graphicsQueueFamilyIndex;
    renderContext->presentQueueIndex = presentQueueFamilyIndex;

    DUSK_LOG_INFO( "Allocate CommandPools...\n" );

    // Allocate cmdList Pools
    VkCommandPoolCreateInfo gfxCmdPoolDesc;
    gfxCmdPoolDesc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    gfxCmdPoolDesc.pNext = nullptr;
    gfxCmdPoolDesc.queueFamilyIndex = renderContext->graphicsQueueIndex;
    gfxCmdPoolDesc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        vkCreateCommandPool( renderContext->device, &gfxCmdPoolDesc, nullptr, &renderContext->graphicsCommandPool[i] );
    }

    if ( computeQueueFamilyIndex != -1 && computeQueueFamilyIndex != graphicsQueueFamilyIndex ) {
        VkCommandPoolCreateInfo computeCmdPoolDesc;
        computeCmdPoolDesc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        computeCmdPoolDesc.pNext = nullptr;
        computeCmdPoolDesc.queueFamilyIndex = renderContext->computeQueueIndex;
        computeCmdPoolDesc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
            vkCreateCommandPool( renderContext->device, &computeCmdPoolDesc, nullptr, &renderContext->computeCommandPool[i] );
        }
    } else {
        for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
            renderContext->computeCommandPool[i] = renderContext->graphicsCommandPool[i];
        }
    }

    // Create command list allocators (per command queue)
    constexpr size_t CMD_LIST_ALLOCATION_SIZE = sizeof( CommandList ) * CMD_LIST_POOL_CAPACITY;

#if DUSK_ENABLE_GPU_DEBUG_MARKER
    // Retrieve Debug Marker extension function pointers
    DUSK_GET_INSTANCE_PROC_ADDR( renderContext->instance, CmdDebugMarkerBeginEXT )
    DUSK_GET_INSTANCE_PROC_ADDR( renderContext->instance, CmdDebugMarkerEndEXT )
#endif

    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    fenceInfo.pNext = VK_NULL_HANDLE;
    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        vkCreateFence( renderContext->device, &fenceInfo, nullptr, &renderContext->swapChainFence[i] );
    }

    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        VkCommandBuffer gfxCmdBuffers[CMD_LIST_POOL_CAPACITY];

        VkCommandBufferAllocateInfo cmdBufferAlloc;
        cmdBufferAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferAlloc.pNext = nullptr;
        cmdBufferAlloc.commandPool = renderContext->graphicsCommandPool[i];
        cmdBufferAlloc.commandBufferCount = CMD_LIST_POOL_CAPACITY;
        cmdBufferAlloc.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        vkAllocateCommandBuffers( renderContext->device, &cmdBufferAlloc, gfxCmdBuffers );

        graphicsCmdListAllocator[i] = dk::core::allocate<LinearAllocator>(
            memoryAllocator,
            CMD_LIST_ALLOCATION_SIZE,
            memoryAllocator->allocate( CMD_LIST_ALLOCATION_SIZE )
        );

        for ( i32 j = 0; j < CMD_LIST_POOL_CAPACITY; j++ ) {
            CommandList* cmdList = dk::core::allocate<CommandList>( graphicsCmdListAllocator[i], CommandList::GRAPHICS );
            cmdList->initialize( memoryAllocator, j );

            NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();
            nativeCmdList->cmdList = gfxCmdBuffers[j];

            nativeCmdList->computeQueueIndex = renderContext->computeQueueIndex;
            nativeCmdList->graphicsQueueIndex = renderContext->graphicsQueueIndex;
            nativeCmdList->presentQueueIndex = renderContext->presentQueueIndex;

            nativeCmdList->device = renderContext->device;
            nativeCmdList->cmdQueueIdx = renderContext->graphicsQueueIndex;

#if DUSK_ENABLE_GPU_DEBUG_MARKER
            nativeCmdList->vkCmdDebugMarkerBegin = vkCmdDebugMarkerBeginEXT;
            nativeCmdList->vkCmdDebugMarkerEnd = vkCmdDebugMarkerEndEXT;
#endif
        }

        graphicsCmdListAllocator[i]->clear();
    }

    // Compute Command Lists
    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        VkCommandBuffer compCmdBuffers[CMD_LIST_POOL_CAPACITY];
        VkCommandBufferAllocateInfo cmpCmdBufferAlloc;
        cmpCmdBufferAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmpCmdBufferAlloc.pNext = nullptr;
        cmpCmdBufferAlloc.commandPool = renderContext->computeCommandPool[i];
        cmpCmdBufferAlloc.commandBufferCount = CMD_LIST_POOL_CAPACITY;
        cmpCmdBufferAlloc.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        vkAllocateCommandBuffers( renderContext->device, &cmpCmdBufferAlloc, compCmdBuffers );

        computeCmdListAllocator[i] = dk::core::allocate<LinearAllocator>(
            memoryAllocator,
            CMD_LIST_ALLOCATION_SIZE,
            memoryAllocator->allocate( CMD_LIST_ALLOCATION_SIZE )
        );

        for ( i32 j = 0; j < CMD_LIST_POOL_CAPACITY; j++ ) {
            CommandList* cmdList = dk::core::allocate<CommandList>( computeCmdListAllocator[i], CommandList::COMPUTE );
            cmdList->initialize( memoryAllocator, j );

            NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();
            nativeCmdList->cmdList = compCmdBuffers[j];

            nativeCmdList->computeQueueIndex = renderContext->computeQueueIndex;
            nativeCmdList->graphicsQueueIndex = renderContext->graphicsQueueIndex;
            nativeCmdList->presentQueueIndex = renderContext->presentQueueIndex;

            nativeCmdList->device = renderContext->device;
            nativeCmdList->cmdQueueIdx = renderContext->computeQueueIndex;

#if DUSK_ENABLE_GPU_DEBUG_MARKER
            nativeCmdList->vkCmdDebugMarkerBegin = vkCmdDebugMarkerBeginEXT;
            nativeCmdList->vkCmdDebugMarkerEnd = vkCmdDebugMarkerEndEXT;
#endif
        }

        computeCmdListAllocator[i]->clear();
    }

    VkDescriptorPoolCreateInfo descriptorPoolDesc;
    descriptorPoolDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolDesc.pNext = nullptr;
    descriptorPoolDesc.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolDesc.maxSets = 64u * 3;

    static constexpr uint32_t DESCRIPTOR_POOL_COUNT = 8u;
    VkDescriptorPoolSize descriptorPoolSizes[DESCRIPTOR_POOL_COUNT] = {
       { VK_DESCRIPTOR_TYPE_SAMPLER, 64u * 3 },
       { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64u * 3 },
       { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64u * 3 },
       { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 64u * 3 },
       { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 64u * 3 },
       { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 64u * 3 },
       { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 64u * 3 },
       { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64u * 3 },
    };

    descriptorPoolDesc.pPoolSizes = descriptorPoolSizes;
    descriptorPoolDesc.poolSizeCount = DESCRIPTOR_POOL_COUNT;

    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        vkCreateDescriptorPool( renderContext->device, &descriptorPoolDesc, nullptr, &renderContext->descriptorPool[i] );
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0u;

    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        for ( i32 j = 0; j < 32; j++ ) {
            vkCreateSemaphore( renderContext->device, &semaphoreCreateInfo, nullptr, &renderContext->frameSemaphores[i][j] );
        }

        renderContext->frameSemaphoresCount[i] = 1;
    }

    u32 nextImageIdx = 0;
    VkResult opResult = vkAcquireNextImageKHR( renderContext->device, renderContext->swapChain, std::numeric_limits<uint64_t>::max(), renderContext->frameSemaphores[0][0], VK_NULL_HANDLE, &nextImageIdx );
    DUSK_RAISE_FATAL_ERROR( opResult == VkResult::VK_SUCCESS, "Failed to acquire next image! (error code: 0x%x)", opResult );
}

void RenderDevice::enableVerticalSynchronisation( const bool enabled )
{
    renderContext->presentMode = ( enabled ) ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
}

CommandList& RenderDevice::allocateGraphicsCommandList()
{
    const size_t bufferIdx = frameIndex % PENDING_FRAME_COUNT;

    // Reset the allocator (go back to the start of the allocator memory space)
    if ( graphicsCmdListAllocator[bufferIdx]->getAllocationCount() == CMD_LIST_POOL_CAPACITY ) {
        graphicsCmdListAllocator[bufferIdx]->clear();
    }

    CommandList* cmdList = static_cast< CommandList* >( graphicsCmdListAllocator[bufferIdx]->allocate( sizeof( CommandList ), alignof( CommandList ) ) );
    NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();

    nativeCmdList->descriptorPool = renderContext->descriptorPool[bufferIdx];

    cmdList->setResourceFrameIndex( static_cast< i32 >( frameIndex ) );

    return *cmdList;
}

CommandList& RenderDevice::allocateComputeCommandList()
{
    const size_t bufferIdx = frameIndex % PENDING_FRAME_COUNT;

    // Reset the allocator (go back to the start of the allocator memory space)
    if ( computeCmdListAllocator[bufferIdx]->getAllocationCount() == CMD_LIST_POOL_CAPACITY ) {
        computeCmdListAllocator[bufferIdx]->clear();
    }

    CommandList* cmdList = static_cast< CommandList* >( computeCmdListAllocator[bufferIdx]->allocate( sizeof( CommandList ), alignof( CommandList ) ) );
    NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();

    nativeCmdList->descriptorPool = renderContext->descriptorPool[bufferIdx];

    cmdList->setResourceFrameIndex( static_cast< i32 >( frameIndex ) );

    return *cmdList;
}

void RenderDevice::submitCommandList( CommandList& cmdList )
{
    size_t bufferIdx = frameIndex % PENDING_FRAME_COUNT;
    NativeCommandList* nativeCmdList = cmdList.getNativeCommandList();

    vkEndCommandBuffer( nativeCmdList->cmdList );

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &nativeCmdList->cmdList;
    submitInfo.pSignalSemaphores = &renderContext->frameSemaphores[bufferIdx][renderContext->frameSemaphoresCount[bufferIdx]++];
    submitInfo.signalSemaphoreCount = 1;

    VkFence waitFence = VK_NULL_HANDLE;
    static constexpr const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if ( nativeCmdList->waitForSwapchainRetrival ) {
        submitInfo.pWaitSemaphores = &renderContext->frameSemaphores[bufferIdx][0];
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitDstStageMask = &waitStageMask;

        size_t nextFrameIdx = frameIndex + 1;
        waitFence = renderContext->swapChainFence[nextFrameIdx % PENDING_FRAME_COUNT];
    }

    VkQueue submitQueue = ( cmdList.getCommandListType() == CommandList::GRAPHICS ) ? renderContext->graphicsQueue : renderContext->computeQueue;
    vkQueueSubmit( submitQueue, 1u, &submitInfo, waitFence );
}

void RenderDevice::present()
{
    u32 bufferIdx = frameIndex % PENDING_FRAME_COUNT;

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = renderContext->frameSemaphoresCount[bufferIdx] - 1;
    presentInfo.pWaitSemaphores = &renderContext->frameSemaphores[bufferIdx][1];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &renderContext->swapChain;
    presentInfo.pImageIndices = &bufferIdx;
    presentInfo.pResults = nullptr;

    VkResult operationResult = vkQueuePresentKHR( renderContext->presentQueue, &presentInfo );
    DUSK_ASSERT( operationResult == VK_SUCCESS, "Failed to swap buffers! (error code 0x%x)", operationResult )

    // Prepare device state/resources for the next frame
    frameIndex++;

    bufferIdx = frameIndex % PENDING_FRAME_COUNT;
    vkResetDescriptorPool( renderContext->device, renderContext->descriptorPool[bufferIdx], 0 );
    renderContext->frameSemaphoresCount[bufferIdx] = 1;

    VkFence waitFence = renderContext->swapChainFence[bufferIdx];
    vkWaitForFences( renderContext->device, 1, &waitFence, true, UINT64_MAX );
    vkResetFences( renderContext->device, 1, &waitFence );

    u32 nextImageIdx = 0;
    VkResult opResult = vkAcquireNextImageKHR( renderContext->device, renderContext->swapChain, UINT64_MAX, renderContext->frameSemaphores[bufferIdx][0], VK_NULL_HANDLE, &nextImageIdx );
    DUSK_RAISE_FATAL_ERROR( opResult == VkResult::VK_SUCCESS, "Failed to acquire next image! (error code: 0x%x)", opResult );
}

void RenderDevice::waitForPendingFrameCompletion()
{
    // Wait on the host for the completion of all queue ops.
    vkDeviceWaitIdle( renderContext->device );
}

const dkChar_t* RenderDevice::getBackendName()
{
    return DUSK_STRING( "Vulkan" );
}
#endif
