#!/bin/bash
# Compile all GLSL shaders to SPIR-V
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SHADER_DIR="$SCRIPT_DIR/shaders"

# Find compiler — prefer glslc, fall back to glslangValidator
if [ -n "$VULKAN_SDK" ] && [ -x "$VULKAN_SDK/bin/glslc" ]; then
    COMPILER="$VULKAN_SDK/bin/glslc"
    USE_GLSLC=1
elif command -v glslc &>/dev/null; then
    COMPILER=$(command -v glslc)
    USE_GLSLC=1
elif command -v glslangValidator &>/dev/null; then
    COMPILER=$(command -v glslangValidator)
    USE_GLSLC=0
else
    echo "Error: No GLSL compiler found. Install glslang or Vulkan SDK."
    exit 1
fi

echo "Using: $COMPILER"

compile() {
    local src="$1"
    local dst="$2"
    echo "  $src -> $dst"
    if [ "$USE_GLSLC" = "1" ]; then
        "$COMPILER" "$src" -o "$dst"
    else
        "$COMPILER" -V "$src" -o "$dst"
    fi
}

compile "$SHADER_DIR/shader.vert" "$SHADER_DIR/vert.spv"
compile "$SHADER_DIR/shader.frag" "$SHADER_DIR/frag.spv"
compile "$SHADER_DIR/visibility.vert" "$SHADER_DIR/visibility_vert.spv"
compile "$SHADER_DIR/visibility.frag" "$SHADER_DIR/visibility_frag.spv"
compile "$SHADER_DIR/debug_vis.vert" "$SHADER_DIR/debug_vis_vert.spv"
compile "$SHADER_DIR/debug_vis.frag" "$SHADER_DIR/debug_vis_frag.spv"
compile "$SHADER_DIR/shade.comp" "$SHADER_DIR/shade_comp.spv"

echo "Done."
