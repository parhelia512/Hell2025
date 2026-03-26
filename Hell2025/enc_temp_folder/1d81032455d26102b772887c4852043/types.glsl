
struct ViewportData {
    mat4 projection;
    mat4 inverseProjection;
    mat4 view;
    mat4 inverseView;
    mat4 projectionView;
    mat4 inverseProjectionView;
    mat4 skyboxProjectionView;
    mat4 flashlightProjectionView;
    mat4 previousProjectionView;

    mat4 csmLightProjectionView[5];

    int xOffset;
    int yOffset;
    int width;
    int height;

    float posX;  // 0 t0 1 range
    float posY;  // 0 t0 1 range
    float sizeX; // 0 t0 1 range
    float sizeY; // 0 t0 1 range

    vec4 frustumPlane0;
    vec4 frustumPlane1;
    vec4 frustumPlane2;
    vec4 frustumPlane3;
    vec4 frustumPlane4;
    vec4 frustumPlane5;
    vec4 flashlightDir;
    vec4 flashlightPosition;

    float flashlightModifer;
    int isOrtho; //true or false
    float orthoSize;
    float fov;

    vec4 viewPos;
    vec4 cameraForward;
    vec4 cameraUp;
    vec4 cameraRight;

    vec4 colorTint;

    float colorContrast;
    int isInShop; //true or false
    float padding1;
    float intensityScalar;

    vec4 vignetteColor;
};

struct RendererData {
    float nearPlane;
    float farPlane;
    float gBufferWidth;
    float gBufferHeight;
    float hairBufferWidth;
    float hairBufferHeight;
    float time;
    int splitscreenMode;
    int rendererOverrideState;
    float normalizedMouseX;
    float normalizedMouseY;
    int tileCountX;
    int tileCountY;
};

struct RenderItem {
    mat4 modelMatrix;
    mat4 inverseModelMatrix;
    vec4 aabbMin;
    vec4 aabbMax;

    int meshIndex;
    int baseColorTextureIndex;
    int normalMapTextureIndex;
    int rmaTextureIndex;

    int objectType;
    int woundMaskTextureIndex;
    int exclusiveViewportIndex;
    int ignoredViewportIndex;

    uint objectIdLowerBit;
    uint objectIdUpperBit;
    int baseSkinnedVertex;
    int baseSkinningTransformIndex;

    uint openableId;
    uint customId;
    int skinned;                        // True or false
    int castShadows;                    // True or false

    float emissiveR;
    float emissiveG;
    float emissiveB;
    int emissiveTextureIndex;           // -1 means nothing, anything else is a texture index

    float furLength;
    float furShellDistanceAttenuation;
    float furUVScale;
    int blockScreenSpaceBloodDecals;    // True or false

    int woundBaseColorTextureIndex;
    int woundNormalMapTextureIndex;
    int woundRmaTextureIndex;
    int localMeshNodeIndex;

    float tintColorR;
    float tintColorG;
    float tintColorB;
    int castCSMShadows;                 // True or false
};

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
    int padding0;
    int padding1;
};

struct TileLights {
    uint lightCount;
    uint lightIndices[127];
};

struct TileWorldBounds {
    vec4 boundsMin; // w: count of non-background pixels
    vec4 boundsMax; // w: unused
};

//struct TileBloodDecals {
//    uint decalCount;
//    uint decalOffset;
//};

struct TileInstanceData {
    uint count;
    uint offset;
};

struct BloodDecal {
    mat4 modelMatrix;
    mat4 inverseModelMatrix;
    int type;
    int textureIndex;
    int padding1;
    int padding2;
};

struct ChristmasLight {
    vec4 position;
    vec4 color;
};

struct MetaBall {
    vec4 posAndInvSigma2;
};

struct CloudPoint {
    vec4 position;
    vec4 normal;
    vec4 directLighting;
    vec4 baseColor;
};

struct CloudPointTextureInfo {
    float u;
    float v;
    int baseColorIndex;
    int rmaIndex;
};

struct BvhNode {
    vec3 boundsMin;
    uint firstChildOrPrimitive;
    vec3 boundsMax;
    uint primitiveCount;
};

struct EntityInstance {
    mat4 worldTransform;
    mat4 inverseWorldTransform;

    int rootNodeIndex;
    int objectIdLowerBit;    // Unused on the GPU currently. CPU bvh stuff uses it.
    int objectIdUpperBit;    // Unused on the GPU currently. CPU bvh stuff uses it.
    int openableId;          // Unused on the GPU currently. CPU bvh stuff uses it.

    uint globalMeshIndex;    // Also unsued by the GPU
    uint customId;           // Also unsued by the GPU
    uint localMeshNodeIndex; // Also unsued by the GPU
    uint padding2;           // Also unsued by the GPU
};                           // Also unsued by the GPU

struct Triangle {
    vec4 v0_and_e1x;     // p0.xyz, e1.x
    vec4 e1yz_and_e2xy;  // e1.yz,  e2.xy
    vec4 e2z_and_normal; // e2.z,   normal.xyz
};

struct DispatchIndirectCommand {
    uint num_groups_x;
    uint num_groups_y;
    uint num_groups_z;
};


const int PROBE_DISTANCE_OCTA_SIZE = 16;
const int INTERIOR_SIZE = 14; 
//const int PROBE_NUM_IRRADIANCE_INTERIOR_TEXELS = 14;
const int PROBE_NUM_DISTANCE_INTERIOR_TEXELS = 14;

//#define PROBE_DISTANCE_OCTA_SIZE 16
#define PROBE_DISTANCE_TEXEL_COUNT (PROBE_DISTANCE_OCTA_SIZE * PROBE_DISTANCE_OCTA_SIZE)
//#define RAYS_PER_PROBE 256

struct ProbeDistance {
    vec2 moments[PROBE_DISTANCE_TEXEL_COUNT];
};

struct ProbeColor {
    vec4 sh[9];
};
