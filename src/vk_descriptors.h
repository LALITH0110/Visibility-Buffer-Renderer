#pragma once

#include "vk_init.h"
#include <glm/glm.hpp>

// Uniform buffer for MVP matrices (set 0, binding 0)
struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
  alignas(16) glm::vec3 cameraPos;
};

// Maximum texture slots
const uint32_t MAX_TEXTURES = 32;

// Create descriptor set layout for UBO at binding 0
void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout &layout);

// Create descriptor set layout for texture array (set 2)
void createTextureDescriptorSetLayout(VkDevice device,
                                      VkDescriptorSetLayout &layout);

// Create descriptor pool for UBO and textures
void createDescriptorPool(VkDevice device, uint32_t maxSets,
                          uint32_t textureCount, VkDescriptorPool &pool);

// Allocate and write descriptor set for uniform buffer and material buffer
void createDescriptorSets(VkDevice device, VkDescriptorPool pool,
                          VkDescriptorSetLayout layout,
                          const std::vector<VkBuffer> &uniformBuffers,
                          VkDeviceSize uboSize, VkBuffer materialBuffer,
                          VkDeviceSize materialSize,
                          std::vector<VkDescriptorSet> &descriptorSets);

// Create descriptor set for textures
void createTextureDescriptorSet(VkDevice device, VkDescriptorPool pool,
                                VkDescriptorSetLayout layout, VkSampler sampler,
                                const std::vector<VkImageView> &textureViews,
                                VkDescriptorSet &descriptorSet);

// Create descriptor set layout for visibility buffer sampler (set 0, binding 0)
void createVisBufferDescriptorSetLayout(VkDevice device,
                                        VkDescriptorSetLayout &layout);

// Create descriptor set for visibility buffer sampling
void createVisBufferDescriptorSet(VkDevice device, VkDescriptorPool pool,
                                  VkDescriptorSetLayout layout,
                                  VkSampler sampler, VkImageView visBufferView,
                                  VkDescriptorSet &descriptorSet);
