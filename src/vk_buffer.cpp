#include "vk_buffer.h"
#include <array>
#include <cstring>
#include <stdexcept>

// --- Vertex ---

VkVertexInputBindingDescription Vertex::getBindingDescription() {
  VkVertexInputBindingDescription desc{};
  desc.binding = 0;
  desc.stride = sizeof(Vertex);
  desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return desc;
}

std::array<VkVertexInputAttributeDescription, 3>
Vertex::getAttributeDescriptions() {
  std::array<VkVertexInputAttributeDescription, 3> attrs{};
  attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
  attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
  attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};
  return attrs;
}

// --- Buffer helpers ---

AllocatedBuffer createBuffer(VmaAllocator allocator, VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VmaMemoryUsage memUsage) {
  VkBufferCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  ci.size = size;
  ci.usage = usage;
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo ai{};
  ai.usage = memUsage;

  AllocatedBuffer buf;
  if (vmaCreateBuffer(allocator, &ci, &ai, &buf.buffer, &buf.allocation,
                      nullptr) != VK_SUCCESS)
    throw std::runtime_error("failed to create buffer!");
  return buf;
}

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool) {
  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = pool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(device, &ai, &cmd);

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &bi);

  return cmd;
}

void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue,
                           VkCommandBuffer cmd) {
  vkEndCommandBuffer(cmd);

  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;

  vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(device, pool, 1, &cmd);
}

void uploadBuffer(VmaAllocator allocator, VkDevice device, VkCommandPool pool,
                  VkQueue queue, const void *data, VkDeviceSize size,
                  AllocatedBuffer &dst) {
  // Create staging buffer
  auto staging = createBuffer(allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VMA_MEMORY_USAGE_CPU_ONLY);

  void *mapped;
  vmaMapMemory(allocator, staging.allocation, &mapped);
  memcpy(mapped, data, size);
  vmaUnmapMemory(allocator, staging.allocation);

  // Copy staging -> device local
  auto cmd = beginSingleTimeCommands(device, pool);

  VkBufferCopy region{};
  region.size = size;
  vkCmdCopyBuffer(cmd, staging.buffer, dst.buffer, 1, &region);

  endSingleTimeCommands(device, pool, queue, cmd);

  vmaDestroyBuffer(allocator, staging.buffer, staging.allocation);
}

// --- Depth ---

void createDepthResources(VmaAllocator allocator, VkDevice device,
                          VkExtent2D extent, AllocatedImage &depthImage,
                          VkImageView &depthView) {
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

  VkImageCreateInfo imgCI{};
  imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgCI.imageType = VK_IMAGE_TYPE_2D;
  imgCI.format = depthFormat;
  imgCI.extent = {extent.width, extent.height, 1};
  imgCI.mipLevels = 1;
  imgCI.arrayLayers = 1;
  imgCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VmaAllocationCreateInfo allocCI{};
  allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  if (vmaCreateImage(allocator, &imgCI, &allocCI, &depthImage.image,
                     &depthImage.allocation, nullptr) != VK_SUCCESS)
    throw std::runtime_error("failed to create depth image!");

  VkImageViewCreateInfo viewCI{};
  viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCI.image = depthImage.image;
  viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewCI.format = depthFormat;
  viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  viewCI.subresourceRange.levelCount = 1;
  viewCI.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewCI, nullptr, &depthView) != VK_SUCCESS)
    throw std::runtime_error("failed to create depth image view!");
}

// --- Command pool & sync ---

void createCommandPool(VkDevice device, VkPhysicalDevice physicalDevice,
                       VkSurfaceKHR surface, VkCommandPool &commandPool) {
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

  VkCommandPoolCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  ci.queueFamilyIndex = indices.graphicsFamily.value();

  if (vkCreateCommandPool(device, &ci, nullptr, &commandPool) != VK_SUCCESS)
    throw std::runtime_error("failed to create command pool!");
}

void allocateCommandBuffers(VkDevice device, VkCommandPool pool, uint32_t count,
                            VkCommandBuffer *buffers) {
  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = pool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = count;

  if (vkAllocateCommandBuffers(device, &ai, buffers) != VK_SUCCESS)
    throw std::runtime_error("failed to allocate command buffers!");
}

void createSyncObjects(VkDevice device, VkSemaphore &imageAvailable,
                       VkSemaphore &renderFinished, VkFence &inFlightFence) {
  VkSemaphoreCreateInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fi{};
  fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  if (vkCreateSemaphore(device, &si, nullptr, &imageAvailable) != VK_SUCCESS ||
      vkCreateSemaphore(device, &si, nullptr, &renderFinished) != VK_SUCCESS ||
      vkCreateFence(device, &fi, nullptr, &inFlightFence) != VK_SUCCESS)
    throw std::runtime_error("failed to create sync objects!");
}

// --- Cleanup helpers ---

void destroyBuffer(VmaAllocator allocator, AllocatedBuffer &buffer) {
  if (buffer.buffer != VK_NULL_HANDLE) {
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    buffer.buffer = VK_NULL_HANDLE;
    buffer.allocation = VK_NULL_HANDLE;
  }
}

void destroyImage(VmaAllocator allocator, AllocatedImage &image) {
  if (image.image != VK_NULL_HANDLE) {
    vmaDestroyImage(allocator, image.image, image.allocation);
    image.image = VK_NULL_HANDLE;
    image.allocation = VK_NULL_HANDLE;
  }
}
