#version 460 core

#ifndef ENABLE_BINDLESS
    #define ENABLE_BINDLESS 1
#endif

#if ENABLE_BINDLESS == 1
    #extension GL_ARB_bindless_texture : enable        
    readonly restrict layout(std430, binding = 0) buffer textureSamplersBuffer {
	    uvec2 textureSamplers[];
    };    
    in flat int BaseColorTextureIndex;
    in flat int NormalTextureIndex;
    in flat int RMATextureIndex;
    in flat int WoundBaseColorTextureIndex;
    in flat int WoundNormalTextureIndex;
    in flat int WoundRMATextureIndex;

#else
    layout (binding = 0) uniform sampler2D baseColorTexture;
    layout (binding = 1) uniform sampler2D normalTexture;
    layout (binding = 2) uniform sampler2D rmaTexture;
    layout (binding = 3) uniform sampler2D emissiveTexture;
    layout (binding = 4) uniform sampler2D woundBaseColorTexture;
    layout (binding = 5) uniform sampler2D woundNormalTexture;
    layout (binding = 6) uniform sampler2D woundRmaTexture;
#endif

layout (binding = 7) uniform sampler2DArray woundMaskTextureArray;

#include "../common/lighting.glsl"
#include "../common/post_processing.glsl"

layout (location = 0) out vec4 BaseColorOut;
layout (location = 1) out vec4 NormalOut;
layout (location = 2) out vec4 RMAOut;
layout (location = 3) out vec4 WorldPositionOut;
layout (location = 4) out vec4 EmissiveOut;

in vec2 TexCoord;
in vec3 Normal;
in vec3 Tangent;
in vec3 BiTangent;
in vec4 WorldPos;
in vec3 ViewPos;
in vec3 EmissiveColor;

in flat int WoundMaskTextureIndex;
in flat int BlockScreenSpaceBloodDecalsFlag;
in flat int EmissiveTextureIndex; 

uniform bool u_alphaDiscard;
uniform bool u_flipNormalMapY;

void main() {
    vec3 emissiveColor = EmissiveColor;

#if ENABLE_BINDLESS == 1
    vec4 baseColor = texture(sampler2D(textureSamplers[BaseColorTextureIndex]), TexCoord);
    vec3 normalMap = texture(sampler2D(textureSamplers[NormalTextureIndex]), TexCoord).rgb;   
    vec3 rma = texture(sampler2D(textureSamplers[RMATextureIndex]), TexCoord).rgb;
    vec3 emissiveMapColor = texture(sampler2D(textureSamplers[EmissiveTextureIndex]), TexCoord).rgb;
#else
    vec4 baseColor = texture(baseColorTexture, TexCoord);
    vec3 normalMap = texture(normalTexture, TexCoord).rgb;
    vec3 rma = texture(rmaTexture, TexCoord).rgb;
    vec3 emissiveMapColor = texture(emissiveTexture, TexCoord).rgb;
#endif

    // Emissive
    if (EmissiveTextureIndex != -1) {
        emissiveColor *= emissiveMapColor;
    }
    EmissiveOut = vec4(emissiveColor, 0);

    if (u_alphaDiscard) {
        if (baseColor.a < 0.5) {
            discard;
        }    
    }
    
    

    mat3 tbn = mat3(normalize(Tangent), normalize(BiTangent), normalize(Normal));
    normalMap.rgb = normalMap.rgb * 2.0 - 1.0;
    normalMap = normalize(normalMap);
    
    if (u_flipNormalMapY) {
        normalMap.y *= -1;
    }

    vec3 normal = normalize(tbn * (normalMap));

    BaseColorOut = vec4(baseColor);
    NormalOut = vec4(normal, 1.0);   

    RMAOut.rgb = rma;
    RMAOut.a = BlockScreenSpaceBloodDecalsFlag;

    WorldPositionOut = vec4(WorldPos.rgb, 1.0);
}
