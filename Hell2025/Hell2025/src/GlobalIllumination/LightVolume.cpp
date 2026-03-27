#include "LightVolume.h"
#include "GlobalIllumination.h"

void LightVolume::Init(const glm::vec3& aabbMin, const glm::vec3& aabbMax) {
    glm::vec3 inflatedAabbMin = aabbMin - glm::vec3(1.0f);
    glm::vec3 inflatedAabbMax = aabbMax + glm::vec3(1.0f);

    m_origin = inflatedAabbMin;

    m_worldSpaceWidth = inflatedAabbMax.x - inflatedAabbMin.x;
    m_worldSpaceHeight = inflatedAabbMax.y - inflatedAabbMin.y;
    m_worldSpaceDepth = inflatedAabbMax.z - inflatedAabbMin.z;

    float spacing = GlobalIllumination::GetProbeSpacing();
    m_probeCountX = (int)std::ceil(m_worldSpaceWidth / spacing) + 1;
    m_probeCountY = (int)std::ceil(m_worldSpaceHeight / spacing) + 1;
    m_probeCountZ = (int)std::ceil(m_worldSpaceDepth / spacing) + 1;
}

uint32_t LightVolume::GetTotalProbeCount() const {
    return m_probeCountX * m_probeCountY * m_probeCountZ;
}

DDGIVolumeGPU LightVolume::GetGPUData() const {
    DDGIVolumeGPU volume;
    volume.origin = m_origin;
    volume.probeSpacing = GlobalIllumination::GetProbeSpacing();
    volume.probeCounts = glm::ivec3(m_probeCountX, m_probeCountY, m_probeCountZ);
    volume.totalProbes = GetTotalProbeCount();
    volume.worldBoundsMin = m_origin; // FIX ME WHEN YOU CENTER YOUR ORIGIN
    volume.padding0 = 0;
    volume.worldBoundsMax = m_origin + glm::vec3(m_worldSpaceWidth, m_worldSpaceHeight, m_worldSpaceDepth); // AND FIX ME TOO
    volume.padding1 = 0;
    return volume;
}