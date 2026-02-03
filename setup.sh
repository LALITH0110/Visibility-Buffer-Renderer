#!/bin/bash
# Vulkan environment setup for macOS with MoltenVK

export VULKAN_SDK=/usr/local/Cellar/molten-vk/1.4.0
export VK_ICD_FILENAMES=/usr/local/Cellar/molten-vk/1.4.0/etc/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH=/usr/local/Cellar/molten-vk/1.4.0/etc/vulkan/explicit_layer.d
export DYLD_LIBRARY_PATH=/usr/local/Cellar/molten-vk/1.4.0/lib:/usr/local/lib:$DYLD_LIBRARY_PATH

echo "Vulkan environment configured"
echo "VULKAN_SDK: $VULKAN_SDK"
