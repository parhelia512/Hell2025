#version 450
#include "../common/types.glsl"

layout(location = 0) in vec3 inPosition;

uniform mat4 u_projectionView;

layout(std430, binding = 8) readonly restrict buffer BloodDecalBuffer   { BloodDecal bloodDecals[]; };

flat out int v_InstanceID;

void main() {
    mat4 modelMatrix = bloodDecals[gl_InstanceID].modelMatrix;
    v_InstanceID = gl_InstanceID;

	gl_Position = u_projectionView * modelMatrix * vec4(inPosition, 1.0);;
}