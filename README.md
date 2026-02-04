# Vulken - Visibility Buffer Renderer

A modern Vulkan 1.2 renderer demonstrating visibility buffer techniques for efficient deferred shading.

<p>
  <img width="404" height="430" alt="Screenshot 2026-02-04 at 2 58 51 AM" src="https://github.com/user-attachments/assets/77396cbc-cae7-4915-a1e5-a28f80214b78" /> 
  <img width="404" height="400" alt="Screenshot 2026-02-04 at 2 59 06 AM" src="https://github.com/user-attachments/assets/9e6f4d74-49e7-4e63-8551-5fef78621f43" />
</p>


## Features

- **Visibility Buffer Rendering** - Encodes (drawID, triangleID) per-pixel for deferred shading
- **Compute Shader Shading** - Barycentric interpolation and PBR in compute
- **PBR Shading** - Cook-Torrance BRDF with GGX NDF, Geometry Smith, Fresnel Schlick
- **glTF Scene Loading** - Via tinygltf with texture and material support
- **Orbital Camera** - Mouse drag/scroll for rotation and zoom
- **Real-time FPS Display** - In window title

## Controls

| Key | Action |
|-----|--------|
| `1` | Normal rendering |
| `2` | Normals visualization |
| `3` | UV coordinates visualization |
| `4` | Depth visualization |
| `5` | Metallic/Roughness visualization |
| `P` | Take screenshot |
| `ESC` | Quit |
| `Left Mouse` | Rotate camera |
| `Scroll` | Zoom camera |

## Building

### Prerequisites

- CMake 3.15+
- Vulkan SDK
- GLFW3
- GLM

### Build Steps

```bash
# Clone and setup
cd vulken
mkdir build && cd build
cmake ..
make -j4

# Compile shaders
cd ..
./compile_shaders.sh

# Run
./build/vulkan_app
```

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Visibility Buffer Pass                   │
│  ┌─────────────┐      ┌─────────────┐      ┌─────────────┐  │
│  │   Vertex    │──────│  Visibility │──────│   R32_UINT  │  │
│  │   Shader    │      │   Fragment  │      │   Buffer    │  │
│  └─────────────┘      └─────────────┘      └─────────────┘  │
└─────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                     Compute Shade Pass                       │
│  ┌─────────────┐      ┌─────────────┐      ┌─────────────┐  │
│  │   Vis Buf   │      │   Compute   │      │   Output    │  │
│  │   Sample    │──────│   Shader    │──────│   Image     │  │
│  └─────────────┘      │  (PBR/Bary) │      └─────────────┘  │
│                       └─────────────┘                        │
└─────────────────────────────────────────────────────────────┘
```

## Project Structure

```
vulken/
├── src/
│   ├── main.cpp           # Application entry, render loop
│   ├── vk_init.cpp/h      # Vulkan instance, device, swapchain
│   ├── vk_pipeline.cpp/h  # Graphics & compute pipelines
│   ├── vk_buffer.cpp/h    # VMA buffer/image helpers
│   ├── vk_descriptors.cpp/h # Descriptor management
│   ├── vk_mesh.cpp/h      # glTF loading
│   ├── vk_texture.cpp/h   # Texture loading with mipmaps
│   └── vk_camera.cpp/h    # Orbital camera
├── shaders/
│   ├── shader.vert/frag   # Forward pass
│   ├── visibility.vert/frag # Visibility buffer encoding
│   ├── shade.comp         # Compute shading with PBR
│   └── debug_vis.vert/frag # Debug visualization
├── external/
│   ├── vma/               # Vulkan Memory Allocator
│   └── tinygltf/          # glTF loader
└── assets/                # glTF scenes
```

## Technical Details

### Visibility Buffer Encoding

Each pixel stores a 32-bit uint:
- Bits 31-20: Draw ID (12 bits, up to 4096 draws)
- Bits 19-0: Triangle ID (20 bits, up to 1M triangles)

### Barycentric Reconstruction

The compute shader reconstructs barycentric coordinates by:
1. Projecting triangle vertices to screen space
2. Computing edge functions
3. Solving linear system for (u, v, w) weights

### PBR Implementation

Cook-Torrance BRDF with:
- **D**: GGX/Trowbridge-Reitz Normal Distribution
- **G**: Smith's geometry function with Schlick-GGX
- **F**: Fresnel-Schlick approximation

## MoltenVK Compatibility

Designed for macOS/Apple Silicon:
- Uses `VK_FORMAT_D32_SFLOAT` (D24 not supported)
- Fixed texture array size (32) instead of bindless
- VMA with Vulkan 1.2 features

