#include "LightVolume.h"
#include "GlobalIllumination.h"

void LightVolume::Init(const glm::vec3& aabbMin, const glm::vec3& aabbMax) {
    glm::vec3 inflatedAabbMin = aabbMin - glm::vec3(1.0f);
    glm::vec3 inflatedAabbMax = aabbMax + glm::vec3(1.0f);

    m_offset = inflatedAabbMin;

    m_worldSpaceWidth = inflatedAabbMax.x - inflatedAabbMin.x;
    m_worldSpaceHeight = inflatedAabbMax.y - inflatedAabbMin.y;
    m_worldSpaceDepth = inflatedAabbMax.z - inflatedAabbMin.z;

    float spacing = GlobalIllumination::GetProbeSpacing();
    m_probeCountX = (int)std::ceil(m_worldSpaceWidth / spacing) + 1;
    m_probeCountY = (int)std::ceil(m_worldSpaceHeight / spacing) + 1;
    m_probeCountZ = (int)std::ceil(m_worldSpaceDepth / spacing) + 1;
}

uint32_t LightVolume::GetProbeCount() const {
    return m_probeCountX * m_probeCountY * m_probeCountZ;
}

uint32_t LightVolume::GetSHDataSize() const {
    const int SH_COEFFICIENTS = 9;
    const int FLOATS_PER_COEFFICIENT = 4;
    const int TOTAL_VALUES_PER_PROBE = SH_COEFFICIENTS * FLOATS_PER_COEFFICIENT;

    return GetProbeCount() * TOTAL_VALUES_PER_PROBE * sizeof(float);
}