#version 450

layout(location = 0) out uint outVisibility;

layout(push_constant) uniform PushConstants {
    uint drawID;
} pc;

void main() {
    // Encode visibility as (drawID << 20) | triangleID
    // gl_PrimitiveID gives us the triangle index within this draw
    uint triangleID = uint(gl_PrimitiveID) & 0xFFFFF;  // 20 bits for triangle
    uint drawBits = (pc.drawID & 0xFFF) << 20;          // 12 bits for draw
    outVisibility = drawBits | triangleID;
}
