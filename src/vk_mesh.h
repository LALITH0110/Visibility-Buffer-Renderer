#pragma once

#include "vk_buffer.h"
#include <string>
#include <tiny_gltf.h>
#include <vector>

// Per-draw info (one per primitive)
struct DrawInfo {
  uint32_t firstIndex;
  uint32_t indexCount;
  int32_t vertexOffset;
  uint32_t materialIndex;
};

// Material data for SSBO
struct MaterialData {
  int32_t albedoTexIndex = -1;
  int32_t metalRoughTexIndex = -1;
  int32_t normalTexIndex = -1;
  int32_t padding = 0;
  glm::vec4 baseColorFactor = glm::vec4(1.0f);
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;
  float padding2[2] = {0, 0};
};

// Loaded scene data
struct Scene {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<DrawInfo> draws;
  std::vector<MaterialData> materials;

  // Texture data from glTF
  std::vector<tinygltf::Image> images;
};

// Load a glTF scene from file
bool loadGLTF(const std::string &filepath, Scene &scene);
