#include "../common/pbr_functions.glsl"

vec3 GetDirectLighting(vec3 lightPos, vec3 lightColor, float radius, float strength, vec3 Normal, vec3 WorldPos, vec3 baseColor, float roughness, float metallic, vec3 viewPos) {
    vec3 toLight = lightPos - WorldPos;
    float dist = length(toLight);
    vec3 lightDir = toLight / dist;
    vec3 viewDir = normalize(viewPos - WorldPos);
    float att = smoothstep(radius, 0.0, dist) * strength;
    float ndl = max(dot(Normal, lightDir), 0.0) * att;
    vec3 brdf = microfacetBRDF(lightDir, viewDir, Normal, baseColor, metallic, 1.0, roughness);
    return brdf * ndl * clamp(lightColor, 0.0, 1.0);
}

vec3 GetDirectLightingSpecularOnlyOLD(vec3 lightPos, vec3 lightColor, float radius, float strength, vec3 Normal, vec3 WorldPos, vec3 baseColor, float roughness, float metallic, vec3 viewPos) {
    vec3 toLight = lightPos - WorldPos;
    float dist = length(toLight);
    vec3 lightDir = toLight / dist;
    vec3 viewDir = normalize(viewPos - WorldPos);
    float att = smoothstep(radius, 0.0, dist) * strength;
    float ndl = max(dot(Normal, lightDir), 0.0) * att;
    vec3 brdf = microfacetBRDFSpecularOnly(lightDir, viewDir, Normal, baseColor, metallic, 1.0, roughness);
    return brdf * ndl * clamp(lightColor, 0.0, 1.0);
}

vec3 GetDirectLightingSpecularOnly(vec3 lightPos, vec3 lightColor, float radius, float strength, vec3 Normal, vec3 WorldPos, vec3 baseColor, float roughness, float metallic, vec3 viewPos) {
    vec3 toLight = lightPos - WorldPos;
    float dist = length(toLight);
    vec3 lightDir = toLight / dist;
    vec3 viewDir = normalize(viewPos - WorldPos);

    // falloff and light intensity
    float att = smoothstep(radius, 0.0, dist) * strength;
    float ndl = max(dot(Normal, lightDir), 0.0) * att;

    // calculate surface reflection only
    vec3 brdf = microfacetBRDFSpecularOnly(lightDir, viewDir, Normal, baseColor, metallic, 1.0, roughness);

    // return the specular highlight for this light
    return brdf * ndl * clamp(lightColor, 0.0, 1.0);
}

vec3 GetDirectionalLighting(vec3 lightDir, vec3 lightColor, float strength, vec3 Normal, vec3 WorldPos, vec3 baseColor, float roughness, float metallic, vec3 viewPos) {
    vec3 viewDir = normalize(viewPos - WorldPos);
    float ndl = max(dot(Normal, lightDir), 0.0) * strength;
    vec3 brdf = microfacetBRDF(lightDir, viewDir, Normal, baseColor, metallic, 1.0, roughness);
    return brdf * ndl * clamp(lightColor, 0.0, 1.0);
}


vec3 GetSpotlightLighting(vec3 lightPos, vec3 lightDir, vec3 lightColor, float radius, float strength, float innerAngle, float outerAngle, vec3 Normal, vec3 WorldPos, vec3 baseColor, float roughness, float metallic, vec3 viewPos, mat4 LightViewProj) {
    vec3 d = lightPos - WorldPos;
    float dist = length(d);
    vec3 toLight = d / dist;
    vec3 viewDir = normalize(viewPos - WorldPos);

    // distance fall-off + strength
    float lightAttenuation = smoothstep(radius, 0.0, dist) * strength;

    // cone fall-off
    float spotFactor = smoothstep(outerAngle, innerAngle, dot(toLight, -lightDir));

    // extra smooth fade by distance²
    float distanceFactor = clamp(1.0 - dist / radius, 0.0, 1.0);
    spotFactor *= distanceFactor * distanceFactor;

    // lambert
    float irradiance = max(dot(toLight, Normal), 0.0) * lightAttenuation * spotFactor;

    vec3 brdf = microfacetBRDF(toLight, viewDir, Normal, baseColor, metallic, 1.0, roughness);
    return brdf * irradiance * clamp(lightColor, 0.0, 1.0);
}

float SpotlightShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, vec3 fragWorldPos, vec3 lightPos, vec3 viewPos, sampler2DArray shadowMapArray, int layerIndex) {
    // Project and bias
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    // Fold slope bias and constant bias into one
    float dist = length(lightPos - fragWorldPos);
    float bias = 0.0001 + 0.028/(dist + 0.001);

    // Precompute texel size
    ivec2 size = textureSize(shadowMapArray, 0).xy;
    vec2 texelSize = 1.0/vec2(size);

    // PCF over 5×5 kernel
    float shadow = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float d = texture(shadowMapArray, vec3(projCoords.xy + vec2(x, y)*texelSize, layerIndex)).r;
            shadow += (currentDepth - bias > d) ? 1.0 : 0.0;
        }
    }

    // Average via multiply
    return shadow * (1.0 / 25.0);
}

float SpotlightShadowCalculationFast(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, vec3 fragWorldPos, vec3 lightPos, vec3 viewPos, sampler2DArray shadowMapArray, int layerIndex) {
    // Project and bias
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    // Fold slope bias and constant bias into one
    float dist = length(lightPos - fragWorldPos);
    float bias = 0.0001 + 0.028/(dist + 0.001);

    // Precompute texel size
    ivec2 size = textureSize(shadowMapArray, 0).xy;
    vec2 texelSize = 1.0/vec2(size);

    // Single sample (no PCF)
    float d = texture(shadowMapArray, vec3(projCoords.xy, layerIndex)).r;

    // Apply bias and check shadow
    float shadow = (currentDepth - bias > d) ? 1.0 : 0.0;

    // Return the final shadow factor
    return shadow;
}

vec3 ApplyCookie(mat4 LightViewProj, vec3 worldPos, vec3 lightPos, vec3 lightColor, float maxDistance, sampler2D cookieTexture) {
   vec4 lightSpacePos = LightViewProj * vec4(worldPos, 1.0);
    vec2 cookieUV = lightSpacePos.xy / lightSpacePos.w * 0.5 + 0.5;

    // Centered scale
    float cookieScale = 1.5;
    vec2 scaledUV = (cookieUV - 0.5) * cookieScale + 0.5;

    // Edge fade based on scaledUV
    vec2 clampedUV = clamp(scaledUV, vec2(0.0), vec2(1.0));
    float fadeFactor = clamp(1.0 - length(scaledUV - clampedUV) * 15.0, 0.0, 1.0);

    float dist = length(worldPos - lightPos);
    float distanceFactor = clamp(1.0 - dist / maxDistance, 0.0, 1.0);
    distanceFactor *= distanceFactor;

    float cookieFactor = texture(cookieTexture, clampedUV).r;
    return lightColor * cookieFactor * fadeFactor * distanceFactor;
}


vec3 gridSamplingDisk[20] = vec3[](
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1),
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

float ShadowCalculation(int lightIndex, vec3 lightPos, float lightRadius, vec3 fragPos, vec3 viewPos, vec3 Normal, samplerCubeArray shadowCubeMapArray) {
    vec3 lightDir = fragPos - lightPos;
    float currentDepth = length(lightDir);
    float far_plane = lightRadius;
    float shadow = 0.0;
    float bias = max(0.0125 * (1.0 - dot(Normal, normalize(lightDir))), 0.00125);  // Added normalize to lightDir
    int samples = 20;
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 200.0;
    // Sample the cubemap array for shadows
    for (int i = 0; i < samples; ++i) {
        // Sample the cubemap array with the light index and the current sampling offset
        float closestDepth = texture(shadowCubeMapArray, vec4(lightDir + gridSamplingDisk[i] * diskRadius, lightIndex)).r;
        closestDepth *= far_plane;  // Undo mapping [0;1]
        // Apply bias and check if the fragment is in shadow
        if (currentDepth - bias > closestDepth) {
            shadow += 1.0;
        }
    }
    // Average the shadow results
    shadow /= float(samples);
    // Return the final shadow factor (1 means fully lit, 0 means fully in shadow)
    return 1.0 - shadow;
}

float ShadowCalculationFast(int lightIndex, vec3 lightPos, float lightRadius, vec3 fragPos, vec3 viewPos, vec3 Normal, samplerCubeArray shadowCubeMapArray) {
    vec3 lightDir = fragPos - lightPos;
    float currentDepth = length(lightDir);
    float far_plane = lightRadius;
    float shadow = 0.0;
    float bias = max(0.0125 * (1.0 - dot(Normal, normalize(lightDir))), 0.00125);  // Added normalize to lightDir
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 200.0;

    // Sample the cubemap array for shadows (single sample)
    float closestDepth = texture(shadowCubeMapArray, vec4(lightDir + gridSamplingDisk[0] * diskRadius, lightIndex)).r;
    closestDepth *= far_plane;  // Undo mapping [0;1]

    // Apply bias and check if the fragment is in shadow
    if (currentDepth - bias > closestDepth) {
        shadow = 1.0;
    }

    // Return the final shadow factor (1 means fully lit, 0 means fully in shadow)
    return 1.0 - shadow;
}

//vec3 GetDirectionalLighting(vec3 WorldPos, vec3 Normal, vec3 baseColor, float roughness, float metallic, vec3 viewPos, vec3 lightDir, vec3 lightColor, float strength, float fresnelReflect) {
//	vec3 viewDir = normalize(viewPos - WorldPos);
//	float irradiance = max(dot(lightDir, Normal), 0.0) * strength;
//	vec3 brdf = microfacetBRDF(lightDir, viewDir, Normal, baseColor, metallic, fresnelReflect, roughness);
//    return brdf * irradiance * clamp(lightColor, 0, 1);
//}














// These are duplicates of what is also in the pbr common shader, only the ocean frag shader uses these so maybe organise better bro
// These are duplicates of what is also in the pbr common shader, only the ocean frag shader uses these so maybe organise better bro
// These are duplicates of what is also in the pbr common shader, only the ocean frag shader uses these so maybe organise better bro
// These are duplicates of what is also in the pbr common shader, only the ocean frag shader uses these so maybe organise better bro
// These are duplicates of what is also in the pbr common shader, only the ocean frag shader uses these so maybe organise better bro
// These are duplicates of what is also in the pbr common shader, only the ocean frag shader uses these so maybe organise better bro
// These are duplicates of what is also in the pbr common shader, only the ocean frag shader uses these so maybe organise better bro
// These are duplicates of what is also in the pbr common shader, only the ocean frag shader uses these so maybe organise better bro

float D_GGX(float NoH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NoH2 = NoH * NoH;
    float b = (NoH2 * (alpha2 - 1.0) + 1.0);
    return alpha2 / (PI * b * b);
}

float G1_GGX_Schlick(float NdotV, float roughness) {
  //float r = roughness; // original
  float r = 0.5 + 0.5 * roughness; // Disney remapping
  float k = (r * r) / 2.0;
  float denom = NdotV * (1.0 - k) + k;
  return NdotV / denom;
}

float G_Smith(float NoV, float NoL, float roughness) {
  float g1_l = G1_GGX_Schlick(NoL, roughness);
  float g1_v = G1_GGX_Schlick(NoV, roughness);
  return g1_l * g1_v;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 microfacetBRDF(in vec3 L, in vec3 V, in vec3 N, in vec3 baseColor, in float metallicness, in float fresnelReflect, in float roughness, in vec3 WorldPos) {
  // Half vector
  vec3 H = normalize(V + L);

  // Dot products
  float NoV = clamp(dot(N, V), 0.0, 1.0);
  float NoL = clamp(dot(N, L), 0.0, 1.0);
  float NoH = clamp(dot(N, H), 0.0, 1.0);
  float VoH = clamp(dot(V, H), 0.0, 1.0);

  // Base reflectance (F0)
  vec3 f0 = vec3(0.16 * (fresnelReflect * fresnelReflect));
  f0 = mix(f0, baseColor, metallicness);

  // Fresnel term
  vec3 F = fresnelSchlick(VoH, f0);

  // Specular microfacet BRDF
  float D = D_GGX(NoH, roughness);
  float G = G_Smith(NoV, NoL, roughness);
  vec3 specular = (D * G * F) / max(4.0 * NoV * NoL, 0.001);

  // Energy-conserving diffuse
  vec3 notSpecular = (1.0 - F) * (1.0 - metallicness);
  vec3 diffuse = notSpecular * baseColor / PI;

  return diffuse + specular;
}

vec3 microfacetSpecular(in vec3 L, in vec3 V, in vec3 N, in vec3 F0, in float roughness) {
    vec3 H = normalize(L + V);

    float NoV = clamp(dot(N, V), 0.0, 1.0);
    float NoL = clamp(dot(N, L), 0.0, 1.0);
    float NoH = clamp(dot(N, H), 0.0, 1.0);
    float VoH = clamp(dot(V, H), 0.0, 1.0);

    float D = D_GGX(NoH, roughness);
    float G = G_Smith(NoV, NoL, roughness);
    vec3  F = fresnelSchlick(VoH, F0);

    return (D * G * F) / max(4.0 * NoV * NoL, 0.001);
}