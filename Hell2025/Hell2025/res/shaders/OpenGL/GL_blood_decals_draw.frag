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

layout(std430, binding = 1)  readonly restrict  buffer rendererDataBuffer    { RendererData rendererData; };
layout(std430, binding = 2)  readonly restrict  buffer viewportDataBuffer    { ViewportData viewportData[]; };
layout(std430, binding = 7)  restrict           buffer tileBloodDecalsBuffer { TileBloodDecals tileBloodDecals[]; };
layout(std430, binding = 8)  readonly restrict  buffer BloodDecalBuffer      { BloodDecal bloodDecals[]; };
layout(std430, binding = 9)  restrict           buffer DecalIndexPool        { uint globalBloodDecalIndices[]; };

uniform int u_decalCount; // old only
uniform int u_tileXCount;
uniform int u_tileYCount;

void main() {
    ivec2 pixelCoords = ivec2(gl_FragCoord.xy);
    vec2 resolution = vec2(rendererData.gBufferWidth, rendererData.gBufferHeight);
    vec2 screenUV = (vec2(pixelCoords) + 0.5) / resolution;

    uvec2 tileCoords = uvec2(gl_FragCoord.xy) / TILE_SIZE; 
    uint tileIndex = tileCoords.y * u_tileXCount + tileCoords.x;

    uint count = tileBloodDecals[tileIndex].decalCount;
    uint offset = tileBloodDecals[tileIndex].decalOffset;

    // Skip if this tile has no decals
    if (count == 0) discard;

    // Do nothing on walls (assuming Y is up)
    vec3 normal = texture(GBufferNormalTexture, screenUV).rgb;
    if (abs(normal.y) < 0.5) discard;

    // Skip if this tile is masked out
    float blockedOut = texture(GBufferRMATexture, screenUV).a;
    if (blockedOut == 0.0) discard;

    // World position from depth... is acutally slower for some reason
	// uint viewportIndex = ComputeViewportIndexFromSplitscreenMode(pixelCoords, ivec2(resolution), rendererData.splitscreenMode);
	// mat4 inverseProjectionView = viewportData[viewportIndex].inverseProjectionView;
	// vec2 viewportUV = ScreenUVToViewportUV(screenUV, viewportData[viewportIndex]);
	// float depth = texelFetch(u_depthTexture, pixelCoords, 0).r;
	// vec4 clip = vec4(viewportUV * 2.0 - 1.0, depth, 1.0);
	// vec4 worldH = inverseProjectionView * clip;
	// vec3 worldPos = worldH.xyz / max(worldH.w, 1e-6);

    vec3 worldPos = texture(WorldPositionTexture, screenUV).rgb;
    
    float bestMask = 0.0;

    for (uint i = 0; i < count; ++i) {
        uint decalIdx = globalBloodDecalIndices[offset + i];
        
        vec4 localPos = bloodDecals[decalIdx].inverseModelMatrix * vec4(worldPos, 1.0);

        // Explicit clipping
        if (any(greaterThan(abs(localPos.xyz), vec3(0.5, 0.5, 0.5)))) {
            continue; 
        }

        int textureIndex = bloodDecals[decalIdx].textureIndex;
        vec2 texCoords = localPos.xz + 0.5;

        #if ENABLE_BINDLESS == 1
            float a = texture(sampler2D(textureSamplers[textureIndex]), texCoords).a;
            bestMask = max(bestMask, a);
        #endif

        if (bestMask >= 1.0) break;
    }

    DecalMaskOut = vec4(vec3(bestMask), 1.0);
}












void main2() {
    ivec2 pixelCoords = ivec2(gl_FragCoord.xy);
    vec2 resolution = vec2(rendererData.gBufferWidth, rendererData.gBufferHeight);
    vec2 screenUV = (vec2(pixelCoords) + 0.5) / resolution;

    uvec2 tileCoords = uvec2(gl_FragCoord.xy) / TILE_SIZE; 
    uint tileIndex = tileCoords.y * u_tileXCount + tileCoords.x;

    uint count = tileBloodDecals[tileIndex].decalCount;
    uint offset = tileBloodDecals[tileIndex].decalOffset;

    // Skip if this tile has no decals
    if (count == 0) discard;

    // Do nothing on walls (assuming Y is up)
    vec3 normal = texture(GBufferNormalTexture, screenUV).rgb;
    if (abs(normal.y) < 0.5) discard;

    // Skip if this tile is masked out
    float maskedOut = texture(GBufferRMATexture, screenUV).a;
    if (maskedOut == 0.0) discard;

    // World position (reconstruction from depth)
	//uint viewportIndex = ComputeViewportIndexFromSplitscreenMode(pixelCoords, ivec2(resolution), rendererData.splitscreenMode);
	//mat4 inverseProjectionView = viewportData[viewportIndex].inverseProjectionView;
	//vec2 viewportUV = ScreenUVToViewportUV(screenUV, viewportData[viewportIndex]);
	//float depth = texelFetch(u_depthTexture, pixelCoords, 0).r;
	//vec4 clip = vec4(viewportUV * 2.0 - 1.0, depth, 1.0);
	//vec4 worldH = inverseProjectionView * clip;
	//vec3 worldPos = worldH.xyz / max(worldH.w, 1e-6);

    vec3 worldPos = texture(WorldPositionTexture, screenUV).rgb;

    float bestMask = 0.0;

   
    for (uint i = 0; i < count; ++i) {
        uint decalIdx = globalBloodDecalIndices[offset + i];
        mat4 invMat = bloodDecals[decalIdx].inverseModelMatrix;

        // Correct Row Extraction
        // We need the rows because: (Matrix * Vector)_i = dot(Row_i, Vector)
        vec4 rowX = vec4(invMat[0].x, invMat[1].x, invMat[2].x, invMat[3].x);
        vec4 rowY = vec4(invMat[0].y, invMat[1].y, invMat[2].y, invMat[3].y);
        vec4 rowZ = vec4(invMat[0].z, invMat[1].z, invMat[2].z, invMat[3].z);

        vec4 vPos = vec4(worldPos, 1.0);

        // Calculate Local Y (Thickness) first for early exit
        float localY = dot(rowY, vPos);
        if (abs(localY) > 0.5) continue;

        // Calculate Local X and Z
        float localX = dot(rowX, vPos);
        if (abs(localX) > 0.5) continue;

        float localZ = dot(rowZ, vPos);
        if (abs(localZ) > 0.5) continue;

        // Texture Fetch
        int texIdx = bloodDecals[decalIdx].textureIndex;
        vec2 texCoords = vec2(localX, localZ) + 0.5;

        #if ENABLE_BINDLESS == 1
            float a = texture(sampler2D(textureSamplers[texIdx]), texCoords).a;
            bestMask = max(bestMask, a);
        #endif

        if (bestMask >= 1.00) break;
    }

    DecalMaskOut = vec4(vec3(bestMask), 1.0);
}
