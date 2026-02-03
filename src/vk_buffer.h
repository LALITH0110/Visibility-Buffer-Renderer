#pragma once

#include "vk_init.h"
#include <glm/glm.hpp>

struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 uv;

  static VkVertexInputBindingDescription getBindingDescription();
  static std::array<VkVertexInputAttributeDescription, 3>
  getAttributeDescriptions();
};

struct AllocatedBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
};

struct AllocatedImage {
  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
};

// Buffer creation
AllocatedBuffer createBuffer(VmaAllocator allocator, VkDeviceSize size,
                             VkBufferUsageFlags usage, VmaMemoryUsage memUsage);

void uploadBuffer(VmaAllocator allocator, VkDevice device, VkCommandPool pool,
                  VkQueue queue, const void *data, VkDeviceSize size,
                  AllocatedBuffer &dst);

// Depth buffer
void createDepthResources(VmaAllocator allocator, VkDevice device,
                          VkExtent2D extent, AllocatedImage &depthImage,
                          VkImageView &depthView);

// Commands
void createCommandPool(VkDevice device, VkPhysicalDevice physicalDevice,
                       VkSurfaceKHR surface, VkCommandPool &commandPool);

void allocateCommandBuffers(VkDevice device, VkCommandPool pool, uint32_t count,
                            VkCommandBuffer *buffers);

void createSyncObjects(VkDevice device, VkSemaphore &imageAvailable,
                       VkSemaphore &renderFinished, VkFence &inFlightFence);

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);
void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue,
                           VkCommandBuffer cmd);

// Cleanup helpers
void destroyBuffer(VmaAllocator allocator, AllocatedBuffer &buffer);
void destroyImage(VmaAllocator allocator, AllocatedImage &image);
