#version 460
layout (location = 0) in vec3 vPosition;
out vec3 v_worldPos;

uniform mat4 u_shadowMatrices[6];
uniform int u_faceIndex;

void main() {
	vec4 worldPos = vec4(vPosition, 1.0);

    v_worldPos = worldPos.xyz;
    gl_Position = u_shadowMatrices[u_faceIndex] * worldPos;
}