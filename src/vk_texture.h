#pragma once

#include "vk_buffer.h"
#include <tiny_gltf.h>
#include <vector>

struct Texture {
  AllocatedImage image;
  VkImageView view = VK_NULL_HANDLE;
  uint32_t mipLevels = 1;
};

// Create a texture from glTF image data
Texture createTextureFromImage(VmaAllocator allocator, VkDevice device,
                               VkCommandPool pool, VkQueue queue,
                               VkPhysicalDevice physicalDevice,
                               const tinygltf::Image &gltfImage);

// Create a 1x1 white texture as default
Texture createDefaultTexture(VmaAllocator allocator, VkDevice device,
                             VkCommandPool pool, VkQueue queue);

// Create sampler with mipmapping and anisotropic filtering
VkSampler createTextureSampler(VkDevice device, VkPhysicalDevice physicalDevice,
                               uint32_t mipLevels);

// Cleanup
void destroyTexture(VmaAllocator allocator, VkDevice device, Texture &tex);
