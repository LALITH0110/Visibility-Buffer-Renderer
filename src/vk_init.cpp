#include "vk_init.h"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <set>
#include <algorithm>

// --- Helpers ---

static bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());

    for (const char* name : validationLayers) {
        bool found = false;
        for (const auto& layer : available) {
            if (strcmp(name, layer.layerName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

static std::vector<const char*> getAvailableExtensions(const std::vector<const char*>& requested) {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());

    std::vector<const char*> result;
    for (const char* req : requested) {
        for (const auto& ext : available) {
            if (strcmp(req, ext.extensionName) == 0) { result.push_back(req); break; }
        }
    }
    return result;
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present);
        if (present) indices.presentFamily = i;

        if (indices.isComplete()) break;
    }
    return indices;
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, nullptr);
    if (modeCount) {
        details.presentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, details.presentModes.data());
    }
    return details;
}

static bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : available) required.erase(ext.extensionName);
    return required.empty();
}

static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    bool extSupported = checkDeviceExtensionSupport(device);
    bool swapOk = false;
    if (extSupported) {
        auto sc = querySwapChainSupport(device, surface);
        swapOk = !sc.formats.empty() && !sc.presentModes.empty();
    }
    return indices.isComplete() && extSupported && swapOk;
}

// --- Instance ---

void createInstance(VkInstance& instance) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulken";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (!glfwVulkanSupported())
        throw std::runtime_error("GLFW: Vulkan not supported!");

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    if (!glfwExts)
        throw std::runtime_error("GLFW: Failed to get required instance extensions!");

    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);

    std::vector<const char*> optional = {
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };
    auto availOpt = getAvailableExtensions(optional);
    extensions.insert(extensions.end(), availOpt.begin(), availOpt.end());

    for (const char* ext : extensions) {
        if (strcmp(ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            break;
        }
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    bool useValidation = enableValidationLayers && checkValidationLayerSupport();
    if (useValidation) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance!");

    std::cout << "Vulkan 1.2 instance created" << std::endl;
}

// --- Surface ---

void createSurface(VkInstance instance, GLFWwindow* window, VkSurfaceKHR& surface) {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");
}

// --- Physical device ---

void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice& physicalDevice) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("no GPUs with Vulkan support!");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    for (const auto& dev : devices) {
        if (isDeviceSuitable(dev, surface)) { physicalDevice = dev; break; }
    }
    if (physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("failed to find suitable GPU!");

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "GPU: " << props.deviceName << std::endl;
}

// --- Logical device ---

void createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                         VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue) {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> uniqueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float priority = 1.0f;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device!");

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

// --- Swapchain ---

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto& m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window) {
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    VkExtent2D extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

void createSwapChain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                     GLFWwindow* window, VkSwapchainKHR& swapChain,
                     std::vector<VkImage>& images, VkFormat& format, VkExtent2D& extent) {
    auto support = querySwapChainSupport(physicalDevice, surface);
    auto surfFmt = chooseSwapSurfaceFormat(support.formats);
    auto presentMode = chooseSwapPresentMode(support.presentModes);
    extent = chooseSwapExtent(support.capabilities, window);

    uint32_t imgCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imgCount > support.capabilities.maxImageCount)
        imgCount = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = surface;
    ci.minImageCount = imgCount;
    ci.imageFormat = surfFmt.format;
    ci.imageColorSpace = surfFmt.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    auto indices = findQueueFamilies(physicalDevice, surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform = support.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &ci, nullptr, &swapChain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain!");

    vkGetSwapchainImagesKHR(device, swapChain, &imgCount, nullptr);
    images.resize(imgCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imgCount, images.data());
    format = surfFmt.format;
}

void createImageViews(VkDevice device, const std::vector<VkImage>& images, VkFormat format,
                      std::vector<VkImageView>& imageViews) {
    imageViews.resize(images.size());
    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = images[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = format;
        ci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &ci, nullptr, &imageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create image view!");
    }
}

// --- VMA ---

void createAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
                     VmaAllocator& allocator) {
    VmaAllocatorCreateInfo ci{};
    ci.instance = instance;
    ci.physicalDevice = physicalDevice;
    ci.device = device;
    ci.vulkanApiVersion = VK_API_VERSION_1_2;

    if (vmaCreateAllocator(&ci, &allocator) != VK_SUCCESS)
        throw std::runtime_error("failed to create VMA allocator!");
}
