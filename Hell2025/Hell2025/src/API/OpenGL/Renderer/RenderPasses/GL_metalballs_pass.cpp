#include "API/OpenGL/Renderer/GL_renderer.h"
#include "API/OpenGL/GL_backend.h"
#include "AssetManagement/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderDataManager.h"
#include "Viewport/ViewportManager.h"

#include "HellLogging.h"

namespace OpenGLRenderer {

    float Hash3(int x, int y, int z, int period) {
        x = (x % period + period) % period;
        y = (y % period + period) % period;
        z = (z % period + period) % period;

        int n = x + y * 57 + z * 113;
        n = (n << 13) ^ n;
        return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    }

    float TilingValueNoise3D(float x, float y, float z, int period) {
        int ix = (int)std::floor(x);
        int iy = (int)std::floor(y);
        int iz = (int)std::floor(z);

        float fx = x - ix;
        float fy = y - iy;
        float fz = z - iz;

        float u = fx * fx * (3.0f - 2.0f * fx);
        float v = fy * fy * (3.0f - 2.0f * fy);
        float w = fz * fz * (3.0f - 2.0f * fz);

        // Sample eight corners of the surrounding cell
        float c000 = Hash3(ix, iy, iz, period);
        float c100 = Hash3(ix + 1, iy, iz, period);
        float c010 = Hash3(ix, iy + 1, iz, period);
        float c110 = Hash3(ix + 1, iy + 1, iz, period);
        float c001 = Hash3(ix, iy, iz + 1, period);
        float c101 = Hash3(ix + 1, iy, iz + 1, period);
        float c011 = Hash3(ix, iy + 1, iz + 1, period);
        float c111 = Hash3(ix + 1, iy + 1, iz + 1, period);

        // Interpolate along x
        float r00 = c000 * (1.0f - u) + c100 * u;
        float r10 = c010 * (1.0f - u) + c110 * u;
        float r01 = c001 * (1.0f - u) + c101 * u;
        float r11 = c011 * (1.0f - u) + c111 * u;

        // Interpolate along y
        float r0 = r00 * (1.0f - v) + r10 * v;
        float r1 = r01 * (1.0f - v) + r11 * v;

        // Interpolate along z
        return r0 * (1.0f - w) + r1 * w;
    }

    GLuint CreateTiling3DNoiseTexture(int resolution = 64) {
        std::vector<unsigned char> data(resolution * resolution * resolution);

        // Adjust frequency to set noise density within the volume
        int period = 4;

        for (int z = 0; z < resolution; ++z) {
            for (int y = 0; y < resolution; ++y) {
                for (int x = 0; x < resolution; ++x) {
                    float px = (float)x / resolution * period;
                    float py = (float)y / resolution * period;
                    float pz = (float)z / resolution * period;

                    float noise = TilingValueNoise3D(px, py, pz, period);

                    // Normalize float to unsigned byte range
                    unsigned char val = (unsigned char)((noise * 0.5f + 0.5f) * 255.0f);

                    int index = x + y * resolution + z * resolution * resolution;
                    data[index] = val;
                }
            }
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_3D, textureID);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, resolution, resolution, resolution, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());

        return textureID;
    }

    struct GPUMetaBall {
        glm::vec3 m_position = glm::vec3(0.0f);
        float m_invSigma2 = 0;
    };

    struct MetaBall {
        MetaBall() = default;

        MetaBall(const glm::vec3& position, float radius) {
            m_position = position;
            m_radius = radius;
            m_invSigma2 = 1.0f / std::max(radius * radius, 1e-8f);
            m_stepSize = std::max(m_radius * 0.25f, 1e-4f);
            m_proxyRadius = m_radius * m_proxyScale;
        }

        void Update() {
            m_proxyModelMatrix = glm::translate(glm::mat4(1.0f), m_position) * glm::scale(glm::mat4(1.0f), glm::vec3(m_proxyRadius));

            m_gpuMetaBall.m_position = m_position;
            m_gpuMetaBall.m_invSigma2 = m_invSigma2;
        }

        void DebugDraw() {
            DrawSphere(m_position, m_proxyRadius, YELLOW);
        }

        float GetStepSize() const                      { return m_stepSize; }
        float GetProxyRadius() const                   { return m_proxyRadius; }
        const GPUMetaBall GetGPUMetaBall() const       { return m_gpuMetaBall; }
        const glm::mat4& GetProxyModelMatrix() const   { return m_proxyModelMatrix; }

    private:
        glm::vec3 m_position = glm::vec3(0.0f);
        glm::mat4 m_proxyModelMatrix = glm::mat4(1.0f);
        float m_invSigma2 = 0;
        float m_radius = 0;
        float m_proxyRadius = 0;
        float m_stepSize = 0;
        GPUMetaBall m_gpuMetaBall;
        const float m_proxyScale = 1.125f;
    };

    void MetaBallsPass() {
        ProfilerOpenGLZoneFunction();

        std::vector<MetaBall> metaBalls;
        metaBalls.emplace_back(MetaBall(glm::vec3(36.0f, 32.00f, 36.00f), 0.5f));
        metaBalls.emplace_back(MetaBall(glm::vec3(36.5f, 32.75f, 36.25f), 0.5f));
        metaBalls.emplace_back(MetaBall(glm::vec3(37.0f, 32.25f, 37.00f), 0.5f));

        for (MetaBall& metaBall : metaBalls) {
            metaBall.Update();
            //metaBall.DebugDraw();
        }

        const std::vector<ViewportData>& viewportData = RenderDataManager::GetViewportData();

        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* shader = GetShader("MetaBalls");
        Mesh* mesh = AssetManager::GetMeshByModelNameMeshIndex("SphereLowRes", 0);

        if (!gBuffer) return;
        if (!shader) return;
        if (!mesh) return;

        gBuffer->Bind();
        gBuffer->DrawBuffers({ "BaseColor", "Normal", "RMA", "WorldPosition", "Emissive" });


        std::vector<GPUMetaBall> gpuMetaBalls;

        for (MetaBall& metaBall : metaBalls) {
            gpuMetaBalls.emplace_back(metaBall.GetGPUMetaBall());
        }

        UpdateSSBO("MetaBalls", metaBalls.size() * sizeof(GPUMetaBall), gpuMetaBalls.data());
        BindSSBO("MetaBalls", 5);


        SetRasterizerState("GeometryPass_Default");

        shader->Bind();
        shader->SetInt("u_metaBallCount", metaBalls.size());

        static GLuint noiseTexture = 0;
        if (noiseTexture == 0) noiseTexture = CreateTiling3DNoiseTexture(64);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, noiseTexture);

        //glDisable(GL_CULL_FACE);

        for (int i = 0; i < 4; i++) {
            Viewport* viewport = ViewportManager::GetViewportByIndex(i);
            if (!viewport->IsVisible()) continue;

            OpenGLRenderer::SetViewport(gBuffer, viewport);

            shader->SetMat4("u_projectionView", viewportData[i].projectionView);
            
            for (MetaBall& metaBall : metaBalls) {
                shader->SetFloat("u_stepSize", metaBall.GetStepSize());
                shader->SetMat4("u_model", metaBall.GetProxyModelMatrix());

                glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, (GLvoid*)(mesh->baseIndex * sizeof(GLuint)), 1, mesh->baseVertex, i);
            }
        }
    }
}