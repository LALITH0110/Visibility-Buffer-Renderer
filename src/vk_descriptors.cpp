#include "vk_descriptors.h"
#include <array>
#include <stdexcept>

void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout &layout) {
  std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

  // Binding 0: UBO
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[0].pImmutableSamplers = nullptr;

  // Binding 1: Material SSBO
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[1].pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) !=
      VK_SUCCESS)
    throw std::runtime_error("failed to create descriptor set layout!");
}

void createTextureDescriptorSetLayout(VkDevice device,
                                      VkDescriptorSetLayout &layout) {
  VkDescriptorSetLayoutBinding samplerBinding{};
  samplerBinding.binding = 0;
  samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerBinding.descriptorCount = MAX_TEXTURES;
  samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  samplerBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &samplerBinding;

  if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) !=
      VK_SUCCESS)
    throw std::runtime_error("failed to create texture descriptor set layout!");
}

void createDescriptorPool(VkDevice device, uint32_t maxSets,
                          uint32_t textureCount, VkDescriptorPool &pool) {
  std::array<VkDescriptorPoolSize, 3> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = maxSets;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = textureCount > 0 ? textureCount : 1;
  poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSizes[2].descriptorCount = maxSets; // One material SSBO per frame

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = maxSets + 1; // +1 for texture set

  if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
    throw std::runtime_error("failed to create descriptor pool!");
}

void createDescriptorSets(VkDevice device, VkDescriptorPool pool,
                          VkDescriptorSetLayout layout,
                          const std::vector<VkBuffer> &uniformBuffers,
                          VkDeviceSize uboSize, VkBuffer materialBuffer,
                          VkDeviceSize materialSize,
                          std::vector<VkDescriptorSet> &descriptorSets) {
  std::vector<VkDescriptorSetLayout> layouts(uniformBuffers.size(), layout);

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(uniformBuffers.size());
  allocInfo.pSetLayouts = layouts.data();

  descriptorSets.resize(uniformBuffers.size());
  if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) !=
      VK_SUCCESS)
    throw std::runtime_error("failed to allocate descriptor sets!");

  for (size_t i = 0; i < uniformBuffers.size(); i++) {
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = uniformBuffers[i];
    uboInfo.offset = 0;
    uboInfo.range = uboSize;

    VkDescriptorBufferInfo matInfo{};
    matInfo.buffer = materialBuffer;
    matInfo.offset = 0;
    matInfo.range = materialSize;

    std::array<VkWriteDescriptorSet, 2> writes{};

    // UBO at binding 0
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSets[i];
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &uboInfo;

    // Material SSBO at binding 1
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSets[i];
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &matInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }
}

void createTextureDescriptorSet(VkDevice device, VkDescriptorPool pool,
                                VkDescriptorSetLayout layout, VkSampler sampler,
                                const std::vector<VkImageView> &textureViews,
                                VkDescriptorSet &descriptorSet) {
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) !=
      VK_SUCCESS)
    throw std::runtime_error("failed to allocate texture descriptor set!");

  std::vector<VkDescriptorImageInfo> imageInfos(MAX_TEXTURES);
  for (uint32_t i = 0; i < MAX_TEXTURES; i++) {
    imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[i].sampler = sampler;
    // Use provided texture or fallback to first one (default)
    imageInfos[i].imageView =
        (i < textureViews.size()) ? textureViews[i] : textureViews[0];
  }

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = descriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = MAX_TEXTURES;
  descriptorWrite.pImageInfo = imageInfos.data();

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}

void createVisBufferDescriptorSetLayout(VkDevice device,
                                        VkDescriptorSetLayout &layout) {
  VkDescriptorSetLayoutBinding samplerBinding{};
  samplerBinding.binding = 0;
  samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerBinding.descriptorCount = 1;
  samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  samplerBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &samplerBinding;

  if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) !=
      VK_SUCCESS)
    throw std::runtime_error(
        "failed to create vis buffer descriptor set layout!");
}

void createVisBufferDescriptorSet(VkDevice device, VkDescriptorPool pool,
                                  VkDescriptorSetLayout layout,
                                  VkSampler sampler, VkImageView visBufferView,
                                  VkDescriptorSet &descriptorSet) {
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) !=
      VK_SUCCESS)
    throw std::runtime_error("failed to allocate vis buffer descriptor set!");

  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.sampler = sampler;
  imageInfo.imageView = visBufferView;

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = descriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}
