#pragma once
#include "HellTypes.h"

struct LightVolume {
    void Init(const glm::vec3& aabbMin, const glm::vec3& aabbMax);

    uint32_t GetTotalProbeCount() const;
    DDGIVolumeGPU GetGPUData() const;
        
    const glm::vec3& GetOrigin() const { return m_origin; }
    float GetWorldSpaceWidth() const   { return m_worldSpaceWidth; }
    float GetWorldSpaceHeight() const  { return m_worldSpaceHeight; }
    float GetWorldSpaceDepth() const   { return m_worldSpaceDepth; }
    int GetProbeCountX() const         { return m_probeCountX; }
    int GetProbeCountY() const         { return m_probeCountY; }
    int GetProbeCountZ() const         { return m_probeCountZ; }

private:
    glm::vec3 m_origin = glm::vec3(0.0f);
    float m_worldSpaceWidth = 0.0f;
    float m_worldSpaceHeight = 0.0f;
    float m_worldSpaceDepth = 0.0f;
    int m_probeCountX = 0;
    int m_probeCountY = 0;
    int m_probeCountZ = 0;
};