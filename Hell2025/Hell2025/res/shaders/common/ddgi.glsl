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
    int probesPerPlane = DDGIGetProbesPerPlane(probeCounts);
    int planeIndex = DDGIGetPlaneIndex(probeCoords);
    int probeIndexInPlane = DDGIGetProbeIndexInPlane(probeCoords, probeCounts);
    return (planeIndex * probesPerPlane) + probeIndexInPlane;
}



uvec3 DDGIGetProbeTexelCoords(int probeIndex, DDGIVolume volume) {
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);
    int planeIndex = probeIndex / probesPerPlane;

    int x = probeIndex % volume.probeCounts.x;
    int y = (probeIndex / volume.probeCounts.x) % volume.probeCounts.z;

    return uvec3(x, y, planeIndex);
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









// Computes the 3D grid-space coordinates for the probe at the given probe index in the range [0, numProbes-1]
ivec3 DDGIGetProbeCoords(int probeIndex, DDGIVolume volume) {
    ivec3 probeCoords;
    probeCoords.x = probeIndex % volume.probeCounts.x;
    probeCoords.z = (probeIndex / volume.probeCounts.x) % volume.probeCounts.z;
    probeCoords.y = probeIndex / (volume.probeCounts.x * volume.probeCounts.z);
    return probeCoords;
}

vec3 DDGIGetProbeBaseWorldPosition(ivec3 probeCoords, DDGIVolume volume) {
    vec3 counts = vec3(volume.probeCounts);
    vec3 coords = vec3(probeCoords);
    return volume.origin + (coords - (counts - 1.0) * 0.5) * volume.probeSpacing;
}

vec3 DDGIGetProbeWorldPosition(ivec3 probeCoords, DDGIVolume volume, ProbeState probeState) {
    vec3 probeWorldPosition = DDGIGetProbeBaseWorldPosition(probeCoords, volume);
    
    // Scale the normalized offset by the probe spacing
    probeWorldPosition += probeState.relocationOffset * volume.probeSpacing;

    return probeWorldPosition;
}

vec3 DDGIGetSurfaceBias(vec3 fragWorldPos, vec3 fragNormal, vec3 viewPos) {
    vec3 cameraDirection = normalize(fragWorldPos - viewPos);
    return (fragNormal * PROBE_NORMAL_BIAS) + (-cameraDirection * PROBE_VIEW_BIAS);
}

void GetSHBasis(vec3 n, out float sh[9]) {
    const float SH_C0 = 0.28209479177;
    const float SH_C1 = 0.4886025119;
    const float SH_C2 = 1.09254843059;
    const float SH_C3 = 0.31539156525;
    const float SH_C4 = 0.54627421529;

    sh[0] =  SH_C0;
    sh[1] = -SH_C1 * n.y; // User specific sign
    sh[2] =  SH_C1 * n.z;
    sh[3] = -SH_C1 * n.x; // User specific sign
    sh[4] =  SH_C2 * n.x * n.y;
    sh[5] = -SH_C2 * n.y * n.z; // User specific sign
    sh[6] =  SH_C3 * (3.0 * n.z * n.z - 1.0);
    sh[7] = -SH_C2 * n.x * n.z; // User specific sign
    sh[8] =  SH_C4 * (n.x * n.x - n.y * n.y);
}











// NEW
ivec3 DDGIGetBaseProbeGridCoords(vec3 worldPosition, DDGIVolume volume) {
    //vec3 position = worldPosition - (volume.origin + (vec3(volume.probeScrollOffsets) * volume.probeSpacing));
    vec3 position = worldPosition - (volume.origin + (vec3(0.0) * volume.probeSpacing));

    //if (!IsVolumeMovementScrolling(volume)) {
    //    position = RTXGIQuaternionRotate(position, RTXGIQuaternionConjugate(volume.rotation));
    //}

    position += (volume.probeSpacing * (vec3(volume.probeCounts) - vec3(1.0))) * 0.5;

    ivec3 probeCoords = ivec3(position / volume.probeSpacing);

    probeCoords = clamp(probeCoords, ivec3(0), volume.probeCounts - ivec3(1));

    return probeCoords;
}



vec3 DDGIGetProbeUV(int probeIndex, vec2 octantCoordinates, int numProbeInteriorTexels, DDGIVolume volume) {
    uvec3 coords = DDGIGetProbeTexelCoords(probeIndex, volume);

    float numProbeTexels = float(numProbeInteriorTexels) + 2.0;
    float textureWidth = numProbeTexels * float(volume.probeCounts.x);
    float textureHeight = numProbeTexels * float(volume.probeCounts.z);

    vec2 uv = vec2(coords.xy) * numProbeTexels + (numProbeTexels * 0.5);
    uv += octantCoordinates * (float(numProbeInteriorTexels) * 0.5); // NVIDIA version
    // uv += octantCoordinates * (float(numProbeInteriorTexels - 1) * 0.5);
    
    uv /= vec2(textureWidth, textureHeight);

    return vec3(uv, float(coords.z));
}