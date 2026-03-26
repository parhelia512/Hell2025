#include "types.glsl"

float SignNotZero(float v) {
    return (v >= 0.0) ? 1.0 : -1.0;
}

vec2 SignNotZero(vec2 v) {
    return vec2(SignNotZero(v.x), SignNotZero(v.y));
}

// Map 2d texture coordinates to a normalized octahedral space (-1 to 1)
vec2 DDGIGetNormalizedOctahedralCoordinates(ivec2 texCoords, int numTexels) {
    vec2 octahedralTexelCoord = vec2(texCoords.x % numTexels, texCoords.y % numTexels);
    octahedralTexelCoord += 0.5;
    octahedralTexelCoord /= float(numTexels);
    octahedralTexelCoord *= 2.0;
    octahedralTexelCoord -= 1.0;
    return octahedralTexelCoord;
}

// Octahedral UV -> Direction
vec3 DDGIGetOctahedralDirection(vec2 coords) {
    vec3 direction = vec3(coords.x, coords.y, 1.0 - abs(coords.x) - abs(coords.y));
    if (direction.z < 0.0) {
        direction.xy = (1.0 - abs(direction.yx)) * SignNotZero(direction.xy);
    }
    return normalize(direction);
}

// Direction -> Octahedral UV
vec2 DDGIGetOctahedralCoordinates(vec3 direction) {
    float l1norm = abs(direction.x) + abs(direction.y) + abs(direction.z);
    vec2 uv = direction.xy * (1.0 / l1norm);
    if (direction.z < 0.0) {
        uv = (1.0 - abs(uv.yx)) * SignNotZero(uv.xy);
    }
    return uv;
}

// Plane based Indexing
int DDGIGetProbesPerPlane(ivec3 probeCounts) {
    return probeCounts.x * probeCounts.z;
}

int DDGIGetPlaneIndex(ivec3 probeCoords) {
    return probeCoords.y;
}

int DDGIGetProbeIndexInPlane(ivec3 probeCoords, ivec3 probeCounts) {
    return probeCoords.x + (probeCounts.x * probeCoords.z);
}

int DDGIGetProbeIndex(ivec3 probeCoords, ivec3 probeCounts) {
    // how many probes are in one horizontal slice
    int probesPerPlane = probeCounts.x * probeCounts.z;
    
    // index within that specific horizontal slice
    int probeIndexInPlane = probeCoords.x + (probeCounts.x * probeCoords.z);
    
    // final 1D index: (Y_slice * Probes_Per_Slice) + Index_In_Slice
    return (probeCoords.y * probesPerPlane) + probeIndexInPlane;
}

// Reverse index to 3D grid coords
ivec3 DDGIGetProbeCoords(int probeIndex, ivec3 probeCounts) {
    ivec3 probeCoords;
    int probesPerPlane = DDGIGetProbesPerPlane(probeCounts);
    
    // Extract Y (the slowest moving dimension)
    probeCoords.y = probeIndex / probesPerPlane;
    int remaining = probeIndex % probesPerPlane;
    
    // Extract Z and X from the remaining plane index
    probeCoords.z = remaining / probeCounts.x;
    probeCoords.x = remaining % probeCounts.x;
    
    return probeCoords;
}


// Find base probe (bottom-left-front) nearest to world position
ivec3 DDGIGetBaseProbeGridCoords(vec3 worldPosition, vec3 gridOrigin, float probeSpacing, ivec3 probeCounts) {
    // Get the vector from the volume origin to the surface point
    // float3 position = worldPosition - (volume.origin + (volume.probeScrollOffsets * volume.probeSpacing));
    vec3 position = worldPosition - gridOrigin;
    
    // Rotate the world position into the volume's space
    // if(!IsVolumeMovementScrolling(volume)) position = RTXGIQuaternionRotate(position, RTXGIQuaternionConjugate(volume.rotation));
    // not applicable

    // Shift from [-n/2, n/2] to [0, n] (grid space)
    //position += (volume.probeSpacing * (volume.probeCounts - 1)) * 0.5f;
    // position += (probeSpacing * (vec3(probeCounts) - 1.0)) * 0.5; NOT NEEDED COZ U R NOT AT CENTER ORIGIN

    // Quantize the position to grid space
    // int3 probeCoords = int3(position / volume.probeSpacing);
    ivec3 probeCoords = ivec3(position / probeSpacing);
    
    // Clamp to [0, probeCounts - 1]
    // Snaps positions outside of grid to the grid edge
    // probeCoords = clamp(probeCoords, int3(0, 0, 0), (volume.probeCounts - int3(1, 1, 1)));
    probeCoords = clamp(probeCoords, ivec3(0), probeCounts - ivec3(1));
    return probeCoords;
}

//------------------------------------------------------------------------
// Probe World Position
//------------------------------------------------------------------------

/**
 * Computes the world-space position of a probe from the probe's 3D grid-space coordinates.
 * Probe relocation is not considered.
 */
 
//float3 DDGIGetProbeWorldPosition(int3 probeCoords, DDGIVolumeDescGPU volume) {
vec3 DDGIGetBaseProbeGridCoords(ivec3 probeCoords, vec3 gridOrigin, float probeSpacing) {//, ivec3 probeCounts) {
    // Multiply the grid coordinates by the probe spacing
    //float3 probeGridWorldPosition = probeCoords * volume.probeSpacing;
    vec3 probeGridWorldPosition = probeCoords * probeSpacing;

    // Shift the grid of probes by half of each axis extent to center the volume about its origin
    //float3 probeGridShift = (volume.probeSpacing * (volume.probeCounts - 1)) * 0.5f;
    //vec3 probeGridShift = (probeSpacing * (probeCounts - 1)) * 0.5; NOT APPLICABLE COZ YOUR VOLUME HAS BOTTOM LEFT ORIGIN

    // Center the probe grid about the origin
    //float3 probeWorldPosition = (probeGridWorldPosition - probeGridShift);
    vec3 probeWorldPosition = (probeGridWorldPosition); // - probeGridShift); NOT REQUIRED COZ U HAVE NO GRID SHIFT

    // Rotate the probe grid if infinite scrolling is not enabled
    //if (!IsVolumeMovementScrolling(volume)) probeWorldPosition = RTXGIQuaternionRotate(probeWorldPosition, volume.rotation);

    // Translate the grid to the volume's center
    //probeWorldPosition += volume.origin + (volume.probeScrollOffsets * volume.probeSpacing);
    probeWorldPosition += gridOrigin;// + (volume.probeScrollOffsets * probeSpacing); ALSO NOT APPLICABLE

    return probeWorldPosition;
}

vec3 DDGIGetProbeUV(int probeIndex, vec2 octantCoordinates, int numProbeInteriorTexels, ivec3 probeCounts) {
    ivec3 coords = DDGIGetProbeCoords(probeIndex, probeCounts);
    
    // Total size including 1-pixel borders on each side
    float numProbeTexels = float(numProbeInteriorTexels) + 2.0;
    
    // 1. Calculate the top-left corner of this probe's 16x16 block in the atlas
    vec2 probeBaseTexel = vec2(float(coords.x) * numProbeTexels, float(coords.z) * numProbeTexels);

    // 2. Move to the center of the 16x16 probe block
    // (numProbeTexels * 0.5) is 8.0 for a 14+2 probe.
    vec2 uv = probeBaseTexel + (numProbeTexels * 0.5);

    // 3. Offset by the octahedral direction
    // This moves us from the center (8,8) towards the specific texel.
    // We multiply by (Interior / 2), which is 7.0. 
    // This makes the range [8 - 7, 8 + 7] = [1, 15], perfectly hitting interior centers.
    uv += octantCoordinates * (float(numProbeInteriorTexels) * 0.5);
    //uv += octantCoordinates * (float(numProbeInteriorTexels - 1) * 0.5);

    // 4. Normalize by the total atlas dimensions
    float textureWidth = numProbeTexels * float(probeCounts.x);
    float textureHeight = numProbeTexels * float(probeCounts.z);
    
    return vec3(uv / vec2(textureWidth, textureHeight), float(coords.y));
}

// SH Reconstruction 
vec3 ReconstructSH(ProbeColor probe, vec3 n) {
    const float SH_C0 = 0.28209479177;
    const float SH_C1 = 0.4886025119;
    const float SH_C2 = 1.09254843059;
    const float SH_C3 = 0.31539156525;
    const float SH_C4 = 0.54627421529;

    vec3 irradiance =
        probe.sh[0].rgb * SH_C0 +
        probe.sh[1].rgb * (-SH_C1 * n.y) +
        probe.sh[2].rgb * ( SH_C1 * n.z) +
        probe.sh[3].rgb * (-SH_C1 * n.x) +
        probe.sh[4].rgb * ( SH_C2 * n.x * n.y) +
        probe.sh[5].rgb * (-SH_C2 * n.y * n.z) +
        probe.sh[6].rgb * ( SH_C3 * (3.0 * n.z * n.z - 1.0)) +
        probe.sh[7].rgb * (-SH_C2 * n.x * n.z) +
        probe.sh[8].rgb * ( SH_C4 * (n.x * n.x - n.y * n.y));

    return max(vec3(0.0), irradiance);
}







//float3 DDGIGetProbeWorldPosition(int3 probeCoords, DDGIVolumeDescGPU volume) {
vec3 DDGIGetProbeWorldPosition(ivec3 probeCoords, vec3 volumeOrigin, float probeSpacing) {
    // Multiply the grid coordinates by the probe spacing
    // float3 probeGridWorldPosition = probeCoords * volume.probeSpacing;
    vec3 probeGridWorldPosition = probeCoords * probeSpacing;

    // Shift the grid of probes by half of each axis extent to center the volume about its origin
    //float3 probeGridShift = (volume.probeSpacing * (volume.probeCounts - 1)) * 0.5f;

    // Center the probe grid about the origin
    //float3 probeWorldPosition = (probeGridWorldPosition - probeGridShift);
    vec3 probeWorldPosition = (probeGridWorldPosition);// - probeGridShift); not applicable coz our origin is bottom left

    // Rotate the probe grid if infinite scrolling is not enabled
    //if (!IsVolumeMovementScrolling(volume)) probeWorldPosition = RTXGIQuaternionRotate(probeWorldPosition, volume.rotation);
    // Not applicable

    // Translate the grid to the volume's center
    // probeWorldPosition += volume.origin + (volume.probeScrollOffsets * volume.probeSpacing);
    probeWorldPosition += volumeOrigin; // not applicable

    return probeWorldPosition;
}