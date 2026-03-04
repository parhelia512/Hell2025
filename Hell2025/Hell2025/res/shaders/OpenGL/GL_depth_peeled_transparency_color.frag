#version 460
#include "../common/lighting.glsl"
#include "../common/post_processing.glsl"
#include "../common/types.glsl"
#include "../common/util.glsl"

layout (location = 0) out vec4 ColorOut;
layout (location = 1) out float ViewSpaceDepthPreviousOut;

layout (binding = 0) uniform sampler2D baseColorTexture;
layout (binding = 1) uniform sampler2D normalTexture;
layout (binding = 2) uniform sampler2D rmaTexture;

layout (binding = 3) uniform sampler2D plasticBaseColorTexture;
layout (binding = 4) uniform sampler2D plasticNormalTexture;
layout (binding = 5) uniform sampler2D plasticRmaTexture;

layout (binding = 6) uniform sampler2D u_SceneColorTexture;
layout (binding = 7) uniform sampler2D u_SceneDepthTexture;

layout(binding = 4, r32f) uniform image2D u_viewspaceDepth;

in vec2 TexCoord;
in vec3 Normal;
in vec3 Tangent;
in vec3 BiTangent;
in vec4 WorldPos;
in vec3 ViewPos;

uniform mat4 u_view;
uniform mat4 u_model;

readonly restrict layout(std430, binding = 4) buffer lightsBuffer       { Light lights[]; };

float hash12(vec2 p) {
    vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float random(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float InterleavedGradientNoise(vec2 uv) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(uv, magic.xy)));
}

float internalNoise(vec2 uv) {
    return fract(52.9829189 * fract(dot(uv, vec3(0.06711056, 0.00583715, 52.9829189).xy)));
}

void main() {
    // Store this peel layers viewspace depth, so it can be used as the next peel layers previous viewspace depth
    ivec2 px = ivec2(gl_FragCoord.xy);
    float viewspaceDepth = imageLoad(u_viewspaceDepth, px).r;
    ViewSpaceDepthPreviousOut = viewspaceDepth;

    vec3 materialbaseColor = texture2D(baseColorTexture, TexCoord).rgb;
    vec3 materialNormal = texture2D(normalTexture, TexCoord).rgb;
    vec3 materialRma = texture2D(rmaTexture, TexCoord).rgb;

    vec2 plasticUV = TexCoord * 2.0;
    vec3 plasticBaseColor = texture2D(plasticBaseColorTexture, plasticUV).rgb;
    vec3 plasticNormal = texture2D(plasticNormalTexture, plasticUV).rgb;
    vec3 plasticRma = texture2D(plasticRmaTexture, plasticUV).rgb;

    float blendMask = 0.5;
    vec3 baseColor = mix(materialbaseColor, plasticBaseColor, blendMask);
    vec3 rma = mix(materialRma, plasticRma, blendMask);

    vec3 n1 = materialNormal * 2.0 - 1.0;
    vec3 n2 = plasticNormal * 2.0 - 1.0;

    float plasticStrength = 1.5;
    float plasticNormalWeight = 0.5; // closer to 1.0 is more plastic

    n2.xy *= plasticStrength;
    n2.xy *= plasticStrength * plasticNormalWeight;
    n2.z = mix(1.0, n2.z, plasticNormalWeight);

    vec3 blendedNormal;
    blendedNormal.xy = n1.xy + n2.xy;
    blendedNormal.z  = n1.z * n2.z;
    blendedNormal = normalize(blendedNormal);

    mat3 tbn = mat3(normalize(Tangent), normalize(BiTangent), normalize(Normal));
    vec3 normal = normalize(tbn * blendedNormal);

    vec3 gammaBaseColor = pow(baseColor.rgb, vec3(2.2));
    float roughness = rma.r;
    float metallic = rma.g;

    vec3 directLighting = vec3(0.0f);

    // Direct lighting
    for (int i = 0; i < 6; i++) {
        Light light = lights[i];
        vec3 lightPosition = vec3(light.posX, light.posY, light.posZ);
        vec3 lightColor = vec3(light.colorR, light.colorG, light.colorB);
        float lightStrength = light.strength * 3.0;
        float lightRadius = light.radius;

        float shadow = 1.0;
        vec3 L = normalize(lightPosition - WorldPos.xyz);
        vec3 V = normalize(ViewPos - WorldPos.xyz);

        directLighting += GetDirectLightingSpecularOnly(lightPosition, lightColor, lightRadius, lightStrength, normal.xyz, WorldPos.xyz, gammaBaseColor.rgb, roughness, metallic, ViewPos) * shadow;
    }



    // Refraction
    ivec2 texSize = textureSize(u_SceneColorTexture, 0);
    vec2 screenRes = vec2(texSize);

    vec2 screenUV = gl_FragCoord.xy / screenRes;


    mat3 normalMatrix = mat3(u_view * u_model);
    vec3 viewNormal = normalize(normalMatrix * blendedNormal);

    float ior = 1.45; // Index of Refraction for plastic
    float distortionStrength = 0.01;
    float thicknessSpread = 0.0025; // How much the "inner" light scatters
    int samples = 3;
    float aberrationStrength = 0.01;

    vec3 accumulatedRefraction = vec3(0.0);
    //float pixelJitter = random(gl_FragCoord.xy) * 6.28318;
    float pixelJitter = InterleavedGradientNoise(gl_FragCoord.xy) * 6.28318;

    for (int i = 0; i < samples; i++) {
        // Create jitter sample
        float angle = float(i) * (6.28318 / float(samples));
        //float angle = (float(i) * (6.28318 / float(samples))) + pixelJitter;
        vec2 jitter = vec2(cos(angle), sin(angle)) * thicknessSpread;

        // Apply jitter
        vec2 refractionOffset = (viewNormal.xy * distortionStrength) + jitter;
        vec2 refractedUV = (gl_FragCoord.xy / texSize.xy) + refractionOffset;

        // Depth check per sample to ensure you ain't refracting objects in front
        float backgroundDepth = texture2D(u_SceneDepthTexture, refractedUV).r;
        float currentDepth = gl_FragCoord.z;

        vec3 sampleColor;
        if (backgroundDepth < currentDepth) {
            // Fallback to non-distorted screen color if depth fails
            sampleColor = texture2D(u_SceneColorTexture, gl_FragCoord.xy / texSize.xy).rgb;
            //sampleColor = vec3(1,0,0);
        } else {
            sampleColor = texture2D(u_SceneColorTexture, refractedUV).rgb;
        }

        accumulatedRefraction += sampleColor;
    }

    vec3 refractionColor = accumulatedRefraction / float(samples);

    float transparency = 0.75;
    //directLighting += mix(directLighting, refractionColor * gammaBaseColor.rgb, transparency);

    directLighting += (refractionColor * gammaBaseColor.rgb) * transparency;


    //ColorOut.rgb = gammaBaseColor.xyz;
    //ColorOut.rgb *=  ColorOut.rgb;
	ColorOut.a = 1.0;

    ColorOut.rgb = directLighting;
    //ColorOut.rgb = baseColor.rgb;
    //ColorOut.rgb = rma.rgb;

   // ColorOut.rgb = normal.rgb;
    //ColorOut.rgb = normalMap.rgb;

}
