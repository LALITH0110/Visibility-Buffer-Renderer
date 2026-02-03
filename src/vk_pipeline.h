#pragma once

#include "vk_buffer.h"
#include "vk_init.h"

#include <string>
#include <vector>

// Push constants for per-draw data
struct PushConstants {
  uint32_t drawID;        // For visibility pass
  uint32_t materialIndex; // For forward/shade pass
};

VkShaderModule createShaderModule(VkDevice device,
                                  const std::vector<char> &code);
std::vector<char> readFile(const std::string &filename);

//--------------------------------------------------------------
// Forward pass (color + depth)
//--------------------------------------------------------------
void createRenderPass(VkDevice device, VkFormat swapChainFormat,
                      VkFormat depthFormat, VkRenderPass &renderPass);

void createGraphicsPipeline(
    VkDevice device, VkExtent2D extent, VkRenderPass renderPass,
    const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts,
    VkPipelineLayout &pipelineLayout, VkPipeline &pipeline);

void createFramebuffers(VkDevice device, VkRenderPass renderPass,
                        VkExtent2D extent,
                        const std::vector<VkImageView> &colorViews,
                        VkImageView depthView,
                        std::vector<VkFramebuffer> &framebuffers);

//--------------------------------------------------------------
// Visibility buffer pass
//--------------------------------------------------------------
void createVisibilityRenderPass(VkDevice device, VkFormat depthFormat,
                                VkRenderPass &renderPass);

void createVisibilityPipeline(VkDevice device, VkExtent2D extent,
                              VkRenderPass renderPass,
                              VkDescriptorSetLayout uboLayout,
                              VkPipelineLayout &pipelineLayout,
                              VkPipeline &pipeline);

void createVisibilityFramebuffer(VkDevice device, VkRenderPass renderPass,
                                 VkExtent2D extent, VkImageView visView,
                                 VkImageView depthView,
                                 VkFramebuffer &framebuffer);

//--------------------------------------------------------------
// Debug visualization pass
//--------------------------------------------------------------
void createDebugVisRenderPass(VkDevice device, VkFormat swapChainFormat,
                              VkRenderPass &renderPass);

void createDebugVisPipeline(VkDevice device, VkExtent2D extent,
                            VkRenderPass renderPass,
                            VkDescriptorSetLayout visBufferLayout,
                            VkPipelineLayout &pipelineLayout,
                            VkPipeline &pipeline);

//--------------------------------------------------------------
// Compute shade pass
//--------------------------------------------------------------
void createComputeShadeDescriptorSetLayout(VkDevice device,
                                           VkDescriptorSetLayout &layout);

void createComputeShadePipeline(
    VkDevice device, const std::vector<VkDescriptorSetLayout> &layouts,
    VkPipelineLayout &pipelineLayout, VkPipeline &pipeline);
