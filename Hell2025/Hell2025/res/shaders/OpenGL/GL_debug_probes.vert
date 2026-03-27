#version 450
#include "../common/ddgi.glsl"
#include "../common/types.glsl"

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;

layout(std430, binding = 6) readonly buffer ProbeColorBuffer       { ProbeColor probeColors[]; };
layout(std430, binding = 7) readonly buffer DDGIVolumeBuffer       { DDGIVolume volume; };
layout(std430, binding = 8) readonly buffer ProbeVisibleListBuffer { uint probeVisibility[]; };

uniform mat4 u_projectionView;
flat out int v_probeIndex; 
flat out ivec3 v_voxelCoord;
out vec3 v_worldPos;
out vec3 v_normal;

void main() {
    v_probeIndex = gl_InstanceID;

    // Use the unified NVIDIA helper to get the 3D grid coord
    ivec3 probeCoords = DDGIGetProbeCoords(v_probeIndex, volume.probeCounts);
    vec3 probePos = DDGIGetProbeWorldPosition(probeCoords, volume.origin, volume.probeSpacing);

    v_voxelCoord = probeCoords;

    // Standard scale and translation
    mat4 model = mat4(1.0);
    model[0][0] = 0.0625;
    model[1][1] = 0.0625;
    model[2][2] = 0.0625;
    model[3] = vec4(probePos, 1.0);

    v_worldPos = (model * vec4(a_position, 1.0)).xyz;
    v_normal = a_normal;

    gl_Position = u_projectionView * model * vec4(a_position, 1.0);
}