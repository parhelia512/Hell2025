#pragma once
#include "HellTypes.h"
#include <glad/glad.h>

struct LightVolume {
    void Init(const std::vector<Vertex>& vertices, const glm::vec3& aabbMin, const glm::vec3& aabbMax);
    void CleanUp();

    uint32_t GetProbeCount() const;
    uint32_t GetSphericalHarmonicsSSBOSize() const;

    const glm::vec3& GetOffset() const { return m_offset; }
    float GetWorldSpaceWidth() const   { return m_worldSpaceWidth; }
    float GetWorldSpaceHeight() const  { return m_worldSpaceHeight; }
    float GetWorldSpaceDepth() const   { return m_worldSpaceDepth; }
    int GetProbeCountX() const         { return m_probeCountX; }
    int GetProbeCountY() const         { return m_probeCountY; }
    int GetProbeCountZ() const         { return m_probeCountZ; }


private:
    glm::vec3 m_offset = glm::vec3(0.0f);
    float m_worldSpaceWidth = 0.0f;
    float m_worldSpaceHeight = 0.0f;
    float m_worldSpaceDepth = 0.0f;
    int m_probeCountX = 0;
    int m_probeCountY = 0;
    int m_probeCountZ = 0;

    // OpenGL only (Figure out a way to make this more Vulkan friendly later!)
    // Get me out of here
public:
    GLuint m_lightVolumeTextures[2];
    GLuint m_lightVolumeA = 0;
    GLuint m_lightVolumeB = 0;
    GLuint m_lightVolumeMaskTexture = 0;
    int m_pingPongReadIndex = 0;
    int m_pingPongWriteIndex = 1;

    GLuint GetLightingTextureHandle();
    GLuint GetMaskTextureHandle();
};