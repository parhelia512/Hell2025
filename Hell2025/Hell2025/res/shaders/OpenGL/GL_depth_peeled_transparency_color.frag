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

layout(binding = 4, r32f) uniform image2D u_viewspaceDepth;

in vec2 TexCoord;
in vec3 Normal;
in vec3 Tangent;
in vec3 BiTangent;
in vec4 WorldPos;
in vec3 ViewPos;

readonly restrict layout(std430, binding = 4) buffer lightsBuffer       { Light lights[]; };

void main() {
    // Store this peel layers viewspace depth, so it can be used as the next peel layers previous viewspace depth
    ivec2 px = ivec2(gl_FragCoord.xy);
    float viewspaceDepth = imageLoad(u_viewspaceDepth, px).r;
    ViewSpaceDepthPreviousOut = viewspaceDepth;



    // Replace this with full PBR for all light sources when u have your shit together

    vec4 baseColor = texture2D(baseColorTexture, TexCoord);
    vec3 normalMap = texture2D(normalTexture, TexCoord).rgb;
    vec3 rma = texture2D(rmaTexture, TexCoord).rgb;

    normalMap = mix(normalMap, vec3(0.5, 0.5, 1), 0.7);

    mat3 tbn = mat3(normalize(Tangent), normalize(BiTangent), normalize(Normal));
    normalMap.rgb = normalMap.rgb * 2.0 - 1.0;
    normalMap = normalize(normalMap);

    vec3 normal = normalize(tbn * (normalMap));

    vec3 gammaBaseColor = pow(baseColor.rgb, vec3(2.2));
    float roughness = rma.r;
    float metallic = rma.g;


	//if (!gl_FrontFacing) {
    //    normal = -normal;
    //    discard;
    //}

    vec3 directLighting = vec3(0.0f);

     for (int i = 0; i < 4; i++) {

        Light light = lights[i];
        vec3 lightPosition = vec3(light.posX, light.posY, light.posZ);
        vec3 lightColor =  vec3(light.colorR, light.colorG, light.colorB);
        float lightStrength = light.strength * 10;
        float lightRadius = light.radius;

        float shadow = 1.0f;
        directLighting += GetDirectLightingSpecularOnly(lightPosition, lightColor, lightRadius, lightStrength, normal.xyz, WorldPos.xyz, gammaBaseColor.rgb, roughness, metallic, ViewPos) * shadow;
        directLighting += GetDirectLightingSpecularOnly(lightPosition, lightColor, lightRadius, lightStrength, -normal.xyz, WorldPos.xyz, gammaBaseColor.rgb, roughness, metallic, ViewPos) * shadow;
        //directLighting += vec3(0.01);
    }



    ColorOut.rgb = gammaBaseColor.xyz;
    ColorOut.rgb *=  ColorOut.rgb;
	ColorOut.a = 1.0;

    ColorOut.rgb = directLighting;

}
