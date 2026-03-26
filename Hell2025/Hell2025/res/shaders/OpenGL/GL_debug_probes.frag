#version 460
#include "../common/ddgi.glsl"
#include "../common/types.glsl"

layout (location = 0) out vec4 FragOut;

layout(binding = 0) uniform sampler2DArray u_distanceAtlas;

layout(std430, binding = 6) buffer ProbeColorBuffer { ProbeColor probeColors[]; };
layout(std430, binding = 7) buffer ProbeDistanceBuffer { ProbeDistance probeDistances[]; };
layout(std430, binding = 8) buffer ProbeVisibleListBuffer { uint probeVisibility[]; };

flat in int v_probeIndex;
flat in ivec3 v_voxelCoord;
in vec3 v_worldPos;
in vec3 v_normal;

uniform int u_probeCount;
uniform vec3 u_gridOffset;
uniform float u_spacing;
uniform int u_gridWidth;
uniform int u_gridHeight;
uniform int u_gridDepth;

vec3 GetProbeWorldPos(int probeIdx) {
    ivec3 probeCounts = ivec3(u_gridWidth, u_gridHeight, u_gridDepth);
    ivec3 coords = DDGIGetProbeCoords(probeIdx, probeCounts);
    return u_gridOffset + vec3(coords) * u_spacing;
}

vec3 GetColor(int probeIdx) {
    // uses the unified SH helper from ddgi.glsl
    return ReconstructSH(probeColors[probeIdx], normalize(v_normal));
}

vec3 GetDistance(int probeIdx) {
    ivec3 probeCounts = ivec3(u_gridWidth, u_gridHeight, u_gridDepth);
    
    // get octahedral uv for the current surface normal
    vec2 oct = DDGIGetOctahedralCoordinates(normalize(v_normal));
    
    // get 3D UVW for the texture array (Z is layer)
    // 14 is our interior texel count
    vec3 probeUVW = DDGIGetProbeUV(probeIdx, oct, 14, probeCounts);

    vec2 moments = texture(u_distanceAtlas, probeUVW).rg;
    
    // map to grayscale for visual debugging
    float maxRange = u_spacing * 1.5;
    float grayscale = clamp(moments.x / maxRange, 0.0, 1.0);

    return vec3(grayscale);
}

float GetVisiblity(int probeIdx) {
    return float(probeVisibility[probeIdx]);
}

void main() {
    int probeIdx = v_probeIndex;
    
    vec3 color = GetColor(probeIdx);
    vec3 dist = GetDistance(probeIdx);
    float visibility = GetVisiblity(probeIdx);
    
    // visualize color by default, or multiply by visibility to see the culling in action
    FragOut = vec4(color, 1.0);
    FragOut = vec4(dist, 1.0);
    //FragOut = vec4(vec3(visibility), 1.0);
}