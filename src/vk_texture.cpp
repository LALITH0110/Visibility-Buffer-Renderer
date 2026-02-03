#include "vk_texture.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

static void transitionImageLayout(VkDevice device, VkCommandPool pool,
                                  VkQueue queue, VkImage image, VkFormat format,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout, uint32_t mipLevels) {
  VkCommandBuffer cmd = beginSingleTimeCommands(device, pool);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags srcStage, dstStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::runtime_error("unsupported layout transition!");
  }

  vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);

  endSingleTimeCommands(device, pool, queue, cmd);
}

static void generateMipmaps(VkDevice device, VkCommandPool pool, VkQueue queue,
                            VkPhysicalDevice physicalDevice, VkImage image,
                            VkFormat format, int32_t width, int32_t height,
                            uint32_t mipLevels) {
  // Check if format supports linear blitting
  VkFormatProperties props;
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
  if (!(props.optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    throw std::runtime_error(
        "texture format does not support linear blitting!");
  }

  VkCommandBuffer cmd = beginSingleTimeCommands(device, pool);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  int32_t mipWidth = width;
  int32_t mipHeight = height;

  for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                          mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    if (mipWidth > 1)
      mipWidth /= 2;
    if (mipHeight > 1)
      mipHeight /= 2;
  }

  // Transition last mip level
  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  endSingleTimeCommands(device, pool, queue, cmd);
}

Texture createTextureFromImage(VmaAllocator allocator, VkDevice device,
                               VkCommandPool pool, VkQueue queue,
                               VkPhysicalDevice physicalDevice,
                               const tinygltf::Image &gltfImage) {
  Texture tex{};

  int width = gltfImage.width;
  int height = gltfImage.height;
  VkDeviceSize imageSize = width * height * 4;

  tex.mipLevels =
      static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

  // Create staging buffer
  AllocatedBuffer staging =
      createBuffer(allocator, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_CPU_ONLY);

  void *data;
  vmaMapMemory(allocator, staging.allocation, &data);
  memcpy(data, gltfImage.image.data(), imageSize);
  vmaUnmapMemory(allocator, staging.allocation);

  // Create image
  VkImageCreateInfo imgCI{};
  imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgCI.imageType = VK_IMAGE_TYPE_2D;
  imgCI.format = VK_FORMAT_R8G8B8A8_SRGB;
  imgCI.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                  1};
  imgCI.mipLevels = tex.mipLevels;
  imgCI.arrayLayers = 1;
  imgCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgCI.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  VmaAllocationCreateInfo allocCI{};
  allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  if (vmaCreateImage(allocator, &imgCI, &allocCI, &tex.image.image,
                     &tex.image.allocation, nullptr) != VK_SUCCESS)
    throw std::runtime_error("failed to create texture image!");

  // Transition to TRANSFER_DST
  transitionImageLayout(device, pool, queue, tex.image.image,
                        VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tex.mipLevels);

  // Copy buffer to image
  VkCommandBuffer cmd = beginSingleTimeCommands(device, pool);
  VkBufferImageCopy region{};
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height), 1};
  vkCmdCopyBufferToImage(cmd, staging.buffer, tex.image.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  endSingleTimeCommands(device, pool, queue, cmd);

  // Generate mipmaps (also transitions to SHADER_READ_ONLY)
  generateMipmaps(device, pool, queue, physicalDevice, tex.image.image,
                  VK_FORMAT_R8G8B8A8_SRGB, width, height, tex.mipLevels);

  destroyBuffer(allocator, staging);

  // Create image view
  VkImageViewCreateInfo viewCI{};
  viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCI.image = tex.image.image;
  viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewCI.format = VK_FORMAT_R8G8B8A8_SRGB;
  viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewCI.subresourceRange.levelCount = tex.mipLevels;
  viewCI.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewCI, nullptr, &tex.view) != VK_SUCCESS)
    throw std::runtime_error("failed to create texture image view!");

  return tex;
}

Texture createDefaultTexture(VmaAllocator allocator, VkDevice device,
                             VkCommandPool pool, VkQueue queue) {
  Texture tex{};
  tex.mipLevels = 1;

  uint32_t white = 0xFFFFFFFF;

  AllocatedBuffer staging =
      createBuffer(allocator, sizeof(white), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_CPU_ONLY);

  void *data;
  vmaMapMemory(allocator, staging.allocation, &data);
  memcpy(data, &white, sizeof(white));
  vmaUnmapMemory(allocator, staging.allocation);

  VkImageCreateInfo imgCI{};
  imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgCI.imageType = VK_IMAGE_TYPE_2D;
  imgCI.format = VK_FORMAT_R8G8B8A8_SRGB;
  imgCI.extent = {1, 1, 1};
  imgCI.mipLevels = 1;
  imgCI.arrayLayers = 1;
  imgCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  VmaAllocationCreateInfo allocCI{};
  allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  if (vmaCreateImage(allocator, &imgCI, &allocCI, &tex.image.image,
                     &tex.image.allocation, nullptr) != VK_SUCCESS)
    throw std::runtime_error("failed to create default texture!");

  // Transition and copy
  VkCommandBuffer cmd = beginSingleTimeCommands(device, pool);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = tex.image.image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  VkBufferImageCopy region{};
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {1, 1, 1};
  vkCmdCopyBufferToImage(cmd, staging.buffer, tex.image.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  endSingleTimeCommands(device, pool, queue, cmd);
  destroyBuffer(allocator, staging);

  VkImageViewCreateInfo viewCI{};
  viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCI.image = tex.image.image;
  viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewCI.format = VK_FORMAT_R8G8B8A8_SRGB;
  viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewCI.subresourceRange.levelCount = 1;
  viewCI.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewCI, nullptr, &tex.view) != VK_SUCCESS)
    throw std::runtime_error("failed to create default texture view!");

  return tex;
}

VkSampler createTextureSampler(VkDevice device, VkPhysicalDevice physicalDevice,
                               uint32_t mipLevels) {
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(physicalDevice, &props);

  VkSamplerCreateInfo samplerCI{};
  samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCI.magFilter = VK_FILTER_LINEAR;
  samplerCI.minFilter = VK_FILTER_LINEAR;
  samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerCI.anisotropyEnable = VK_TRUE;
  samplerCI.maxAnisotropy = std::min(16.0f, props.limits.maxSamplerAnisotropy);
  samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerCI.unnormalizedCoordinates = VK_FALSE;
  samplerCI.compareEnable = VK_FALSE;
  samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCI.minLod = 0.0f;
  samplerCI.maxLod = static_cast<float>(mipLevels);
  samplerCI.mipLodBias = 0.0f;

  VkSampler sampler;
  if (vkCreateSampler(device, &samplerCI, nullptr, &sampler) != VK_SUCCESS)
    throw std::runtime_error("failed to create texture sampler!");

  return sampler;
}

void destroyTexture(VmaAllocator allocator, VkDevice device, Texture &tex) {
  if (tex.view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, tex.view, nullptr);
    tex.view = VK_NULL_HANDLE;
  }
  destroyImage(allocator, tex.image);
}
