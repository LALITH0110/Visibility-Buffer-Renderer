#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform usampler2D visBuffer;

// Hash function for false color visualization
vec3 hashColor(uint id) {
    // Simple hash to generate distinct colors
    uint h = id * 2654435761u;
    float r = float((h >> 0) & 0xFF) / 255.0;
    float g = float((h >> 8) & 0xFF) / 255.0;
    float b = float((h >> 16) & 0xFF) / 255.0;
    return vec3(r, g, b);
}

void main() {
    uvec4 vis = texture(visBuffer, inUV);
    uint visData = vis.r;
    
    if (visData == 0u) {
        // Background - dark gray
        outColor = vec4(0.02, 0.02, 0.02, 1.0);
        return;
    }
    
    // Decode visibility data
    uint drawID = visData >> 20;
    uint triangleID = visData & 0xFFFFF;
    
    // False color based on combined ID for "stained glass" effect
    vec3 color = hashColor(visData);
    
    outColor = vec4(color, 1.0);
}
