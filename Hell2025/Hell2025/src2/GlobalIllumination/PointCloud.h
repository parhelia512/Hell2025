#pragma once
#include <Hell/Types.h>
#include <vector>

struct Triangle {
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;
    glm::vec2 uv0;
    glm::vec2 uv1;
    glm::vec2 uv2;
    glm::vec3 normal;
    int baseColorTextureIndex;
    int rmaTextureIndex;
};

struct CloudPoint {
    glm::vec4 position = glm::vec4(0);
    glm::vec4 normal = glm::vec4(0);
    glm::vec4 directLighting = glm::vec4(0);
    glm::vec4 baseColor = glm::vec4(1.0);
};

struct CloudPointTextureInfo {
    float u;
    float v;
    int baseColorIndex;
    int rmaIndex;
};

struct PointCloud {
    void Create(const std::vector<Triangle>& triangles, float pointCloudSpacing);
    void CleanUp();

    const std::vector<CloudPoint>& GetPoints() const                 { return m_points; }
    const std::vector<CloudPointTextureInfo>& GetTextureInfo() const { return m_textureInfo; }
    uint32_t GetPointCount() const                                   { return (uint32_t)m_points.size(); }

private:
    std::vector<CloudPoint> m_points;
    std::vector<CloudPointTextureInfo> m_textureInfo;
};