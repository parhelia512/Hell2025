#version 460
#include "../common/ddgi.glsl"
#include "../common/types.glsl"

layout (location = 0) out vec4 FragOut;

layout(binding = 0) uniform sampler2DArray u_distanceAtlas;
layout(binding = 1) uniform sampler2DArray u_irradianceAtlas;

layout(std430, binding = 6) readonly buffer ProbeColorBuffer  { ProbeColor probeColors[]; };
layout(std430, binding = 7) readonly buffer DDGIVolumeBuffer  { DDGIVolume volume; };
layout(std430, binding = 8) readonly buffer ProbeStatesBuffer { ProbeState probeStates[]; };

flat in int v_probeIndex;
flat in ivec3 v_voxelCoord;
in vec3 v_worldPos;
in vec3 v_normal;

uniform bool u_useSH;

vec3 GetColor(int probeIdx) {
    const float encodingGamma = 5.0; // Must match your Compute Shader!
    
    // 1. Get octahedral uv for the current surface normal
    vec2 oct = DDGIGetOctahedralCoordinates(normalize(v_normal));
    
    // 2. Get 3D UVW for the texture array
    // Ensure '6' matches 'RTXGI_DDGI_PROBE_NUM_IRRADIANCE_INTERIOR_TEXELS'
    vec3 probeUVW = DDGIGetProbeUV(probeIdx, oct, 6, volume);

    // 3. Sample the atlas
    vec3 sampledValue = texture(u_irradianceAtlas, probeUVW).rgb;
    
    // 4. DECODE: Reverse the gamma encoding to get linear light
    // This is what turns that "grey" back into "black"
    vec3 linearIrradiance = pow(max(vec3(0.0), sampledValue), vec3(encodingGamma));
    
    // 5. NVIDIA Normalization: 
    // The NVIDIA shader multiplies by 2PI at the very end of the gather[cite: 136].
    // If your compute shader didn't do this, you might need it here.
    return linearIrradiance; 
}

vec3 GetColorSH(int probeIdx) {
    return ReconstructSH(probeColors[probeIdx], normalize(v_normal));
}

vec3 GetDistance(int probeIdx) {
    // get octahedral uv for the current surface normal
    vec2 oct = DDGIGetOctahedralCoordinates(normalize(v_normal));
    
    // get 3D UVW for the texture array (Z is layer)
    // 14 is our interior texel count
    vec3 probeUVW = DDGIGetProbeUV(probeIdx, oct, 14, volume);

    vec2 moments = texture(u_distanceAtlas, probeUVW).rg;
    
    // map to grayscale for visual debugging
    float maxRange = volume.probeSpacing * PROBE_MAX_RAY_DISTANCE;
    float grayscale = clamp(moments.x / maxRange, 0.0, 1.0);

    return vec3(grayscale);
}

vec3 GetVisiblity(int probeIdx) {
    if (probeStates[probeIdx].isVisible) {
        return vec3(0, 1, 0);
    }
    else {
        return vec3(1, 0, 0);
    }
}

vec3 GetActiveState(int probeIdx) {
    if (probeStates[probeIdx].isActive) {
        return vec3(0, 1, 0);
    }
    else {
        return vec3(1, 0, 0);
    }
}

vec3 GetDistanceCooldown(int probeIdx) {
    uint cooldown = probeStates[probeIdx].distanceCooldown;
    float value = float(cooldown) / float(PROBE_MAX_DISTANCE_COOLDOWN);
    return vec3(value, 0.0, 0.0);
}


vec3 GetDisttanceWithCooldown(int probeIdx) {
    vec3 dist = GetDistance(probeIdx);
    vec3 distanceCooldown = GetDistanceCooldown(probeIdx);
    
    if (distanceCooldown.x > 0) {
        return vec3(dist.x, 0, 0);
    }
    else {
        return dist;
    }
}

void main() {
    int probeIdx = v_probeIndex;
    
    vec3 color = u_useSH ? GetColorSH(probeIdx) : GetColor(probeIdx);
    vec3 dist = GetDistance(probeIdx);
    vec3 visibility = GetVisiblity(probeIdx);
    vec3 activeState = GetActiveState(probeIdx);
    vec3 distanceCooldown = GetDistanceCooldown(probeIdx);
    vec3 distanceWithCoolDown = GetDisttanceWithCooldown(probeIdx);
    //if (activeState.x == 1) discard;
    
    FragOut = vec4(visibility, 1.0);
    //FragOut = vec4(color + dist, 1.0);
}