#include "types.glsl"

#define MAX_BVH_STACK_SIZE 16
#define PI 3.14159265359

layout(std430, binding = 0) readonly restrict buffer entityInstances { EntityInstance EntityInstances[]; };
layout(std430, binding = 1) readonly restrict buffer triangleData    { Triangle TriangleData[]; };
layout(std430, binding = 2) readonly restrict buffer sceneNodes      { BvhNode SceneNodes[]; };
layout(std430, binding = 3) readonly restrict buffer meshNodes       { BvhNode MeshNodes[]; };
layout(std430, binding = 4) buffer lightsBuffer                      { Light lights[];  };
layout(std430, binding = 5) buffer PointCloudBuffer                  { CloudPoint points[]; };
layout(std430, binding = 6) buffer ProbeBuffer                       { ProbeColor probeColors[]; };

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

// Möller-Trumbore ray-triangle interesction algorithm
bool IntersectTriangle(in vec3 rayOrigin, in vec3 rayDir, float minDistance, float maxDistance, in vec3 p0, in vec3 e1, in vec3 e2, in vec3 normal) {
    vec3 c = p0 - rayOrigin;
    vec3 r = cross(rayDir, c);
    float det = dot(normal, rayDir);
    
    // Backface culling (if your geometry is strictly enclosed)
    if (abs(det) < 0.000001) return false; 
    
    float invDet = 1.0 / det;
    float u = dot(r, e2) * invDet;
    
    // Bail if u is outside the triangle
    if (u < 0.0 || u > 1.0) return false;

    float v = dot(r, e1) * invDet;
    
    // Bail if v is outside or u+v > 1
    if (v < 0.0 || u + v > 1.0) return false;

    float t = dot(normal, c) * invDet;
    return (t >= minDistance && t < maxDistance);
}

// Ray-mesh bvh interesction
bool MeshAnyHit(in uint rootNodeIndex, in vec3 rayOrigin, in vec3 rayDir, float minDistance, float maxDistance, in mat4 inverseWorldTransform) {
    uint stack[MAX_BVH_STACK_SIZE];
    uint stack_size = 0;
    stack[stack_size++] = rootNodeIndex;
    
    // Precompute transformed ray in mesh space once
    vec3 localOrigin = vec3(inverseWorldTransform * vec4(rayOrigin, 1.0));
    vec3 localEnd = vec3(inverseWorldTransform * vec4(rayOrigin + rayDir * maxDistance, 1.0));
    vec3 localDir = normalize(localEnd - localOrigin);
    float localMaxDistance = length(localEnd - localOrigin);
    float localMinDistance = minDistance * localMaxDistance / maxDistance;
    vec3 localDirInv = 1.0 / localDir;
    
    // Walk the bvh
    while (stack_size != 0) {
        uint current = stack[--stack_size];
        BvhNode node = MeshNodes[current];

        // If there was no ray hit with this node, skip th children
        if (!IntersectNode(localOrigin, localDirInv, localMinDistance, localMaxDistance, node.boundsMin, node.boundsMax)) {
            continue;
        }
        
        // If there was a hit, and the current node is a leaf, then walk the bvh of each triangles within this leaf node
        else if (node.primitiveCount > 0) {
            for (int i = 0; i < int(node.primitiveCount); i++) {
                uint triIndex = (node.firstChildOrPrimitive / 12) + i;
                Triangle tri = TriangleData[triIndex];

                // Reconstruct vectors from packed layout
                vec3 p0 = tri.v0_and_e1x.xyz;
                vec3 e1 = vec3(tri.v0_and_e1x.w, tri.e1yz_and_e2xy.xy);
                vec3 e2 = vec3(tri.e1yz_and_e2xy.zw, tri.e2z_and_normal.x);
                vec3 normal = tri.e2z_and_normal.yzw;

                if (IntersectTriangle(localOrigin, localDir, localMinDistance, localMaxDistance, p0, e1, e2, normal))
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