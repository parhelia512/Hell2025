#version 460

layout (location = 0) out float ViewspaceDepthOut;
layout(binding = 0, r32f) uniform image2D u_viewspaceDepthPrevious;

void main() {
    ivec2 px = ivec2(gl_FragCoord.xy);

    float currentDepth = gl_FragCoord.z;
    float previousDepth = imageLoad(u_viewspaceDepthPrevious, px).r;

    if (currentDepth <= previousDepth) {
        discard;
    }
}