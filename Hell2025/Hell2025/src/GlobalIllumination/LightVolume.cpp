#include "LightVolume.h"
#include "GlobalIllumination.h"

void LightVolume::Init(const std::vector<Vertex>& vertices, const glm::vec3& aabbMin, const glm::vec3& aabbMax) {
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

    // Create the 3d textures
    glGenTextures(1, &m_lightVolumeA);
    glBindTexture(GL_TEXTURE_3D, m_lightVolumeA);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, m_probeCountX, m_probeCountY, m_probeCountZ, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &m_lightVolumeB);
    glBindTexture(GL_TEXTURE_3D, m_lightVolumeB);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, m_probeCountX, m_probeCountY, m_probeCountZ, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    m_lightVolumeTextures[0] = m_lightVolumeA;
    m_lightVolumeTextures[1] = m_lightVolumeB;

    glGenTextures(1, &m_lightVolumeMaskTexture);
    glBindTexture(GL_TEXTURE_3D, m_lightVolumeMaskTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32UI, m_probeCountX, m_probeCountY, m_probeCountZ, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);
}

uint32_t LightVolume::GetProbeCount() const {
    return m_probeCountX * m_probeCountY * m_probeCountZ;
}

uint32_t LightVolume::GetSphericalHarmonicsSSBOSize() const {
    const int SH_COEFFICIENTS = 9;
    const int FLOATS_PER_COEFFICIENT = 4;
    const int TOTAL_VALUES_PER_PROBE = SH_COEFFICIENTS * FLOATS_PER_COEFFICIENT;

    // return the actual byte size needed for the buffer allocation
    return GetProbeCount() * TOTAL_VALUES_PER_PROBE * sizeof(float);
}

void LightVolume::CleanUp() {
    glDeleteTextures(1, &m_lightVolumeA);
    glDeleteTextures(1, &m_lightVolumeB);
    glDeleteTextures(1, &m_lightVolumeMaskTexture);
}

GLuint LightVolume::GetLightingTextureHandle() {
    return m_lightVolumeTextures[m_pingPongReadIndex];
}

GLuint LightVolume::GetMaskTextureHandle() {
    return m_lightVolumeMaskTexture;
}
