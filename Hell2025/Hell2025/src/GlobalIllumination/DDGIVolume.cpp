#include "DDGIVolume.h"

#include "Bvh/Gpu/Bvh.h"
#include "Renderer/Renderer.h" // remove me
#include "World/World.h"

#include <Hell/Logging.h>

DDGIVolume::DDGIVolume(uint64_t id, DDGIVolumeCreateInfo& createInfo, SpawnOffset& spawnOffset) {
    m_id = id;
    m_createInfo = createInfo;
    m_createInfo.origin += spawnOffset.translation; 
    m_createInfo.rotation += glm::vec3(0.0f, spawnOffset.yRotation, 0.0f);

    UpdateMembers();

    std::cout << "Total probe count: " << GetTotalProbeCount() << "\n";
}

void DDGIVolume::Update() {
    if (m_raytracingDataDirty) {
        CreateRaytracingData();
        m_raytracingDataDirty = false;
    }

    // Also in here, find a way to do an Immediate style upload of the point cloud data + compute point light base color
    // This will be handy for Vulkan also.

    m_pointCloud.DebugDrawGrid();
}

void DDGIVolume::CleanUp() {
    CleanUpRaytracingData();
}

void DDGIVolume::CleanUpRaytracingData() {
    Bvh::Gpu::DestroyMeshBvh(m_doorBvhId);
    Bvh::Gpu::DestroyMeshBvh(m_houseBvhId);
    Bvh::Gpu::DestroySceneBvh(m_sceneBvhId);

    m_doorBvhId = 0;
    m_houseBvhId = 0;
    m_sceneBvhId = 0;
    m_probePointIndexPoolSize = 0;

    m_triangles.clear();
    m_pointCloud.CleanUp();

}

void DDGIVolume::CreateRaytracingData() {
    CleanUpRaytracingData();

    m_sceneBvhId = Bvh::Gpu::CreateNewSceneBvh();

    CreateTriangleData();
    CreateHouseBvh();
    CreateDoorBvh(); // Probably rewrite this so doors internally manager their own BVH, that way stained glass ones can have holes
    CreatePointCloud();
    CalculateProbePointIndexPoolSize();

    Bvh::Gpu::FlatternMeshBvhNodes();
}

void DDGIVolume::CreateTriangleData() {
    m_triangles.clear();

    // Gather floor and ceilings triangles
    for (HousePlane& plane : World::GetHousePlanes()) {
        for (uint32_t i = 0; i < plane.GetIndices().size(); i += 3) {
            int idx0 = plane.GetIndices()[i + 0];
            int idx1 = plane.GetIndices()[i + 1];
            int idx2 = plane.GetIndices()[i + 2];

            glm::vec3 v0 = plane.GetVertices()[idx0].position;
            glm::vec3 v1 = plane.GetVertices()[idx1].position;
            glm::vec3 v2 = plane.GetVertices()[idx2].position;

            // Get triangle bounds
            glm::vec3 triMin = glm::min(glm::min(v0, v1), v2);
            glm::vec3 triMax = glm::max(glm::max(v0, v1), v2);

            // Cull if triangle is completely outside the exact volume bounds
            if (triMax.x < m_boundsMin.x || triMin.x > m_boundsMax.x ||
                triMax.y < m_boundsMin.y || triMin.y > m_boundsMax.y ||
                triMax.z < m_boundsMin.z || triMin.z > m_boundsMax.z) {
                continue;
            }

            Triangle& triangle = m_triangles.emplace_back();
            triangle.v0 = v0;
            triangle.v1 = v1;
            triangle.v2 = v2;
            triangle.uv0 = plane.GetVertices()[idx0].uv;
            triangle.uv1 = plane.GetVertices()[idx1].uv;
            triangle.uv2 = plane.GetVertices()[idx2].uv;
            triangle.baseColorTextureIndex = plane.GetMaterial()->m_basecolor;
            triangle.rmaTextureIndex = plane.GetMaterial()->m_rma;
        }
    }

    // Gather wall triangles
    for (Wall& wall : World::GetWalls()) {

        // Gather exterior walls
        if (wall.IsWeatherBoards()) continue;

        for (WallSegment& wallSegment : wall.GetWallSegments()) {
            for (uint32_t i = 0; i < wallSegment.GetIndices().size(); i += 3) {
                int idx0 = wallSegment.GetIndices()[i + 0];
                int idx1 = wallSegment.GetIndices()[i + 1];
                int idx2 = wallSegment.GetIndices()[i + 2];

                glm::vec3 v0 = wallSegment.GetVertices()[idx0].position;
                glm::vec3 v1 = wallSegment.GetVertices()[idx1].position;
                glm::vec3 v2 = wallSegment.GetVertices()[idx2].position;

                // Get triangle bounds
                glm::vec3 triMin = glm::min(glm::min(v0, v1), v2);
                glm::vec3 triMax = glm::max(glm::max(v0, v1), v2);

                // Cull if triangle is completely outside the exact volume bounds
                if (triMax.x < m_boundsMin.x || triMin.x > m_boundsMax.x ||
                    triMax.y < m_boundsMin.y || triMin.y > m_boundsMax.y ||
                    triMax.z < m_boundsMin.z || triMin.z > m_boundsMax.z) {
                    continue;
                }

                Triangle& triangle = m_triangles.emplace_back();
                triangle.v0 = v0;
                triangle.v1 = v1;
                triangle.v2 = v2;
                triangle.uv0 = wallSegment.GetVertices()[idx0].uv;
                triangle.uv1 = wallSegment.GetVertices()[idx1].uv;
                triangle.uv2 = wallSegment.GetVertices()[idx2].uv;
                triangle.baseColorTextureIndex = wall.GetMaterial()->m_basecolor;
                triangle.rmaTextureIndex = wall.GetMaterial()->m_rma;
            }
        }
    }

    // Recompute normals
    for (Triangle& triangle : m_triangles) {
        glm::vec3 edge1 = triangle.v1 - triangle.v0;
        glm::vec3 edge2 = triangle.v2 - triangle.v0;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
        triangle.normal = normal;
    }
}

void DDGIVolume::CreateHouseBvh() {
    // Destroy any previous house bvh
    if (m_houseBvhId != 0) {
        Bvh::Gpu::DestroyMeshBvh(m_houseBvhId);
    }

    // Create house vertices
    std::vector<Vertex> vertices;
    for (Triangle& triangle : m_triangles) {
        Vertex v0, v1, v2;
        v0.position = triangle.v0;
        v1.position = triangle.v1;
        v2.position = triangle.v2;
        v0.normal = triangle.normal;
        v1.normal = triangle.normal;
        v2.normal = triangle.normal;
        vertices.push_back(triangle.v0);
        vertices.push_back(triangle.v1);
        vertices.push_back(triangle.v2);
    }

    // Create house indices
    std::vector<uint32_t> indices(vertices.size());
    for (int i = 0; i < vertices.size(); i++) {
        indices[i] = i;
    }

    m_houseBvhId = Bvh::Gpu::CreateMeshBvhFromVertexData(vertices, indices);
}

void DDGIVolume::CreateDoorBvh() { 
    // ATTENTION:
    // Probably rewrite this so doors internally manager their own BVH, that way stained glass ones can have holes
    // You'll need a function that gets PxBox vertices from PxShape box and computes normals, that'd do it.

    float w = DOOR_DEPTH;
    float h = DOOR_HEIGHT;
    float d = DOOR_WIDTH;

    std::vector<Vertex> vertices;
    vertices.reserve(24);

    // front face
    vertices.emplace_back(Vertex(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1)));
    vertices.emplace_back(Vertex(glm::vec3(-w, 0, 0), glm::vec3(0, 0, 1)));
    vertices.emplace_back(Vertex(glm::vec3(-w, h, 0), glm::vec3(0, 0, 1)));
    vertices.emplace_back(Vertex(glm::vec3(0, h, 0), glm::vec3(0, 0, 1)));

    // back face
    vertices.emplace_back(Vertex(glm::vec3(0, 0, -d), glm::vec3(0, 0, -1)));
    vertices.emplace_back(Vertex(glm::vec3(-w, 0, -d), glm::vec3(0, 0, -1)));
    vertices.emplace_back(Vertex(glm::vec3(-w, h, -d), glm::vec3(0, 0, -1)));
    vertices.emplace_back(Vertex(glm::vec3(0, h, -d), glm::vec3(0, 0, -1)));

    // left face
    vertices.emplace_back(Vertex(glm::vec3(-w, 0, 0), glm::vec3(-1, 0, 0)));
    vertices.emplace_back(Vertex(glm::vec3(-w, 0, -d), glm::vec3(-1, 0, 0)));
    vertices.emplace_back(Vertex(glm::vec3(-w, h, -d), glm::vec3(-1, 0, 0)));
    vertices.emplace_back(Vertex(glm::vec3(-w, h, 0), glm::vec3(-1, 0, 0)));

    // right face
    vertices.emplace_back(Vertex(glm::vec3(0, 0, -d), glm::vec3(1, 0, 0)));
    vertices.emplace_back(Vertex(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0)));
    vertices.emplace_back(Vertex(glm::vec3(0, h, 0), glm::vec3(1, 0, 0)));
    vertices.emplace_back(Vertex(glm::vec3(0, h, -d), glm::vec3(1, 0, 0)));

    // top face
    vertices.emplace_back(Vertex(glm::vec3(0, h, 0), glm::vec3(0, 1, 0)));
    vertices.emplace_back(Vertex(glm::vec3(-w, h, 0), glm::vec3(0, 1, 0)));
    vertices.emplace_back(Vertex(glm::vec3(-w, h, -d), glm::vec3(0, 1, 0)));
    vertices.emplace_back(Vertex(glm::vec3(0, h, -d), glm::vec3(0, 1, 0)));

    // bottom face
    vertices.emplace_back(Vertex(glm::vec3(0, 0, -d), glm::vec3(0, -1, 0)));
    vertices.emplace_back(Vertex(glm::vec3(-w, 0, -d), glm::vec3(0, -1, 0)));
    vertices.emplace_back(Vertex(glm::vec3(-w, 0, 0), glm::vec3(0, -1, 0)));
    vertices.emplace_back(Vertex(glm::vec3(0, 0, 0), glm::vec3(0, -1, 0)));

    std::vector<uint32_t> indices;
    indices.reserve(36);

    // map indices
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t offset = i * 4;
        indices.push_back(offset + 0);
        indices.push_back(offset + 1);
        indices.push_back(offset + 2);
        indices.push_back(offset + 2);
        indices.push_back(offset + 3);
        indices.push_back(offset + 0);
    }

    m_doorBvhId = Bvh::Gpu::CreateMeshBvhFromVertexData(vertices, indices);
}

void DDGIVolume::CreatePointCloud() {
    m_pointCloud.Create(m_triangles, GetBoundsMin(), GetBoundsMax(), GetPointCloudSpacing(), 3.0f);
    m_pointCloudNeedsGpuUpload = true;
}

void DDGIVolume::CalculateProbePointIndexPoolSize() {
    const PointCloud& pointCloud = GetPointClound();
    const glm::ivec3 gridDims = pointCloud.GetGridDimensions();
    const float gridCellSize = pointCloud.GetGridCellSize();
    const std::vector<uint32_t>& gridCellCounts = pointCloud.GetGridCellCounts();

    // Calculate how many probes originate in a single point-grid cell
    float probesPerAxis = gridCellSize / GetProbeSpacing();
    uint32_t probesPerCell = static_cast<uint32_t>(std::ceil(probesPerAxis * probesPerAxis * probesPerAxis));

    m_probePointIndexPoolSize = 0;

    // Map wide density scan
    for (int z = 0; z < gridDims.z; ++z) {
        for (int y = 0; y < gridDims.y; ++y) {
            for (int x = 0; x < gridDims.x; ++x) {

                uint32_t pointsIn27Cells = 0;

                // Sum all points in the 3x3x3 neighborhood of this cell
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = x + dx;
                            int ny = y + dy;
                            int nz = z + dz;

                            if (nx >= 0 && nx < gridDims.x &&
                                ny >= 0 && ny < gridDims.y &&
                                nz >= 0 && nz < gridDims.z) {

                                // Flatten the 3D coords to get the point count for this specific cell
                                int cellIdx = nx + ny * gridDims.x + nz * gridDims.x * gridDims.y;
                                pointsIn27Cells += gridCellCounts[cellIdx];
                            }
                        }
                    }
                }

                // Every probe that could possibly "start" in this cell is allocated the full point-count of its neighborhood
                m_probePointIndexPoolSize += (pointsIn27Cells * probesPerCell);
            }
        }
    }
}

void DDGIVolume::UpdateSceneBvh() {
    std::vector<PrimitiveInstance> instances;

    // Add the house
    PrimitiveInstance& instance = instances.emplace_back();
    instance.worldAabbBoundsMin = Bvh::Gpu::GetMeshBvhRootNodeBoundsMin(m_houseBvhId); // This works because the house mesh never rotates
    instance.worldAabbBoundsMax = Bvh::Gpu::GetMeshBvhRootNodeBoundsMax(m_houseBvhId); // This works because the house mesh never rotates
    instance.objectId = 0;
    instance.worldTransform = glm::mat4(1.0f);
    instance.inverseWorldTransform = glm::inverse(instance.worldTransform);
    instance.meshBvhId = m_houseBvhId;
    instance.worldAabbCenter = (instance.worldAabbBoundsMin + instance.worldAabbBoundsMax) * 0.5f;

    // Add all the doors
    for (Door& door : World::GetDoors()) {
        // Bit of a hack but get the hinges matrix, then zero out the y translation to match the doors main model matrix
        MeshNode* meshNode = door.GetMeshNodes().GetMeshNodeByMeshName("Door_Hinges");
        glm::mat4 worldMatrix = meshNode->worldMatrix;
        worldMatrix[3][1] = door.GetDoorModelMatrix()[3][1];

        // The PhysX aabb is tighter than the one your MeshNode holds, so use that
        const AABB& aabb = door.GetPhsyicsAABB();

        PrimitiveInstance& instance = instances.emplace_back();
        instance.worldAabbBoundsMin = aabb.GetBoundsMin();
        instance.worldAabbBoundsMax = aabb.GetBoundsMax();
        instance.objectId = door.GetObjectId();
        instance.worldTransform = worldMatrix;
        instance.inverseWorldTransform = glm::inverse(instance.worldTransform);
        instance.meshBvhId = m_doorBvhId;
        instance.worldAabbCenter = (instance.worldAabbBoundsMin + instance.worldAabbBoundsMax) * 0.5f;
    }

    Bvh::Gpu::UpdateSceneBvh(m_sceneBvhId, instances);
}

void DDGIVolume::DebugDraw() {
    // Draw volume bounds as AABB
    if (false) {
        AABB aabb = AABB(GetBoundsMin(), GetBoundsMax());
        Renderer::DrawAABB(aabb, YELLOW);
    }

    // Draw probe positions as points
    if (false) {
        for (uint32_t x = 0; x < m_probeCountX; x++) {
            for (uint32_t y = 0; y < m_probeCountY; y++) {
                for (uint32_t z = 0; z < m_probeCountZ; z++) {
                    glm::vec3 probePosition = GetProbeBaseWorldPosition(glm::ivec3(x, y, z));
                    Renderer::DrawPoint(probePosition, RED);
                }
            }
        }
    }
}

void DDGIVolume::UpdateMembers() {
    m_boundsMin = m_createInfo.origin - m_createInfo.extents * 0.5f;
    m_boundsMax = m_createInfo.origin + m_createInfo.extents * 0.5f;

    m_worldSpaceWidth = m_boundsMax.x - m_boundsMin.x;
    m_worldSpaceHeight = m_boundsMax.y - m_boundsMin.y;
    m_worldSpaceDepth = m_boundsMax.z - m_boundsMin.z;

    m_probeCountX = (int)std::ceil(m_worldSpaceWidth / m_createInfo.probeSpacing) + 1;
    m_probeCountY = (int)std::ceil(m_worldSpaceHeight / m_createInfo.probeSpacing) + 1;
    m_probeCountZ = (int)std::ceil(m_worldSpaceDepth / m_createInfo.probeSpacing) + 1;

    m_raytracingDataDirty = true;
}

void DDGIVolume::Init(const glm::vec3& aabbMin, const glm::vec3& aabbMax, float probeSpacing) {
    glm::vec3 inflatedAabbMin = aabbMin - glm::vec3(1.0f);
    glm::vec3 inflatedAabbMax = aabbMax + glm::vec3(1.0f);

    m_createInfo.origin = (inflatedAabbMin + inflatedAabbMax) * 0.5f;

    m_worldSpaceWidth = inflatedAabbMax.x - inflatedAabbMin.x;
    m_worldSpaceHeight = inflatedAabbMax.y - inflatedAabbMin.y;
    m_worldSpaceDepth = inflatedAabbMax.z - inflatedAabbMin.z;

    m_createInfo.probeSpacing = probeSpacing;
    m_probeCountX = (int)std::ceil(m_worldSpaceWidth / GetProbeSpacing()) + 1;
    m_probeCountY = (int)std::ceil(m_worldSpaceHeight / GetProbeSpacing()) + 1;
    m_probeCountZ = (int)std::ceil(m_worldSpaceDepth / GetProbeSpacing()) + 1;

    Logging::Debug() << "Created DDGIVolume " << aabbMin << " boundsMin " << aabbMin << " boundsMax\n";
}

uint32_t DDGIVolume::GetTotalProbeCount() const {
    return m_probeCountX * m_probeCountY * m_probeCountZ;
}

DDGIVolumeGPU DDGIVolume::GetGPUData() const {
    glm::vec3 halfExtents = glm::vec3(m_worldSpaceWidth, m_worldSpaceHeight, m_worldSpaceDepth) * 0.5f;

    DDGIVolumeGPU volume;
    volume.origin = GetOrigin();
    volume.probeSpacing = GetProbeSpacing();
    volume.probeCounts = glm::ivec3(m_probeCountX, m_probeCountY, m_probeCountZ);
    volume.numProbes = GetTotalProbeCount(); // sort this out, uint vs int
    volume.worldBoundsMin = GetOrigin() - halfExtents;
    volume.padding0 = 0;
    volume.worldBoundsMax = GetOrigin() + halfExtents;
    volume.padding1 = 0;

    return volume;
}

void DDGIVolume::SetEditorName(const std::string& name) {
    m_createInfo.editorName = name;
}

void DDGIVolume::SetOrigin(const glm::vec3& origin) {
    m_createInfo.origin = origin;
    UpdateMembers();
}

void DDGIVolume::SetRotation(const glm::vec3& rotation) {
    m_createInfo.rotation = rotation;
    UpdateMembers();
}

void DDGIVolume::SetExtents(const glm::vec3& extents) {
    m_createInfo.extents = extents;
    UpdateMembers();
}

void DDGIVolume::SetProbeSpacing(float spacing) {
    m_createInfo.probeSpacing = spacing;
    UpdateMembers();
}

glm::vec3 DDGIVolume::GetProbeBaseWorldPosition(const glm::ivec3& probeCoords) const {
    const glm::vec3 counts = glm::vec3(m_probeCountX, m_probeCountY, m_probeCountZ);
    const glm::vec3 coords = glm::vec3(probeCoords);
    return GetOrigin() + (coords - (counts - 1.0f) * 0.5f) * GetProbeSpacing();
}

const std::vector<BvhNode>& DDGIVolume::GetSceneNodes() {
    static std::vector<BvhNode> empty;
    if (m_sceneBvhId == 0) {
        return empty;
    }

    SceneBvh* sceneBvh = Bvh::Gpu::GetSceneBvhById(m_sceneBvhId);
    if (!sceneBvh) return empty;

    return sceneBvh->m_nodes;
}