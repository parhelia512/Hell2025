#include "PointCloud.h"
#include "Util.h"

// TODO: Move me Util.h and add division by zero checks
float RoundUp(float value, float spacing) { return std::ceil(value / spacing) * spacing; }
float RoundDown(float value, float spacing) { return std::floor(value / spacing) * spacing; }

void PointCloud::Create(const std::vector<Triangle>& triangles, float pointCloudSpacing) {
    m_points.clear();
    m_textureInfo.clear();

    for (const Triangle& triangle : triangles) {
        // Choose the up vector based on the normal
        glm::vec3 up = (std::abs(triangle.normal.z) > 0.999f) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);

        // Calculate right and up vectors
        glm::vec3 right = glm::normalize(glm::cross(up, triangle.normal));
        up = glm::cross(triangle.normal, right);

        glm::vec2 v0_2d(glm::dot(right, triangle.v0), glm::dot(up, triangle.v0));
        glm::vec2 v1_2d(glm::dot(right, triangle.v1), glm::dot(up, triangle.v1));
        glm::vec2 v2_2d(glm::dot(right, triangle.v2), glm::dot(up, triangle.v2));

        // Determine the bounding box of the 2D triangle
        glm::vec2 min = glm::min(glm::min(v0_2d, v1_2d), v2_2d);
        glm::vec2 max = glm::max(glm::max(v0_2d, v1_2d), v2_2d);

        // Round min and max values
        min.x = RoundDown(min.x, pointCloudSpacing) - pointCloudSpacing * 0.5f;
        min.y = RoundDown(min.y, pointCloudSpacing) - pointCloudSpacing * 0.5f;
        max.x = RoundUp(max.x, pointCloudSpacing) + pointCloudSpacing * 0.5f;
        max.y = RoundUp(max.y, pointCloudSpacing) + pointCloudSpacing * 0.5f;

        float theshold = 0.05f;
        min.x += theshold;
        min.y += theshold;
        max.x -= theshold;
        max.y -= theshold;

        // Generate points within the bounding box
        for (float x = min.x; x <= max.x; x += pointCloudSpacing) {
            for (float y = min.y; y <= max.y; y += pointCloudSpacing) {
                glm::vec2 pt(x, y);
                if (Util::IsPointInTriangle2D(pt, v0_2d, v1_2d, v2_2d)) {
                    glm::vec3 pt3d = triangle.v0 + right * (pt.x - v0_2d.x) + up * (pt.y - v0_2d.y);

                    CloudPoint& cloudPoint = m_points.emplace_back();
                    cloudPoint.position = glm::vec4(pt3d, 0.0f);
                    cloudPoint.normal = glm::vec4(triangle.normal, 0.0f);

                    // Calculate uv via barycentrics
                    glm::vec3 bary = Util::GetBarycentric(pt, v0_2d, v1_2d, v2_2d);
                    glm::vec2 uv = bary.x * triangle.uv0 + bary.y * triangle.uv1 + bary.z * triangle.uv2;

                    CloudPointTextureInfo& cloudPointTextureInfo = m_textureInfo.emplace_back();
                    cloudPointTextureInfo.u = uv.x;
                    cloudPointTextureInfo.v = uv.y;
                    cloudPointTextureInfo.baseColorIndex = triangle.baseColorTextureIndex;
                    cloudPointTextureInfo.rmaIndex = triangle.rmaTextureIndex;
                }
            }
        }
    }
}

void PointCloud::CleanUp() {
    m_points.clear();
    m_textureInfo.clear();
}
