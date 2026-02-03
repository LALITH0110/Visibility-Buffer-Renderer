#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk_mem_alloc.h>

#include <vector>
#include <optional>
#include <string>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    "VK_KHR_portability_subset"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// Instance & device
void createInstance(VkInstance& instance);
void createSurface(VkInstance instance, GLFWwindow* window, VkSurfaceKHR& surface);
void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice& physicalDevice);
void createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                         VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue);

// Swapchain
void createSwapChain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                     GLFWwindow* window, VkSwapchainKHR& swapChain,
                     std::vector<VkImage>& images, VkFormat& format, VkExtent2D& extent);
void createImageViews(VkDevice device, const std::vector<VkImage>& images, VkFormat format,
                      std::vector<VkImageView>& imageViews);

// VMA
void createAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
                     VmaAllocator& allocator);

// Helpers
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
