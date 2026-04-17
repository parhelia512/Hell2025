#version 460

//#extension GL_ARB_bindless_texture : enable        
//readonly restrict layout(std430, binding = 0) buffer textureSamplersBuffer { uvec2 textureSamplers[]; };    

layout (location = 0) out float ViewspaceDepthOut;
layout (binding = 0, r32f) uniform image2D u_viewspaceDepthPrevious;
layout (binding = 1) uniform sampler2D u_gBufferDepth;

in flat int BaseColorTextureIndex;
in vec2 v_uv;

void main() {
    ivec2 px = ivec2(gl_FragCoord.xy);
    
    //vec4 baseColor = texture(sampler2D(textureSamplers[BaseColorTextureIndex]), v_uv);
    //if (baseColor.a < 0.15) discard;

    float sceneDepth = texelFetch(u_gBufferDepth, px, 0).r;
    if (gl_FragCoord.z >= sceneDepth) {
        discard;
    }

    float currentDepth = gl_FragCoord.z;
    float previousDepth = imageLoad(u_viewspaceDepthPrevious, px).r;

    if (currentDepth <= previousDepth) {
        discard;
    }
}