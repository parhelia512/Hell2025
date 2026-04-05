#include "types.glsl"

#define MAX_BVH_STACK_SIZE 16
#define PI 3.14159265359

layout(std430, binding = 0) readonly restrict buffer entityInstances { EntityInstance EntityInstances[]; };
layout(std430, binding = 1) readonly restrict buffer triangleData    { Triangle TriangleData[]; };
layout(std430, binding = 2) readonly restrict buffer sceneNodes      { BvhNode SceneNodes[]; };
layout(std430, binding = 3) readonly restrict buffer meshNodes       { BvhNode MeshNodes[]; };

struct RayResult {
    float t;
    uint instanceIndex;
    uint triangleIndex;
    vec2 barycentrics;
};

// "Slab test" ray-aabb intersection algorithm
bool IntersectNode(in vec3 rayOrigin, in vec3 rayDirInv, float minDistance, float maxDistance, in vec3 boundsMin, in vec3 boundsMax) {
    vec3 t0 = (boundsMin - rayOrigin) * rayDirInv;
    vec3 t1 = (boundsMax - rayOrigin) * rayDirInv;
    
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    
    float tNear = max(max(tmin.x, tmin.y), tmin.z);
    float tFar  = min(min(tmax.x, tmax.y), tmax.z);
    
    return tNear <= tFar && tFar >= minDistance && tNear <= maxDistance;
}

float IntersectNodeDist(vec3 rayOrigin, vec3 rayDirInv, vec3 boundsMin, vec3 boundsMax) {
    vec3 t0 = (boundsMin - rayOrigin) * rayDirInv;
    vec3 t1 = (boundsMax - rayOrigin) * rayDirInv;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float tNear = max(max(tmin.x, tmin.y), tmin.z);
    float tFar  = min(min(tmax.x, tmax.y), tmax.z);
    // return a massive distance if we miss to simplify sorting logic
    return (tNear <= tFar && tFar > 0.0) ? tNear : 1e27;
}

// Möller-Trumbore ray-triangle interesction algorithm
bool IntersectTriangle(in vec3 rayOrigin, in vec3 rayDir, float minDistance, float maxDistance, in vec3 p0, in vec3 e1, in vec3 e2, in vec3 normal) {
    vec3 c = p0 - rayOrigin;
    vec3 r = cross(rayDir, c);
    float det = dot(normal, rayDir);
    if (abs(det) < 0.000001) return false;
    
    float invDet = 1.0 / det;
    float u = dot(r, e2) * invDet;
    if (u < 0.0 || u > 1.0) return false;

    float v = dot(r, e1) * invDet;
    if (v < 0.0 || u + v > 1.0) return false;

    float t = dot(normal, c) * invDet;
    return (t >= minDistance && t < maxDistance);
}

bool IntersectTriangleClosest(in vec3 rayOrigin, in vec3 rayDir, float minDistance, inout float maxDistance, in vec3 p0, in vec3 e1, in vec3 e2, in vec3 normal, out vec2 barycentrics) {
    vec3 c = p0 - rayOrigin;
    vec3 r = cross(rayDir, c);
    float det = dot(normal, rayDir);
    
    // prevent division by zero for rays parallel to the surface
    if (abs(det) < 0.000001) {
        return false;
    }
    
    float invDet = 1.0 / det;
    float u = dot(r, e2) * invDet;
    
    if (u < 0.0 || u > 1.0) {
        return false;
    }

    float v = dot(r, e1) * invDet;
    
    if (v < 0.0 || u + v > 1.0) {
        return false;
    }

    float t = dot(normal, c) * invDet;
    
    if (t >= minDistance && t < maxDistance) {
        maxDistance = t;
        barycentrics = vec2(u, v);
        return true;
    }
    
    return false;
}

bool IntersectTriangleClosestOLD(in vec3 rayOrigin, in vec3 rayDir, float minDistance, inout float maxDistance, in vec3 p0, in vec3 e1, in vec3 e2, in vec3 normal, out float signedT, out vec2 barycentrics) {
    vec3 c = p0 - rayOrigin;
    vec3 r = cross(rayDir, c);
    float det = dot(normal, rayDir);
    if (abs(det) < 0.000001) return false;

    float invDet = 1.0 / det;
    float u = dot(r, e2) * invDet;
    if (u < 0.0 || u > 1.0) return false;

    float v = dot(r, e1) * invDet;
    if (v < 0.0 || u + v > 1.0) return false;

    float t = dot(normal, c) * invDet;
    float absT = abs(t);

    if (absT >= minDistance && absT < maxDistance) {
        maxDistance = absT; 
        barycentrics = vec2(u, v);
        signedT = (det < 0.0) ? -absT : absT;
        return true;
    }
    return false;
}

bool MeshAnyHitOLD(in uint rootNodeIndex, in vec3 rayOrigin, in vec3 rayDir, float minDistance, float maxDistance, in mat4 inverseWorldTransform) {
    uint stack[MAX_BVH_STACK_SIZE];
    uint stack_size = 0;
    stack[stack_size++] = rootNodeIndex;
    
    // map origin point to local space
    vec3 localOrigin = vec3(inverseWorldTransform * vec4(rayOrigin, 1.0));
    
    // map direction vector to local space without translation
    vec3 localDirUnorm = vec3(inverseWorldTransform * vec4(rayDir, 0.0));
    float localToWorldScale = 1.0 / length(localDirUnorm);
    vec3 localDir = localDirUnorm * localToWorldScale;
    
    // cache scaled distances
    float localMaxDistance = maxDistance / localToWorldScale;
    float localMinDistance = minDistance / localToWorldScale;
    vec3 localDirInv = 1.0 / localDir;
    
    // walk the bvh
    while (stack_size != 0) {
        uint current = stack[--stack_size];
        BvhNode node = MeshNodes[current];

        // skip children if aabb missed
        if (!IntersectNode(localOrigin, localDirInv, localMinDistance, localMaxDistance, node.boundsMin, node.boundsMax)) {
            continue;
        }
        
        if (node.primitiveCount > 0) {
            for (int i = 0; i < int(node.primitiveCount); i++) {
                uint triIndex = (node.firstChildOrPrimitive / 12) + i;
                Triangle tri = TriangleData[triIndex];

                vec3 p0 = tri.v0_and_e1x.xyz;
                vec3 e1 = vec3(tri.v0_and_e1x.w, tri.e1yz_and_e2xy.xy);
                vec3 e2 = vec3(tri.e1yz_and_e2xy.zw, tri.e2z_and_normal.x);
                vec3 normal = tri.e2z_and_normal.yzw;

                if (IntersectTriangle(localOrigin, localDir, localMinDistance, localMaxDistance, p0, e1, e2, normal)) {
                    return true;
                }
            }
        } else {
            if (stack_size >= MAX_BVH_STACK_SIZE - 1) {
                continue;
            } else {
                stack[stack_size++] = node.firstChildOrPrimitive;
                stack[stack_size++] = node.firstChildOrPrimitive + 1;
            }
        }
    }
    return false;
}

bool MeshAnyHit(in uint rootNodeIndex, in vec3 rayOrigin, in vec3 rayDir, float minDistance, float maxDistance, in mat4 inverseWorldTransform) {
    uint stack[MAX_BVH_STACK_SIZE];
    uint stack_size = 0;
    stack[stack_size++] = rootNodeIndex;
    
    // scale is 1.0 so i can just transform origin and direction directly
    vec3 localOrigin = vec3(inverseWorldTransform * vec4(rayOrigin, 1.0));
    vec3 localDir = vec3(inverseWorldTransform * vec4(rayDir, 0.0));
    vec3 localDirInv = 1.0 / localDir;
    
    while (stack_size != 0) {
        uint current = stack[--stack_size];
        BvhNode node = MeshNodes[current];

        if (node.primitiveCount > 0) {
            for (int i = 0; i < int(node.primitiveCount); i++) {
                uint triIndex = (node.firstChildOrPrimitive / 12) + i;
                Triangle tri = TriangleData[triIndex];

                // extract vertices and edges
                vec3 p0 = tri.v0_and_e1x.xyz;
                vec3 e1 = vec3(tri.v0_and_e1x.w, tri.e1yz_and_e2xy.xy);
                vec3 e2 = vec3(tri.e1yz_and_e2xy.zw, tri.e2z_and_normal.x);
                vec3 normal = tri.e2z_and_normal.yzw;

                if (IntersectTriangle(localOrigin, localDir, minDistance, maxDistance, p0, e1, e2, normal)) {
                    return true; // found a hit, bail out early
                }
            }
        } else {
            uint left = node.firstChildOrPrimitive;
            uint right = node.firstChildOrPrimitive + 1;

            // sort children front to back based on distance
            float distL = IntersectNodeDist(localOrigin, localDirInv, MeshNodes[left].boundsMin, MeshNodes[left].boundsMax);
            float distR = IntersectNodeDist(localOrigin, localDirInv, MeshNodes[right].boundsMin, MeshNodes[right].boundsMax);

            // push the farther one first so the closer one is processed first
            if (distL < distR) {
                if (distR < maxDistance) stack[stack_size++] = right;
                if (distL < maxDistance) stack[stack_size++] = left;
            } else {
                if (distL < maxDistance) stack[stack_size++] = left;
                if (distR < maxDistance) stack[stack_size++] = right;
            }
        }
    }
    return false;
}

// Scene-mesh bvh interesction
bool AnyHit(in vec3 rayOrigin, in vec3 rayDir, float minDistance, float maxDistance) {
    uint stack[MAX_BVH_STACK_SIZE];
    uint stack_size = 0;
    stack[stack_size++] = 0; // start with the rootnode
    vec3 rayDirInv = 1.0 / rayDir;
    
    // Walk the bvh
    while (stack_size != 0) {
        uint current = stack[--stack_size];
        BvhNode node = SceneNodes[current];

        // If there is no ray hit on the node AABB, then skip its children
        if (!IntersectNode(rayOrigin, rayDirInv, minDistance, maxDistance, node.boundsMin, node.boundsMax))
            continue;

        // If there was a hit, and the current node is a leaf, then walk the bvh of each entity within this leaf node
        else if (node.primitiveCount > 0) {
            for (int i = 0; i < int(node.primitiveCount); i++) {
                EntityInstance instance = EntityInstances[node.firstChildOrPrimitive + i];

                if (MeshAnyHit(uint(instance.rootNodeIndex), rayOrigin, rayDir, minDistance, maxDistance, instance.inverseWorldTransform))
                    return true;
            }
        }
        // Otherwise, recursively process the child nodes
        else {
            // Prevent stack overflow
            if (stack_size >= MAX_BVH_STACK_SIZE - 1) {
                 continue;
            } else {
                 stack[stack_size++] = node.firstChildOrPrimitive;
                 stack[stack_size++] = node.firstChildOrPrimitive + 1;
            }
        }
    }
    return false;
}



bool IntersectTriangleClosest(in vec3 rayOrigin, in vec3 rayDir, float minDistance, inout float maxDistance, in vec3 p0, in vec3 e1, in vec3 e2, in vec3 normal, out float signedT, out vec2 barycentrics) {
    vec3 c = p0 - rayOrigin;
    vec3 r = cross(rayDir, c);
    float det = dot(normal, rayDir);

    if (abs(det) < 0.000001) return false;

    float invDet = 1.0 / det;
    float u = dot(r, e2) * invDet;
    if (u < 0.0 || u > 1.0) return false;

    float v = dot(r, e1) * invDet;
    if (v < 0.0 || u + v > 1.0) return false;

    float t = dot(normal, c) * invDet;
    float absT = abs(t);

    if (absT >= minDistance && absT < maxDistance) {
        maxDistance = absT; // Shrink the ray for BVH traversal using absolute distance
        barycentrics = vec2(u, v);
        // If det > 0, the ray and normal point in the same direction (Backface)
        //signedT = (det > 0.0) ? -absT : absT; 
        signedT = (det < 0.0) ? -absT : absT;
        return true;
    }
    return false;
}

// 2. Signed Mesh Traversal
bool MeshClosestHitOLD(in uint rootNodeIndex, in uint instanceIndex, in vec3 rayOrigin, in vec3 rayDir, float minDistance, inout float maxDistance, in mat4 inverseWorldTransform, inout RayResult record) {
    uint stack[MAX_BVH_STACK_SIZE];
    uint stack_size = 0;
    stack[stack_size++] = rootNodeIndex;
    bool hitFound = false;

    vec3 localOrigin = vec3(inverseWorldTransform * vec4(rayOrigin, 1.0));
    vec3 localDirUnorm = vec3(inverseWorldTransform * vec4(rayDir, 0.0));
    float localToWorldScale = 1.0 / length(localDirUnorm);
    vec3 localDir = localDirUnorm * localToWorldScale;

    float currentLocalMax = maxDistance / localToWorldScale;
    float localMinDistance = minDistance / localToWorldScale;
    vec3 localDirInv = 1.0 / localDir;

    while (stack_size != 0) {
        uint current = stack[--stack_size];
        BvhNode node = MeshNodes[current];
        if (!IntersectNode(localOrigin, localDirInv, localMinDistance, currentLocalMax, node.boundsMin, node.boundsMax)) continue;
        
        if (node.primitiveCount > 0) {
            for (int i = 0; i < int(node.primitiveCount); i++) {
                uint triIndex = (node.firstChildOrPrimitive / 12) + i;
                Triangle tri = TriangleData[triIndex];
                vec3 p0 = tri.v0_and_e1x.xyz;
                vec3 e1 = vec3(tri.v0_and_e1x.w, tri.e1yz_and_e2xy.xy);
                vec3 e2 = vec3(tri.e1yz_and_e2xy.zw, tri.e2z_and_normal.x);
                vec3 normal = tri.e2z_and_normal.yzw;

                float signedHitT;
                vec2 bary;
                if (IntersectTriangleClosestOLD(localOrigin, localDir, localMinDistance, currentLocalMax, p0, e1, e2, normal, signedHitT, bary)) {
                    hitFound = true;
                    maxDistance = currentLocalMax * localToWorldScale; 
                    record.t = signedHitT * localToWorldScale; // Store signed world distance
                    record.instanceIndex = instanceIndex;
                    record.triangleIndex = triIndex;
                    record.barycentrics = bary;
                }
            }
        } else {
            stack[stack_size++] = node.firstChildOrPrimitive;
            stack[stack_size++] = node.firstChildOrPrimitive + 1;
        }
    }
    return hitFound;
}

bool MeshClosestHit(in uint rootNodeIndex, in uint instanceIndex, in vec3 rayOrigin, in vec3 rayDir, float minDistance, inout float maxDistance, in mat4 inverseWorldTransform, inout RayResult record) {
    uint stack[MAX_BVH_STACK_SIZE];
    uint stack_size = 0;
    stack[stack_size++] = rootNodeIndex;
    bool hitFound = false;

    // scale is 1.0 so we just transform origin and direction directly
    vec3 localOrigin = vec3(inverseWorldTransform * vec4(rayOrigin, 1.0));
    vec3 localDir = vec3(inverseWorldTransform * vec4(rayDir, 0.0));
    vec3 localDirInv = 1.0 / localDir;

    while (stack_size != 0) {
        uint current = stack[--stack_size];
        BvhNode node = MeshNodes[current];

        if (node.primitiveCount > 0) {
            for (int i = 0; i < int(node.primitiveCount); i++) {
                uint triIndex = (node.firstChildOrPrimitive / 12) + i;
                Triangle tri = TriangleData[triIndex];
                
                float signedHitT;
                vec2 bary;
                if (IntersectTriangleClosest(localOrigin, localDir, minDistance, maxDistance, tri.v0_and_e1x.xyz, 
                    vec3(tri.v0_and_e1x.w, tri.e1yz_and_e2xy.xy), 
                    vec3(tri.e1yz_and_e2xy.zw, tri.e2z_and_normal.x), 
                    tri.e2z_and_normal.yzw, signedHitT, bary)) {
                    
                    hitFound = true;
                    record.t = signedHitT; 
                    record.instanceIndex = instanceIndex;
                    record.triangleIndex = triIndex;
                    record.barycentrics = bary;
                }
            }
        } else {
            uint left = node.firstChildOrPrimitive;
            uint right = node.firstChildOrPrimitive + 1;

            // sort children front to back
            float distL = IntersectNodeDist(localOrigin, localDirInv, MeshNodes[left].boundsMin, MeshNodes[left].boundsMax);
            float distR = IntersectNodeDist(localOrigin, localDirInv, MeshNodes[right].boundsMin, MeshNodes[right].boundsMax);

            if (distL < distR) {
                if (distR < maxDistance) stack[stack_size++] = right;
                if (distL < maxDistance) stack[stack_size++] = left;
            } else {
                if (distL < maxDistance) stack[stack_size++] = left;
                if (distR < maxDistance) stack[stack_size++] = right;
            }
        }
    }
    return hitFound;
}

// 3. Signed Scene Traversal
bool ClosestHit(in vec3 rayOrigin, in vec3 rayDir, float minDistance, float maxDistance, out RayResult record) {
    uint stack[MAX_BVH_STACK_SIZE];
    uint stack_size = 0;
    stack[stack_size++] = 0; 
    vec3 rayDirInv = 1.0 / rayDir;
    float currentMaxDistance = maxDistance;
    bool hitFound = false;

    while (stack_size != 0) {
        uint current = stack[--stack_size];
        BvhNode node = SceneNodes[current];
        if (!IntersectNode(rayOrigin, rayDirInv, minDistance, currentMaxDistance, node.boundsMin, node.boundsMax)) continue;

        if (node.primitiveCount > 0) {
            for (int i = 0; i < int(node.primitiveCount); i++) {
                uint instIndex = node.firstChildOrPrimitive + i;
                EntityInstance instance = EntityInstances[instIndex];
                if (MeshClosestHit(uint(instance.rootNodeIndex), instIndex, rayOrigin, rayDir, minDistance, currentMaxDistance, instance.inverseWorldTransform, record)) {
                    hitFound = true;
                }
            }
        } else {
            stack[stack_size++] = node.firstChildOrPrimitive;
            stack[stack_size++] = node.firstChildOrPrimitive + 1;
        }
    }
    return hitFound;
}