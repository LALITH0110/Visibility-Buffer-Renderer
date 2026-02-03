#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
} ubo;

// Material data structure matching MaterialData in vk_mesh.h
struct Material {
    int albedoTexIndex;
    int metalRoughTexIndex;
    int normalTexIndex;
    int padding;
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float padding2[2];
};

layout(std430, set = 0, binding = 1) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 1, binding = 0) uniform sampler2D textures[32];

layout(push_constant) uniform PushConstants {
    uint drawID;
    uint materialIndex;
} pc;

// PBR constants
const vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
const vec3 lightColor = vec3(3.0);
const float PI = 3.14159265359;

// GGX Normal Distribution
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Geometry Schlick-GGX
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Fresnel Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    Material mat = materials[pc.materialIndex];
    
    // Sample albedo
    vec4 albedo = mat.baseColorFactor;
    if (mat.albedoTexIndex >= 0) {
        albedo *= texture(textures[mat.albedoTexIndex], fragUV);
    }
    
    // Sample metallic/roughness
    float metallic = mat.metallicFactor;
    float roughness = mat.roughnessFactor;
    if (mat.metalRoughTexIndex >= 0) {
        vec4 mr = texture(textures[mat.metalRoughTexIndex], fragUV);
        metallic *= mr.b;  // Blue channel
        roughness *= mr.g; // Green channel
    }
    
    // Normal
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.cameraPos - fragWorldPos);
    vec3 L = lightDir;
    vec3 H = normalize(V + L);
    
    // F0 - surface reflection at zero incidence
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo.rgb / PI + specular) * lightColor * NdotL;
    
    // Ambient
    vec3 ambient = vec3(0.03) * albedo.rgb;
    vec3 color = ambient + Lo;
    
    // Tone mapping (ACES-ish)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    
    outColor = vec4(color, 1.0);
}

