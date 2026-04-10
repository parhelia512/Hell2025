#version 460
//#include "../common/types.glsl"

in vec3 v_worldPos;

struct Light {
    float posX;
    float posY;
    float posZ;
    float colorR;

    float colorG;
    float colorB;
    float strength;
    float radius;

    int lightIndex;
    int shadowMapDirty; // true or false
    int useIes;         // true or false
    int iesIndex;

    float iesVScale;
    float iesVBias;
    float iesHScale;
    float iesHBias;

    vec3 forward;
    float iesMaxIntensity;

    vec3 right;
    float iesExposure;

    vec3 up;
    int iesTextureIndex;
};

struct AABB {
    vec4 boundsMin;
    vec4 boundsMax;
};

restrict layout(std430, binding = 4) readonly buffer lightsBuffer    { Light lights[]; };
restrict layout(std430, binding = 5) buffer          LightAABBBuffer { AABB lightAABBs[]; };

uniform int u_lightIndex;

uniform vec3 u_lightPosition;
uniform float u_farPlane;


void ZeroBuffer();
void UpdateBuffer();


void main() {
    //ZeroBuffer();
    UpdateBuffer();
}


void UpdateBuffer() {
    int u_lightIndex = 3;

    Light light = lights[u_lightIndex];
    vec3 pos = vec3(light.posX, light.posY, light.posZ);
    float radius = light.radius;

    vec3 oldBoundsMin = lightAABBs[u_lightIndex].boundsMin.xyz;
    vec3 oldBoundsMax = lightAABBs[u_lightIndex].boundsMax.xyz;

    vec3 sphereBoundsMin = pos - vec3(radius);
    vec3 sphereBoundsMax = pos + vec3(radius);

    //float newBoundsMaxX = min(max(oldBoundsMax.x, v_worldPos.x), sphereBoundsMax.x);
    //float newBoundsMaxY = min(max(oldBoundsMax.y, v_worldPos.y), sphereBoundsMax.y);
    //float newBoundsMaxZ = min(max(oldBoundsMax.z, v_worldPos.z), sphereBoundsMax.z);

    float newBoundsMaxX = max(oldBoundsMax.x, v_worldPos.x);
    float newBoundsMaxY = max(oldBoundsMax.y, v_worldPos.y);
    float newBoundsMaxZ = max(oldBoundsMax.z, v_worldPos.z);


    ///vec3 newBoundsMax = vec3(newBoundsMaxX, oldBoundsMax.y, oldBoundsMax.z);
    //vec3 newBoundsMax = vec3(newBoundsMaxX, newBoundsMaxY, newBoundsMaxZ);

    vec3 newBoundsMax = vec3(newBoundsMaxX, oldBoundsMax.y, oldBoundsMax.z);

    lightAABBs[u_lightIndex].boundsMax.xyz = newBoundsMax;
}



// THIS FUNCTION ZEROS OUT YOUR SHIT
void ZeroBuffer() {
    Light light = lights[u_lightIndex];
    vec3 pos = vec3(light.posX, light.posY, light.posZ);
    float radius = 0.01;// light.radius;

    lightAABBs[u_lightIndex].boundsMin.xyz = pos - vec3(radius);
    lightAABBs[u_lightIndex].boundsMax.xyz = pos + vec3(radius);
}