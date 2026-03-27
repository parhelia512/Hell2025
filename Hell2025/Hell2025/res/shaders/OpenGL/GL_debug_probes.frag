#version 460
#include "../common/ddgi.glsl"
#include "../common/types.glsl"

layout (location = 0) out vec4 FragOut;

layout(binding = 0) uniform sampler2DArray u_distanceAtlas;

layout(std430, binding = 6) readonly buffer ProbeColorBuffer       { ProbeColor probeColors[]; };
layout(std430, binding = 7) readonly buffer DDGIVolumeBuffer       { DDGIVolume volume; };
layout(std430, binding = 8) readonly buffer ProbeVisibleListBuffer { uint probeVisibility[]; };

flat in int v_probeIndex;
flat in ivec3 v_voxelCoord;
in vec3 v_worldPos;
in vec3 v_normal;

vec3 GetProbeWorldPos(int probeIdx) {
    ivec3 coords = DDGIGetProbeCoords(probeIdx, volume.probeCounts);
    return volume.origin + vec3(coords) * volume.probeSpacing;
}

vec3 GetColor(int probeIdx) {
    // uses the unified SH helper from ddgi.glsl
    return ReconstructSH(probeColors[probeIdx], normalize(v_normal));
}

vec3 GetDistance(int probeIdx) {
    // get octahedral uv for the current surface normal
    vec2 oct = DDGIGetOctahedralCoordinates(normalize(v_normal));
    
    // get 3D UVW for the texture array (Z is layer)
    // 14 is our interior texel count
    vec3 probeUVW = DDGIGetProbeUV(probeIdx, oct, 14, volume.probeCounts);

    vec2 moments = texture(u_distanceAtlas, probeUVW).rg;
    
    // map to grayscale for visual debugging
    float maxRange = volume.probeSpacing * 1.5;
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