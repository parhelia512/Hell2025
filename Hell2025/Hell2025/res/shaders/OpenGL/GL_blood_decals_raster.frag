#version 450

#ifndef ENABLE_BINDLESS
    #define ENABLE_BINDLESS 1
#endif

#if ENABLE_BINDLESS == 1
    #extension GL_ARB_bindless_texture : enable
#endif

#include "../common/constants.glsl"
#include "../common/types.glsl"
#include "../common/util.glsl"

layout (location = 0) out vec4 DecalMaskOut;
layout(binding = 0) uniform sampler2D GBufferRMATexture;
layout(binding = 1) uniform sampler2D WorldPositionTexture;
layout(binding = 2) uniform sampler2D GBufferNormalTexture;
layout(binding = 3) uniform sampler2D u_depthTexture;

#if ENABLE_BINDLESS == 1
    readonly restrict layout(std430, binding = 0) buffer textureSamplersBuffer {
        uvec2 textureSamplers[];
    };
#else
    layout(binding = 3) uniform sampler2D DecalTex0;
    layout(binding = 4) uniform sampler2D DecalTex1;
    layout(binding = 5) uniform sampler2D DecalTex2;
    layout(binding = 6) uniform sampler2D DecalTex3;
#endif

flat in int v_InstanceID;

layout(std430, binding = 1)  readonly restrict  buffer rendererDataBuffer    { RendererData rendererData; };
layout(std430, binding = 2)  readonly restrict  buffer viewportDataBuffer    { ViewportData viewportData[]; };
layout(std430, binding = 8)  readonly restrict buffer BloodDecalBuffer   { BloodDecal bloodDecals[]; };

void main() {
    vec2 resolution = textureSize(GBufferNormalTexture, 0);
    vec2 screenCoords = gl_FragCoord.xy / resolution;

    ivec2 pixelCoords = ivec2(gl_FragCoord.xy);
    vec2 screenUV = gl_FragCoord.xy / resolution;

    // Do nothing on walls (assuming Y is up)
    vec3 normal = texture(GBufferNormalTexture, screenUV).rgb;
    if (abs(normal.y) < 0.5) discard;

    // Skip if this tile is masked out
    float blockedOut = texture(GBufferRMATexture, screenUV).a;
    if (blockedOut == 0.0) discard;

    //vec3 worldPos = texture(WorldPositionTexture, screenCoords).rgb;

    // World position (reconstruction from depth)
	uint viewportIndex = ComputeViewportIndexFromSplitscreenMode(pixelCoords, ivec2(resolution), rendererData.splitscreenMode);
	mat4 inverseProjectionView = viewportData[viewportIndex].inverseProjectionView;
	vec2 viewportUV = ScreenUVToViewportUV(screenUV, viewportData[viewportIndex]);
	float depth = texelFetch(u_depthTexture, pixelCoords, 0).r;
	vec4 clip = vec4(viewportUV * 2.0 - 1.0, depth, 1.0);
	vec4 worldH = inverseProjectionView * clip;
	vec3 worldPos = worldH.xyz / max(worldH.w, 1e-6);

    mat4 inverseModelMatrix = bloodDecals[v_InstanceID].inverseModelMatrix;
    vec4 localPos = inverseModelMatrix * vec4(worldPos, 1.0);

    // Explicit clipping
    if (any(greaterThan(abs(localPos.xyz), vec3(0.5)))) {
        discard;
    }

    vec2 decalTexCoord = localPos.xz + 0.5;

    #if ENABLE_BINDLESS == 1
        int textureIndex = bloodDecals[v_InstanceID].textureIndex;
        vec4 textureData  = texture(sampler2D(textureSamplers[textureIndex]), decalTexCoord);
    #else
        vec4 textureData = vec4(0);
        int type = bloodDecals[v_InstanceID].type;
        if (type == 0) { textureData = texture(DecalTex0, decalTexCoord); }
        if (type == 1) { textureData = texture(DecalTex1, decalTexCoord); }
        if (type == 2) { textureData = texture(DecalTex2, decalTexCoord); }
        if (type == 3) { textureData = texture(DecalTex3, decalTexCoord); }
    #endif

    DecalMaskOut = vec4(vec3(textureData.a), 1.0);
}