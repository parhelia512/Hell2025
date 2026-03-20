#pragma once
#include "HellTypes.h"
#include <vector>
#include <glad/glad.h>

#include "LightVolume.h"

struct CloudPoint {
    glm::vec4 position = glm::vec4(0);
    glm::vec4 normal = glm::vec4(0);
    glm::vec4 directLighting = glm::vec4(0);
    glm::vec4 baseColor = glm::vec4(1.0);
};



struct PointCloudOctrant {
    unsigned int m_cloudPointCount;
    unsigned int m_offset;
};

namespace GlobalIllumination {
    void Update();
    void SetGlobalIlluminationStructuresDirtyState(bool state);
    void SetPointCloudNeedsGpuUpdateState(bool state);

    bool GlobalIlluminationStructuresAreDirty();
    bool PointCloudNeedsGpuUpdate();

    float GetProbeSpacing();
    float GetPointCloudSpacing();

    uint64_t GetSceneBvhId();
    const std::vector<BvhNode>& GetSceneNodes();
    std::vector<CloudPoint>& GetPointClound();
    std::vector<LightVolume>& GetLightVolumes();
    std::vector<PointCloudOctrant>& GetPointCloudOctrants();
    std::vector<unsigned int>& GetPointIndices();
    glm::uvec3 GetPointCloudGridDimensions();
    glm::vec3 GetPointGridWorldMin();
    glm::vec3 GetPointGridWorldMax();
}