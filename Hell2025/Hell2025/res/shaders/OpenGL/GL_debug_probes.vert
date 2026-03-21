#version 420 core

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

    int x = v_probeIndex % u_probeCountX;
    int y = (v_probeIndex / u_probeCountX) % u_probeCountY;
    int z = v_probeIndex / (u_probeCountX * u_probeCountY);

    v_voxelCoord = ivec3(x, y, z);

    vec3 pos = vec3(x, y, z) * u_spacing + u_offset;

    mat4 translation = mat4(1.0);
    translation[3] = vec4(pos, 1.0);

    mat4 scaleMat = mat4(1.0);
    scaleMat[0][0] = 0.0625;
    scaleMat[1][1] = 0.0625;
    scaleMat[2][2] = 0.0625;

    mat4 model = translation * scaleMat;

    v_worldPos = (model * vec4(a_position, 1.0)).xyz;
    v_normal = a_normal;

    gl_Position = u_projectionView * model * vec4(a_position, 1.0);

}
