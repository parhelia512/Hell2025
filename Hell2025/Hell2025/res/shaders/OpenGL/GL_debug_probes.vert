#version 450
#include "../common/ddgi.glsl"

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;

uniform mat4 u_projectionView;
uniform float u_spacing;
uniform vec3 u_offset;
uniform int u_probeCountX;
uniform int u_probeCountY;
uniform int u_probeCountZ;

flat out int v_probeIndex; 
flat out ivec3 v_voxelCoord;
out vec3 v_worldPos;
out vec3 v_normal;

void main() {
    v_probeIndex = gl_InstanceID;

    // Use the unified NVIDIA helper to get the 3D grid coord
    ivec3 probeCounts = ivec3(u_probeCountX, u_probeCountY, u_probeCountZ);
    ivec3 coords = DDGIGetProbeCoords(v_probeIndex, probeCounts);

    v_voxelCoord = coords;

    // Calculate world position based on the corrected coordinates
    vec3 pos = vec3(coords) * u_spacing + u_offset;

    // Standard scale and translation
    mat4 model = mat4(1.0);
    model[0][0] = 0.0625;
    model[1][1] = 0.0625;
    model[2][2] = 0.0625;
    model[3] = vec4(pos, 1.0);

    v_worldPos = (model * vec4(a_position, 1.0)).xyz;
    v_normal = a_normal;

    gl_Position = u_projectionView * model * vec4(a_position, 1.0);
}