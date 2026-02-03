#include "vk_buffer.h"
#include "vk_camera.h"
#include "vk_descriptors.h"
#include "vk_init.h"
#include "vk_mesh.h"
#include "vk_pipeline.h"
#include "vk_texture.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

const int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanApp {
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  GLFWwindow *window = nullptr;
  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue presentQueue = VK_NULL_HANDLE;
  VmaAllocator allocator = VK_NULL_HANDLE;

  VkSwapchainKHR swapChain = VK_NULL_HANDLE;
  std::vector<VkImage> swapChainImages;
  VkFormat swapChainImageFormat{};
  VkExtent2D swapChainExtent{};
  std::vector<VkImageView> swapChainImageViews;

  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout textureSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline graphicsPipeline = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> swapChainFramebuffers;

  VkCommandPool commandPool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commandBuffers;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  uint32_t currentFrame = 0;

  AllocatedImage depthImage;
  VkImageView depthImageView = VK_NULL_HANDLE;

  // Scene data
  Scene scene;
  AllocatedBuffer vertexBuffer;
  AllocatedBuffer indexBuffer;

  // Textures
  std::vector<Texture> textures;
  Texture defaultTexture;
  VkSampler textureSampler = VK_NULL_HANDLE;

  // Uniform buffers
  std::vector<AllocatedBuffer> uniformBuffers;
  std::vector<void *> uniformBuffersMapped;

  // Material buffer (SSBO)
  AllocatedBuffer materialBuffer;

  // Descriptors
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> descriptorSets;
  VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;

  Camera camera;
  bool framebufferResized = false;

  // Debug and polish (M5)
  int debugMode = 0; // 0=normal, 1=normals, 2=UVs, 3=depth, 4=wireframe
  double lastFrameTime = 0.0;
  double deltaTime = 0.0;
  int frameCount = 0;
  double fpsTimer = 0.0;
  float currentFPS = 0.0f;
  bool screenshotRequested = false;

  static void framebufferResizeCallback(GLFWwindow *window, int width,
                                        int height) {
    auto app = reinterpret_cast<VulkanApp *>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
  }

  static void cursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
    auto app = reinterpret_cast<VulkanApp *>(glfwGetWindowUserPointer(window));
    if (app->camera.dragging) {
      double deltaX = xpos - app->camera.lastX;
      double deltaY = ypos - app->camera.lastY;
      app->camera.rotate(static_cast<float>(deltaX),
                         static_cast<float>(deltaY));
    }
    app->camera.lastX = xpos;
    app->camera.lastY = ypos;
  }

  static void mouseButtonCallback(GLFWwindow *window, int button, int action,
                                  int mods) {
    auto app = reinterpret_cast<VulkanApp *>(glfwGetWindowUserPointer(window));
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
      app->camera.dragging = (action == GLFW_PRESS);
    }
  }

  static void scrollCallback(GLFWwindow *window, double xoffset,
                             double yoffset) {
    auto app = reinterpret_cast<VulkanApp *>(glfwGetWindowUserPointer(window));
    app->camera.zoom(static_cast<float>(yoffset));
  }

  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulken - M2 glTF Scene", nullptr,
                              nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
  }

  void initVulkan() {
    createInstance(instance);
    createSurface(instance, window, surface);
    pickPhysicalDevice(instance, surface, physicalDevice);
    createLogicalDevice(physicalDevice, surface, device, graphicsQueue,
                        presentQueue);
    createAllocator(instance, physicalDevice, device, allocator);
    createSwapChain(physicalDevice, device, surface, window, swapChain,
                    swapChainImages, swapChainImageFormat, swapChainExtent);
    createImageViews(device, swapChainImages, swapChainImageFormat,
                     swapChainImageViews);
    createRenderPass(device, swapChainImageFormat, VK_FORMAT_D32_SFLOAT,
                     renderPass);
    createDescriptorSetLayout(device, descriptorSetLayout);
    createTextureDescriptorSetLayout(device, textureSetLayout);

    std::vector<VkDescriptorSetLayout> layouts = {descriptorSetLayout,
                                                  textureSetLayout};
    createGraphicsPipeline(device, swapChainExtent, renderPass, layouts,
                           pipelineLayout, graphicsPipeline);

    createCommandPool(device, physicalDevice, surface, commandPool);
    createDepthResources(allocator, device, swapChainExtent, depthImage,
                         depthImageView);
    createFramebuffers(device, renderPass, swapChainExtent, swapChainImageViews,
                       depthImageView, swapChainFramebuffers);

    loadScene();
    createTextures();
    createUniformBuffers();
    createDescriptors();
    createCommandBuffers();
    createSyncObjects();
  }

  void loadScene() {
    // Try loading glTF, fall back to cube if not found
    if (!loadGLTF("assets/DamagedHelmet/DamagedHelmet.gltf", scene)) {
      std::cout << "DamagedHelmet not found, using default cube" << std::endl;
      createDefaultCube();
    }

    // Upload vertex buffer
    VkDeviceSize vertSize = sizeof(Vertex) * scene.vertices.size();
    vertexBuffer = createBuffer(allocator, vertSize,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VMA_MEMORY_USAGE_GPU_ONLY);
    uploadBuffer(allocator, device, commandPool, graphicsQueue,
                 scene.vertices.data(), vertSize, vertexBuffer);

    // Upload index buffer
    VkDeviceSize idxSize = sizeof(uint32_t) * scene.indices.size();
    indexBuffer = createBuffer(allocator, idxSize,
                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VMA_MEMORY_USAGE_GPU_ONLY);
    uploadBuffer(allocator, device, commandPool, graphicsQueue,
                 scene.indices.data(), idxSize, indexBuffer);

    // Adjust camera for scene
    camera.distance = 3.0f;
  }

  void createDefaultCube() {
    scene.vertices = {
        // Front face
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        // Back face
        {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        // Top face
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        // Bottom face
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        // Right face
        {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        // Left face
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    };

    scene.indices = {0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,
                     8,  9,  10, 10, 11, 8,  12, 13, 14, 14, 15, 12,
                     16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};

    DrawInfo draw{};
    draw.firstIndex = 0;
    draw.indexCount = 36;
    draw.vertexOffset = 0;
    draw.materialIndex = 0;
    scene.draws.push_back(draw);

    scene.materials.push_back(MaterialData{});
  }

  void createTextures() {
    // Create default white texture
    defaultTexture =
        createDefaultTexture(allocator, device, commandPool, graphicsQueue);

    // Create textures from glTF images
    for (const auto &img : scene.images) {
      textures.push_back(createTextureFromImage(
          allocator, device, commandPool, graphicsQueue, physicalDevice, img));
    }

    // Create sampler
    uint32_t maxMip = textures.empty() ? 1 : textures[0].mipLevels;
    for (const auto &tex : textures) {
      if (tex.mipLevels > maxMip)
        maxMip = tex.mipLevels;
    }
    textureSampler = createTextureSampler(device, physicalDevice, maxMip);
  }

  void createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      uniformBuffers[i] = createBuffer(allocator, bufferSize,
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                       VMA_MEMORY_USAGE_CPU_TO_GPU);
      vmaMapMemory(allocator, uniformBuffers[i].allocation,
                   &uniformBuffersMapped[i]);
    }

    // Create material buffer from scene materials
    VkDeviceSize matSize = sizeof(MaterialData) * scene.materials.size();
    if (matSize == 0)
      matSize = sizeof(MaterialData); // At least one material

    materialBuffer = createBuffer(allocator, matSize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VMA_MEMORY_USAGE_GPU_ONLY);

    uploadBuffer(allocator, device, commandPool, graphicsQueue,
                 scene.materials.data(), matSize, materialBuffer);
  }

  void createDescriptors() {
    uint32_t texCount = static_cast<uint32_t>(textures.size()) + 1;
    createDescriptorPool(device, MAX_FRAMES_IN_FLIGHT, MAX_TEXTURES,
                         descriptorPool);

    std::vector<VkBuffer> buffers;
    for (auto &ub : uniformBuffers)
      buffers.push_back(ub.buffer);

    VkDeviceSize matSize = sizeof(MaterialData) * scene.materials.size();
    if (matSize == 0)
      matSize = sizeof(MaterialData);

    createDescriptorSets(device, descriptorPool, descriptorSetLayout, buffers,
                         sizeof(UniformBufferObject), materialBuffer.buffer,
                         matSize, descriptorSets);

    // Create texture descriptor set
    std::vector<VkImageView> views;
    views.push_back(defaultTexture.view); // Default at index 0
    for (const auto &tex : textures) {
      views.push_back(tex.view);
    }

    createTextureDescriptorSet(device, descriptorPool, textureSetLayout,
                               textureSampler, views, textureDescriptorSet);
  }

  void createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    allocateCommandBuffers(device, commandPool, MAX_FRAMES_IN_FLIGHT,
                           commandBuffers.data());
  }

  void createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      if (vkCreateSemaphore(device, &semInfo, nullptr,
                            &imageAvailableSemaphores[i]) != VK_SUCCESS ||
          vkCreateSemaphore(device, &semInfo, nullptr,
                            &renderFinishedSemaphores[i]) != VK_SUCCESS ||
          vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) !=
              VK_SUCCESS)
        throw std::runtime_error("failed to create sync objects!");
    }
  }

  void updateUniformBuffer(uint32_t currentImage) {
    camera.aspectRatio = swapChainExtent.width / (float)swapChainExtent.height;

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = camera.getViewMatrix();
    ubo.projection = camera.getProjectionMatrix();
    ubo.cameraPos = camera.getPosition();

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }

  void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
      throw std::runtime_error("failed to begin recording command buffer!");

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass;
    rpInfo.framebuffer = swapChainFramebuffers[imageIndex];
    rpInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.02f, 0.02f, 0.02f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkBuffer vertexBuffers[] = {vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind descriptor sets
    VkDescriptorSet sets[] = {descriptorSets[currentFrame],
                              textureDescriptorSet};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 2, sets, 0, nullptr);

    // Draw each primitive
    for (const auto &draw : scene.draws) {
      PushConstants pc{};
      pc.materialIndex = draw.materialIndex;
      vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(pc), &pc);

      vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex,
                       draw.vertexOffset, 0);
    }

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
      throw std::runtime_error("failed to record command buffer!");
  }

  void cleanupSwapChain() {
    vkDestroyImageView(device, depthImageView, nullptr);
    destroyImage(allocator, depthImage);

    for (auto fb : swapChainFramebuffers)
      vkDestroyFramebuffer(device, fb, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    for (auto iv : swapChainImageViews)
      vkDestroyImageView(device, iv, nullptr);
    vkDestroySwapchainKHR(device, swapChain, nullptr);
  }

  void recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);
    cleanupSwapChain();

    createSwapChain(physicalDevice, device, surface, window, swapChain,
                    swapChainImages, swapChainImageFormat, swapChainExtent);
    createImageViews(device, swapChainImages, swapChainImageFormat,
                     swapChainImageViews);
    createRenderPass(device, swapChainImageFormat, VK_FORMAT_D32_SFLOAT,
                     renderPass);

    std::vector<VkDescriptorSetLayout> layouts = {descriptorSetLayout,
                                                  textureSetLayout};
    createGraphicsPipeline(device, swapChainExtent, renderPass, layouts,
                           pipelineLayout, graphicsPipeline);

    createDepthResources(allocator, device, swapChainExtent, depthImage,
                         depthImageView);
    createFramebuffers(device, renderPass, swapChainExtent, swapChainImageViews,
                       depthImageView, swapChainFramebuffers);
  }

  void drawFrame() {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                    UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapChain();
      return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    updateUniformBuffer(currentFrame);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                      inFlightFences[currentFrame]) != VK_SUCCESS)
      throw std::runtime_error("failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        framebufferResized) {
      framebufferResized = false;
      recreateSwapChain();
    } else if (result != VK_SUCCESS) {
      throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  void mainLoop() {
    lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      // Calculate delta time and FPS
      double currentTime = glfwGetTime();
      deltaTime = currentTime - lastFrameTime;
      lastFrameTime = currentTime;

      frameCount++;
      fpsTimer += deltaTime;
      if (fpsTimer >= 1.0) {
        currentFPS =
            static_cast<float>(frameCount) / static_cast<float>(fpsTimer);
        frameCount = 0;
        fpsTimer = 0.0;

        // Update window title with FPS and debug mode
        const char *modeNames[] = {"Normal", "Normals", "UVs", "Depth",
                                   "Metallic/Rough"};
        char title[128];
        snprintf(title, sizeof(title), "Vulken | FPS: %.1f | Mode: %s (1-5)",
                 currentFPS, modeNames[debugMode]);
        glfwSetWindowTitle(window, title);
      }

      drawFrame();
    }
    vkDeviceWaitIdle(device);
  }

  void cleanup() {
    cleanupSwapChain();

    vkDestroySampler(device, textureSampler, nullptr);
    for (auto &tex : textures)
      destroyTexture(allocator, device, tex);
    destroyTexture(allocator, device, defaultTexture);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vmaUnmapMemory(allocator, uniformBuffers[i].allocation);
      destroyBuffer(allocator, uniformBuffers[i]);
    }

    destroyBuffer(allocator, indexBuffer);
    destroyBuffer(allocator, vertexBuffer);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
      vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
      vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
  }
};

int main() {
  VulkanApp app;
  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
