cmake_minimum_required(VERSION 3.15)
project(Vulken)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(vulkan_app
    src/main.cpp
    src/vk_init.cpp
    src/vk_pipeline.cpp
    src/vk_buffer.cpp
    src/vk_descriptors.cpp
    src/vk_mesh.cpp
    src/vk_texture.cpp
    src/vk_mem_impl.cpp
    src/tinygltf_impl.cpp
)

target_include_directories(vulkan_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/vma
    ${CMAKE_SOURCE_DIR}/external/tinygltf
)

target_link_libraries(vulkan_app
    Vulkan::Vulkan
    glfw
    glm::glm
)

if(APPLE)
    target_compile_definitions(vulkan_app PRIVATE VK_USE_PLATFORM_MACOS_MVK)
endif()

# Suppress VMA nullability warnings on Apple
set_source_files_properties(src/vk_mem_impl.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-nullability-completeness"
)
