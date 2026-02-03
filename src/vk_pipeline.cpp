#include "vk_pipeline.h"

#include <array>
#include <fstream>
#include <stdexcept>

std::vector<char> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open())
    throw std::runtime_error("failed to open file: " + filename);

  size_t size = (size_t)file.tellg();
  std::vector<char> buffer(size);
  file.seekg(0);
  file.read(buffer.data(), size);
  return buffer;
}

VkShaderModule createShaderModule(VkDevice device,
                                  const std::vector<char> &code) {
  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = code.size();
  ci.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule module;
  if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS)
    throw std::runtime_error("failed to create shader module!");
  return module;
}

void createRenderPass(VkDevice device, VkFormat swapChainFormat,
                      VkFormat depthFormat, VkRenderPass &renderPass) {
  // Color attachment
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapChainFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorRef{};
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // Depth attachment
  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = depthFormat;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef{};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.srcAccessMask = 0;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkAttachmentDescription, 2> attachments = {colorAttachment,
                                                        depthAttachment};

  VkRenderPassCreateInfo rpInfo{};
  rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  rpInfo.pAttachments = attachments.data();
  rpInfo.subpassCount = 1;
  rpInfo.pSubpasses = &subpass;
  rpInfo.dependencyCount = 1;
  rpInfo.pDependencies = &dep;

  if (vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass) != VK_SUCCESS)
    throw std::runtime_error("failed to create render pass!");
}

void createGraphicsPipeline(
    VkDevice device, VkExtent2D extent, VkRenderPass renderPass,
    const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts,
    VkPipelineLayout &pipelineLayout, VkPipeline &pipeline) {
  auto vertCode = readFile("shaders/vert.spv");
  auto fragCode = readFile("shaders/frag.spv");

  VkShaderModule vertModule = createShaderModule(device, vertCode);
  VkShaderModule fragModule = createShaderModule(device, fragCode);

  VkPipelineShaderStageCreateInfo vertStage{};
  vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStage.module = vertModule;
  vertStage.pName = "main";

  VkPipelineShaderStageCreateInfo fragStage{};
  fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStage.module = fragModule;
  fragStage.pName = "main";

  VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

  // Vertex input from Vertex struct
  auto bindingDesc = Vertex::getBindingDescription();
  auto attrDescs = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount = 1;
  vertexInput.pVertexBindingDescriptions = &bindingDesc;
  vertexInput.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attrDescs.size());
  vertexInput.pVertexAttributeDescriptions = attrDescs.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkViewport viewport{};
  viewport.width = (float)extent.width;
  viewport.height = (float)extent.height;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.extent = extent;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // Depth testing
  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  // Push constants
  VkPushConstantRange pushConstRange{};
  pushConstRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstRange.offset = 0;
  pushConstRange.size = sizeof(PushConstants);

  // Pipeline layout with all descriptor sets
  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount =
      static_cast<uint32_t>(descriptorSetLayouts.size());
  layoutInfo.pSetLayouts = descriptorSetLayouts.data();
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushConstRange;

  if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS)
    throw std::runtime_error("failed to create pipeline layout!");

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = stages;
  pipelineInfo.pVertexInputState = &vertexInput;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = renderPass;

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &pipeline) != VK_SUCCESS)
    throw std::runtime_error("failed to create graphics pipeline!");

  vkDestroyShaderModule(device, fragModule, nullptr);
  vkDestroyShaderModule(device, vertModule, nullptr);
}

void createFramebuffers(VkDevice device, VkRenderPass renderPass,
                        VkExtent2D extent,
                        const std::vector<VkImageView> &colorViews,
                        VkImageView depthView,
                        std::vector<VkFramebuffer> &framebuffers) {
  framebuffers.resize(colorViews.size());
  for (size_t i = 0; i < colorViews.size(); i++) {
    std::array<VkImageView, 2> attachments = {colorViews[i], depthView};

    VkFramebufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass = renderPass;
    ci.attachmentCount = static_cast<uint32_t>(attachments.size());
    ci.pAttachments = attachments.data();
    ci.width = extent.width;
    ci.height = extent.height;
    ci.layers = 1;

    if (vkCreateFramebuffer(device, &ci, nullptr, &framebuffers[i]) !=
        VK_SUCCESS)
      throw std::runtime_error("failed to create framebuffer!");
  }
}

//--------------------------------------------------------------
// Visibility Buffer Pass
//--------------------------------------------------------------
void createVisibilityRenderPass(VkDevice device, VkFormat depthFormat,
                                VkRenderPass &renderPass) {
  // Visibility buffer attachment (R32_UINT)
  VkAttachmentDescription visAttachment{};
  visAttachment.format = VK_FORMAT_R32_UINT;
  visAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  visAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  visAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  visAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  visAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  visAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  visAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference visRef{};
  visRef.attachment = 0;
  visRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // Depth attachment
  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = depthFormat;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef{};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &visRef;
  subpass.pDepthStencilAttachment = &depthRef;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.srcAccessMask = 0;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkAttachmentDescription, 2> attachments = {visAttachment,
                                                        depthAttachment};

  VkRenderPassCreateInfo rpInfo{};
  rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  rpInfo.pAttachments = attachments.data();
  rpInfo.subpassCount = 1;
  rpInfo.pSubpasses = &subpass;
  rpInfo.dependencyCount = 1;
  rpInfo.pDependencies = &dep;

  if (vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass) != VK_SUCCESS)
    throw std::runtime_error("failed to create visibility render pass!");
}

void createVisibilityPipeline(VkDevice device, VkExtent2D extent,
                              VkRenderPass renderPass,
                              VkDescriptorSetLayout uboLayout,
                              VkPipelineLayout &pipelineLayout,
                              VkPipeline &pipeline) {
  auto vertCode = readFile("shaders/visibility_vert.spv");
  auto fragCode = readFile("shaders/visibility_frag.spv");

  VkShaderModule vertModule = createShaderModule(device, vertCode);
  VkShaderModule fragModule = createShaderModule(device, fragCode);

  VkPipelineShaderStageCreateInfo vertStage{};
  vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStage.module = vertModule;
  vertStage.pName = "main";

  VkPipelineShaderStageCreateInfo fragStage{};
  fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStage.module = fragModule;
  fragStage.pName = "main";

  VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

  auto bindingDesc = Vertex::getBindingDescription();
  auto attrDescs = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount = 1;
  vertexInput.pVertexBindingDescriptions = &bindingDesc;
  vertexInput.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attrDescs.size());
  vertexInput.pVertexAttributeDescriptions = attrDescs.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkViewport viewport{};
  viewport.width = (float)extent.width;
  viewport.height = (float)extent.height;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.extent = extent;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

  // No blending for R32_UINT
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  // Push constants - drawID
  VkPushConstantRange pushConstRange{};
  pushConstRange.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstRange.offset = 0;
  pushConstRange.size = sizeof(PushConstants);

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &uboLayout;
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushConstRange;

  if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS)
    throw std::runtime_error("failed to create visibility pipeline layout!");

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = stages;
  pipelineInfo.pVertexInputState = &vertexInput;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = renderPass;

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &pipeline) != VK_SUCCESS)
    throw std::runtime_error("failed to create visibility pipeline!");

  vkDestroyShaderModule(device, fragModule, nullptr);
  vkDestroyShaderModule(device, vertModule, nullptr);
}

void createVisibilityFramebuffer(VkDevice device, VkRenderPass renderPass,
                                 VkExtent2D extent, VkImageView visView,
                                 VkImageView depthView,
                                 VkFramebuffer &framebuffer) {
  std::array<VkImageView, 2> attachments = {visView, depthView};

  VkFramebufferCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  ci.renderPass = renderPass;
  ci.attachmentCount = static_cast<uint32_t>(attachments.size());
  ci.pAttachments = attachments.data();
  ci.width = extent.width;
  ci.height = extent.height;
  ci.layers = 1;

  if (vkCreateFramebuffer(device, &ci, nullptr, &framebuffer) != VK_SUCCESS)
    throw std::runtime_error("failed to create visibility framebuffer!");
}

//--------------------------------------------------------------
// Debug Visualization Pass
//--------------------------------------------------------------
void createDebugVisRenderPass(VkDevice device, VkFormat swapChainFormat,
                              VkRenderPass &renderPass) {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapChainFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorRef{};
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo rpInfo{};
  rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rpInfo.attachmentCount = 1;
  rpInfo.pAttachments = &colorAttachment;
  rpInfo.subpassCount = 1;
  rpInfo.pSubpasses = &subpass;
  rpInfo.dependencyCount = 1;
  rpInfo.pDependencies = &dep;

  if (vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass) != VK_SUCCESS)
    throw std::runtime_error("failed to create debug vis render pass!");
}

void createDebugVisPipeline(VkDevice device, VkExtent2D extent,
                            VkRenderPass renderPass,
                            VkDescriptorSetLayout visBufferLayout,
                            VkPipelineLayout &pipelineLayout,
                            VkPipeline &pipeline) {
  auto vertCode = readFile("shaders/debug_vis_vert.spv");
  auto fragCode = readFile("shaders/debug_vis_frag.spv");

  VkShaderModule vertModule = createShaderModule(device, vertCode);
  VkShaderModule fragModule = createShaderModule(device, fragCode);

  VkPipelineShaderStageCreateInfo vertStage{};
  vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStage.module = vertModule;
  vertStage.pName = "main";

  VkPipelineShaderStageCreateInfo fragStage{};
  fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStage.module = fragModule;
  fragStage.pName = "main";

  VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

  // No vertex input for fullscreen triangle
  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkViewport viewport{};
  viewport.width = (float)extent.width;
  viewport.height = (float)extent.height;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.extent = extent;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &visBufferLayout;

  if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS)
    throw std::runtime_error("failed to create debug vis pipeline layout!");

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = stages;
  pipelineInfo.pVertexInputState = &vertexInput;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = renderPass;

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &pipeline) != VK_SUCCESS)
    throw std::runtime_error("failed to create debug vis pipeline!");

  vkDestroyShaderModule(device, fragModule, nullptr);
  vkDestroyShaderModule(device, vertModule, nullptr);
}

//--------------------------------------------------------------
// Compute Shade Pass
//--------------------------------------------------------------
void createComputeShadeDescriptorSetLayout(VkDevice device,
                                           VkDescriptorSetLayout &layout) {
  std::array<VkDescriptorSetLayoutBinding, 8> bindings{};

  // binding 0: output image (storage image)
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // binding 1: visibility buffer (sampler)
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // binding 2: depth buffer (sampler)
  bindings[2].binding = 2;
  bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // binding 3: camera UBO
  bindings[3].binding = 3;
  bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[3].descriptorCount = 1;
  bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // binding 4: vertex buffer (storage)
  bindings[4].binding = 4;
  bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[4].descriptorCount = 1;
  bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // binding 5: index buffer (storage)
  bindings[5].binding = 5;
  bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[5].descriptorCount = 1;
  bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // binding 6: draw buffer (storage)
  bindings[6].binding = 6;
  bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[6].descriptorCount = 1;
  bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // binding 7: material buffer (storage)
  bindings[7].binding = 7;
  bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[7].descriptorCount = 1;
  bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) !=
      VK_SUCCESS)
    throw std::runtime_error(
        "failed to create compute shade descriptor set layout!");
}

void createComputeShadePipeline(
    VkDevice device, const std::vector<VkDescriptorSetLayout> &layouts,
    VkPipelineLayout &pipelineLayout, VkPipeline &pipeline) {
  auto compCode = readFile("shaders/shade_comp.spv");
  VkShaderModule compModule = createShaderModule(device, compCode);

  VkPipelineShaderStageCreateInfo stageInfo{};
  stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageInfo.module = compModule;
  stageInfo.pName = "main";

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
  layoutInfo.pSetLayouts = layouts.data();

  if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS)
    throw std::runtime_error("failed to create compute shade pipeline layout!");

  VkComputePipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage = stageInfo;
  pipelineInfo.layout = pipelineLayout;

  if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                               nullptr, &pipeline) != VK_SUCCESS)
    throw std::runtime_error("failed to create compute shade pipeline!");

  vkDestroyShaderModule(device, compModule, nullptr);
}
