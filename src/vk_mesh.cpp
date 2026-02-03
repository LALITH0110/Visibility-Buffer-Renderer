#include "vk_mesh.h"
#include <cstring>
#include <iostream>

static void loadPrimitive(const tinygltf::Model &model,
                          const tinygltf::Primitive &prim, Scene &scene,
                          int materialIdx) {
  // Track vertex offset for this primitive
  int32_t vertexOffset = static_cast<int32_t>(scene.vertices.size());
  uint32_t firstIndex = static_cast<uint32_t>(scene.indices.size());

  // Get accessors
  const auto &posAccessor =
      model.accessors[prim.attributes.find("POSITION")->second];
  const auto &posView = model.bufferViews[posAccessor.bufferView];
  const auto &posBuffer = model.buffers[posView.buffer];
  const float *posData = reinterpret_cast<const float *>(
      posBuffer.data.data() + posView.byteOffset + posAccessor.byteOffset);
  size_t posStride =
      posView.byteStride ? posView.byteStride / sizeof(float) : 3;

  // Normals (optional)
  const float *normData = nullptr;
  size_t normStride = 3;
  if (prim.attributes.count("NORMAL")) {
    const auto &normAccessor =
        model.accessors[prim.attributes.find("NORMAL")->second];
    const auto &normView = model.bufferViews[normAccessor.bufferView];
    const auto &normBuffer = model.buffers[normView.buffer];
    normData = reinterpret_cast<const float *>(
        normBuffer.data.data() + normView.byteOffset + normAccessor.byteOffset);
    normStride = normView.byteStride ? normView.byteStride / sizeof(float) : 3;
  }

  // UVs (optional)
  const float *uvData = nullptr;
  size_t uvStride = 2;
  if (prim.attributes.count("TEXCOORD_0")) {
    const auto &uvAccessor =
        model.accessors[prim.attributes.find("TEXCOORD_0")->second];
    const auto &uvView = model.bufferViews[uvAccessor.bufferView];
    const auto &uvBuffer = model.buffers[uvView.buffer];
    uvData = reinterpret_cast<const float *>(
        uvBuffer.data.data() + uvView.byteOffset + uvAccessor.byteOffset);
    uvStride = uvView.byteStride ? uvView.byteStride / sizeof(float) : 2;
  }

  // Add vertices
  for (size_t i = 0; i < posAccessor.count; i++) {
    Vertex v{};
    v.pos = glm::vec3(posData[i * posStride], posData[i * posStride + 1],
                      posData[i * posStride + 2]);
    if (normData) {
      v.normal =
          glm::vec3(normData[i * normStride], normData[i * normStride + 1],
                    normData[i * normStride + 2]);
    } else {
      v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    if (uvData) {
      v.uv = glm::vec2(uvData[i * uvStride], uvData[i * uvStride + 1]);
    } else {
      v.uv = glm::vec2(0.0f);
    }
    scene.vertices.push_back(v);
  }

  // Load indices
  if (prim.indices >= 0) {
    const auto &idxAccessor = model.accessors[prim.indices];
    const auto &idxView = model.bufferViews[idxAccessor.bufferView];
    const auto &idxBuffer = model.buffers[idxView.buffer];
    const uint8_t *idxData =
        idxBuffer.data.data() + idxView.byteOffset + idxAccessor.byteOffset;

    for (size_t i = 0; i < idxAccessor.count; i++) {
      uint32_t idx = 0;
      switch (idxAccessor.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        idx = reinterpret_cast<const uint16_t *>(idxData)[i];
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        idx = reinterpret_cast<const uint32_t *>(idxData)[i];
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        idx = idxData[i];
        break;
      }
      scene.indices.push_back(idx);
    }
  }

  // Create draw info
  DrawInfo draw{};
  draw.firstIndex = firstIndex;
  draw.indexCount = static_cast<uint32_t>(scene.indices.size()) - firstIndex;
  draw.vertexOffset = vertexOffset;
  draw.materialIndex =
      materialIdx >= 0 ? static_cast<uint32_t>(materialIdx) : 0;
  scene.draws.push_back(draw);
}

static void loadNode(const tinygltf::Model &model, int nodeIdx, Scene &scene) {
  const auto &node = model.nodes[nodeIdx];

  if (node.mesh >= 0) {
    const auto &mesh = model.meshes[node.mesh];
    for (const auto &prim : mesh.primitives) {
      if (prim.mode == TINYGLTF_MODE_TRIANGLES) {
        loadPrimitive(model, prim, scene, prim.material);
      }
    }
  }

  // Recurse into children
  for (int child : node.children) {
    loadNode(model, child, scene);
  }
}

bool loadGLTF(const std::string &filepath, Scene &scene) {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err, warn;

  bool ret;
  std::string glbExt = ".glb";
  bool isGlb = filepath.size() >= glbExt.size() &&
               filepath.compare(filepath.size() - glbExt.size(), glbExt.size(),
                                glbExt) == 0;
  if (isGlb) {
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
  } else {
    ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
  }

  if (!warn.empty())
    std::cerr << "glTF warning: " << warn << std::endl;
  if (!err.empty())
    std::cerr << "glTF error: " << err << std::endl;
  if (!ret)
    return false;

  // Load materials
  for (const auto &mat : model.materials) {
    MaterialData md{};
    const auto &pbr = mat.pbrMetallicRoughness;

    md.baseColorFactor =
        glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1],
                  pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
    md.metallicFactor = static_cast<float>(pbr.metallicFactor);
    md.roughnessFactor = static_cast<float>(pbr.roughnessFactor);

    if (pbr.baseColorTexture.index >= 0) {
      md.albedoTexIndex = model.textures[pbr.baseColorTexture.index].source;
    }
    if (pbr.metallicRoughnessTexture.index >= 0) {
      md.metalRoughTexIndex =
          model.textures[pbr.metallicRoughnessTexture.index].source;
    }
    if (mat.normalTexture.index >= 0) {
      md.normalTexIndex = model.textures[mat.normalTexture.index].source;
    }

    scene.materials.push_back(md);
  }

  // Default material if none exist
  if (scene.materials.empty()) {
    scene.materials.push_back(MaterialData{});
  }

  // Store images for texture creation
  scene.images = model.images;

  // Load nodes - use default scene or scene 0
  int sceneIdx = model.defaultScene >= 0 ? model.defaultScene : 0;
  if (sceneIdx < static_cast<int>(model.scenes.size())) {
    for (int nodeIdx : model.scenes[sceneIdx].nodes) {
      loadNode(model, nodeIdx, scene);
    }
  }

  std::cout << "Loaded glTF: " << scene.vertices.size() << " vertices, "
            << scene.indices.size() << " indices, " << scene.draws.size()
            << " draws, " << scene.materials.size() << " materials, "
            << scene.images.size() << " images" << std::endl;

  return true;
}
