#include "API/OpenGL/Renderer/GL_renderer.h"
#include "API/OpenGL/GL_backend.h"
#include "AssetManagement/AssetManager.h"
#include "Bvh/Gpu/Bvh.h"
#include "Renderer/RenderDataManager.h"
#include "GlobalIllumination/GlobalIllumination.h"
#include "Viewport/ViewportManager.h"
#include "World/World.h"

#include "Input/Input.h" // REMOVE ME!
#include "Hell/Logging.h"
#include "Util/Util.h"
#include "Renderer/Renderer.h"

namespace OpenGLRenderer {

    GLuint g_pointCloudVao = 0;
    GLuint g_pointCloudVbo = 0;

    void UploadPointCloud();
    void RaytraceScene();
    void ResizeLightVolumeSSBO();

    void UpdateGlobalIllumintation() {
        if (GlobalIllumination::PointCloudNeedsGpuUpdate()) {
            UploadPointCloud();
            ResizeLightVolumeSSBO();
        }

        OpenGLSSBO* triangleDataSSBO = GetSSBO("TriangleData");
        OpenGLSSBO* sceneBvhSSBO = GetSSBO("SceneBvh");
        OpenGLSSBO* meshesBvhSSBO = GetSSBO("MeshesBvh");
        OpenGLSSBO* entityInstancesSSBO = GetSSBO("EntityInstances");
        OpenGLSSBO* lightsSSBO = GetSSBO("Lights");

        OpenGLSSBO* pointGridBufferSSBO = GetSSBO("PointGridBuffer");
        OpenGLSSBO* pointIndicesBufferSSBO = GetSSBO("PointIndicesBuffer");
        
        uint64_t sceneBvhId = GlobalIllumination::GetSceneBvhId();

        const std::vector<BvhNode>& sceneNodes = GlobalIllumination::GetSceneNodes();
        const std::vector<BvhNode>& meshBvhNodes = Bvh::Gpu::GetMeshGpuBvhNodes();
        const std::vector<GpuPrimitiveInstance>& entityInstances = Bvh::Gpu::GetGpuEntityInstances(sceneBvhId);
        const std::vector<float>& triData = Bvh::Gpu::GetTriangleData();
        std::vector<PointCloudOctrant>& pointCloudOctrants = GlobalIllumination::GetPointCloudOctrants();
        std::vector<unsigned int>& pointIndices = GlobalIllumination::GetPointIndices();

        UpdateSSBO("SceneBvh", sceneNodes.size() * sizeof(BvhNode), &sceneNodes[0]);
        UpdateSSBO("MeshesBvh", meshBvhNodes.size() * sizeof(BvhNode), &meshBvhNodes[0]);
        UpdateSSBO("EntityInstances", entityInstances.size() * sizeof(GpuPrimitiveInstance), &entityInstances[0]);
        UpdateSSBO("TriangleData", triData.size() * sizeof(float), &triData[0]);
        UpdateSSBO("PointGridBuffer", pointCloudOctrants.size() * sizeof(PointCloudOctrant), &pointCloudOctrants[0]);
        UpdateSSBO("PointIndicesBuffer", pointIndices.size() * sizeof(unsigned int), &pointIndices[0]);

        //std::cout << "pointCloudOctrants.size(): " << pointCloudOctrants.size() << "\n";
        //std::cout << "pointIndices.size(): " << pointIndices.size() << "\n";

        BindSSBO("EntityInstances", 0);
        BindSSBO("TriangleData", 1);
        BindSSBO("SceneBvh", 2);
        BindSSBO("MeshesBvh", 3);
        BindSSBO("Lights", 4);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, g_pointCloudVbo);
        BindSSBO("SphericalHarmonics", 6);


        //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, pointGridBufferSSBO->GetHandle());    // did these ever work?
        //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, pointIndicesBufferSSBO->GetHandle()); // did these ever work?
    }

    void UploadPointCloud() {
        if (g_pointCloudVao == 0) {
            glGenVertexArrays(1, &g_pointCloudVao);
            glGenBuffers(1, &g_pointCloudVbo);
        }

        std::vector<CloudPoint>& pointCloud = GlobalIllumination::GetPointClound();
        std::vector<PointCloudOctrant>& pointCloudOctrants = GlobalIllumination::GetPointCloudOctrants();
        std::vector<unsigned int>& pointIndics = GlobalIllumination::GetPointIndices();

        // Point cloud
        glBindBuffer(GL_ARRAY_BUFFER, g_pointCloudVbo);
        glBindVertexArray(g_pointCloudVao);
        glBufferData(GL_ARRAY_BUFFER, sizeof(CloudPoint) * pointCloud.size(), pointCloud.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(CloudPoint), (void*)offsetof(CloudPoint, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CloudPoint), (void*)offsetof(CloudPoint, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(CloudPoint), (void*)offsetof(CloudPoint, directLighting));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(CloudPoint), (void*)offsetof(CloudPoint, baseColor));

        // Octrants
        //glGenBuffers(1, &g_pointGridBuffer);
        //glGenBuffers(1, &g_pointIndicesBuffer);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_pointGridBuffer);
        //glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(PointCloudOctrant) * pointCloudOctrants.size(), pointCloudOctrants.data(), GL_DYNAMIC_DRAW);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_pointIndicesBuffer);
        //glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * pointIndics.size(), pointIndics.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        GlobalIllumination::SetPointCloudNeedsGpuUpdateState(false);
        Logging::Debug() << "Uploaded point cloud to GPU (" << pointCloud.size() << " points)\n";
    }

    void PointCloudDirectLighting() {
        ProfilerOpenGLZoneFunction();

        OpenGLShader* shader = GetShader("PointCloudLighting");
        shader->Bind();
        shader->SetInt("u_lightCount", World::GetLightCount());

        GLuint numGroupsX = (GlobalIllumination::GetPointClound().size() + 127) / 128;

        glDispatchCompute(numGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

       
    }

    void RaytraceSceneIntoFinalLighting() {
        return;

        // This pass needs ssbos rebound to work correctly.

        OpenGLShader* shader = GetShader("RaytraceScene");
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
    
        if (!gBuffer) return;
        if (!shader) return;
    
        const std::vector<ViewportData>& viewportData = RenderDataManager::GetViewportData();
    
        shader->Bind();
        shader->SetMat4("u_projectionMatrix", viewportData[0].projection);
        shader->SetMat4("u_viewMatrix", viewportData[0].view);
    
        glBindImageTexture(0, gBuffer->GetColorAttachmentHandleByName("FinalLighting"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    
        glDispatchCompute(gBuffer->GetWidth() / 8, gBuffer->GetHeight() / 8, 1);
    }

    void DrawPointCloud() {
        if (g_pointCloudVao == 0) return;

        OpenGLShader* shader = GetShader("DebugPointCloud");
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");

        if (!gBuffer) return;
        if (!shader) return;

        const std::vector<ViewportData>& viewportData = RenderDataManager::GetViewportData();

        gBuffer->Bind();
        gBuffer->DrawBuffer("FinalLighting");

        shader->Bind();

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        std::vector<CloudPoint>& pointCloud = GlobalIllumination::GetPointClound();
        int pointCloudCount = pointCloud.size();

        //std::cout << "pointCloudCount: " << pointCloudCount << "\n";

        for (int i = 0; i < 4; i++) {
            Viewport* viewport = ViewportManager::GetViewportByIndex(i);
            if (!viewport->IsVisible()) continue;

            OpenGLRenderer::SetViewport(gBuffer, viewport);
            shader->SetInt("u_viewportIndex", i);
            shader->SetMat4("u_projectionView", viewportData[i].projectionView);

            glBindVertexArray(g_pointCloudVao);
            glDrawArrays(GL_POINTS, 0, pointCloudCount);
            glBindVertexArray(0);
        }
    }

    void DrawLightVolume() {

        //static bool renderProbes = false;
        static bool showMask = false;
        //if (Input::KeyPressed(HELL_KEY_Z)) {
        //    renderProbes = !renderProbes;
        //}
        //if (Input::KeyPressed(HELL_KEY_C)) {
        //    showMask = !showMask;
        //}
        //if (!renderProbes) return;

        OpenGLShader* shader = GetShader("DebugLightVolume");
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");

        shader->Bind();

        gBuffer->Bind();
        gBuffer->DrawBuffer("FinalLighting");

        BindSSBO("SphericalHarmonics", 6);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        glBindVertexArray(OpenGLBackEnd::GetVertexDataVAO());

        const std::vector<ViewportData>& viewportData = RenderDataManager::GetViewportData();
        Mesh* mesh = AssetManager::GetMeshByModelNameMeshIndex("Sphere", 0);

        for (int i = 0; i < 4; i++) {
            Viewport* viewport = ViewportManager::GetViewportByIndex(i);
            if (!viewport->IsVisible()) continue;

            OpenGLRenderer::SetViewport(gBuffer, viewport);
            shader->SetInt("u_viewportIndex", i);
            shader->SetMat4("u_projectionView", viewportData[i].projectionView);

            for (LightVolume& lightVolume : GlobalIllumination::GetLightVolumes()) {

                shader->SetFloat("u_spacing", GlobalIllumination::GetProbeSpacing());
                shader->SetVec3("u_offset", lightVolume.GetOffset());
                shader->SetInt("u_probeCount", lightVolume.GetProbeCount());
                shader->SetInt("u_probeCountX", lightVolume.GetProbeCountX());
                shader->SetInt("u_probeCountY", lightVolume.GetProbeCountY());
                shader->SetInt("u_probeCountZ", lightVolume.GetProbeCountZ());

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_3D, lightVolume.GetLightingTextureHandle());
                glBindImageTexture(1, lightVolume.GetMaskTextureHandle(), 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32UI);

                int instanceCount = lightVolume.GetProbeCount();

                glDrawElementsInstancedBaseVertex(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * mesh->baseIndex), instanceCount, mesh->baseVertex);
            }
        }
    }


    void LightProbeTest() {
        ProfilerOpenGLZoneFunction();

        std::vector<LightVolume>& lightVolumes = GlobalIllumination::GetLightVolumes();

        if (lightVolumes.size() == 0) {
            Logging::Fatal() << "ResizeLightVolumeSSBO() failed coz there were no light volumes\n";
        }
        if (lightVolumes.size() > 1) {
            Logging::Warning() << "ResizeLightVolumeSSBO() warnning: you have more than 1 light volume. Only the first will be used\n";
        }

        LightVolume& lightVolume = lightVolumes[0];
        std::vector<CloudPoint>& pointCloud = GlobalIllumination::GetPointClound();

        OpenGLShader* shader = GetShader("LightProbeTest");
        OpenGLSSBO* ssbo = GetSSBO("SphericalHarmonics");

        if (!shader) return;
        if (!ssbo) return;

        static int frameIndex = 0;
        frameIndex++;

        shader->Bind();
        shader->SetInt("u_probeCount", lightVolume.GetProbeCount());
        shader->SetVec3("u_gridOffset", lightVolume.GetOffset());
        shader->SetFloat("u_spacing", GlobalIllumination::GetProbeSpacing());
        shader->SetInt("u_gridWidth", lightVolume.GetProbeCountX());
        shader->SetInt("u_gridHeight", lightVolume.GetProbeCountY());
        shader->SetInt("u_gridDepth", lightVolume.GetProbeCountZ());
        shader->SetInt("u_pointCount", pointCloud.size());
        shader->SetInt("u_frameIndex", frameIndex);
        shader->SetFloat("u_pointCloudSpacing", GlobalIllumination::GetPointCloudSpacing());

        int groupCount = (lightVolume.GetProbeCount() + 63) / 64;
        glDispatchCompute(groupCount, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }


    void ResizeLightVolumeSSBO() {
        std::vector<LightVolume>& lightVolumes = GlobalIllumination::GetLightVolumes();

        if (lightVolumes.size() == 0) {
            Logging::Fatal() << "ResizeLightVolumeSSBO() failed coz there were no light volumes\n";
        }
        if (lightVolumes.size() > 1) {
            Logging::Warning() << "ResizeLightVolumeSSBO() warnning: you have more than 1 light volume. Only the first will be used\n";
        }

        LightVolume& lightVolume = lightVolumes[0];
        float bufferSize = lightVolume.GetSphericalHarmonicsSSBOSize();

        OpenGLSSBO* ssbo = GetSSBO("SphericalHarmonics");
        if (!ssbo) return;

        ssbo->PreAllocate(bufferSize);

        Logging::Debug() << "Resized SphericalHarmonics SSBO to " << bufferSize << " bytes\n";
    }


    void ComputeLightVolumeMask() {
        OpenGLShader* shader = GetShader("LightVolumeMask");
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");

        for (LightVolume& lightVolume : GlobalIllumination::GetLightVolumes()) {
            // Zero out the texture
            GLuint zero = 0;
            glClearTexImage(lightVolume.GetMaskTextureHandle(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

            glBindImageTexture(0, lightVolume.GetMaskTextureHandle(), 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, gBuffer->GetColorAttachmentHandleByName("WorldPosition"));
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, gBuffer->GetColorAttachmentHandleByName("Normal"));

            shader->Bind();
            shader->SetVec3("u_lightVolumeOffset", lightVolume.GetOffset());
            shader->SetFloat("u_lightVolumeSpacing", GlobalIllumination::GetProbeSpacing());

            int halfW = (gBuffer->GetWidth() + 1) / 2;
            int halfH = (gBuffer->GetHeight() + 1) / 2;
            int groupsX = (halfW + 8 - 1) / 8;
            int groupsY = (halfH + 8 - 1) / 8;
            glDispatchCompute(groupsX, groupsY, 1);

            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }
    }

    void ComputeProbeLighting() {

        for (LightVolume& lightVolume : GlobalIllumination::GetLightVolumes()) {

            static int frameIndex = -1;

            if (Input::KeyPressed(HELL_KEY_T)) {
                // Enable GI
                if (frameIndex == -1) {
                    frameIndex = 0;
                }
                // Disable GI
                else {
                    frameIndex = -1;
                    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    glClearTexImage(lightVolume.m_lightVolumeA, 0, GL_RGBA, GL_FLOAT, &clearColor);
                    glClearTexImage(lightVolume.m_lightVolumeB, 0, GL_RGBA, GL_FLOAT, &clearColor);
                }
            }

            if (frameIndex != -1) {

                // Lighting pass
                OpenGLShader* shader = GetShader("LightVolumeLighting");
                shader->Bind();
                shader->SetInt("u_width", lightVolume.GetProbeCountX());
                shader->SetInt("u_height", lightVolume.GetProbeCountY());
                shader->SetInt("u_depth", lightVolume.GetProbeCountZ());
                shader->SetFloat("u_spacing", GlobalIllumination::GetProbeSpacing());
                shader->SetVec3("u_offset", lightVolume.GetOffset());
                shader->SetFloat("u_bounceRange", 5.0f);
                shader->SetInt("u_frameIndex", frameIndex);

                // Set point grid uniforms
                shader->SetUVec3("u_pointGridDimensions", GlobalIllumination::GetPointCloudGridDimensions());
                shader->SetVec3("u_pointGridWorldMin", GlobalIllumination::GetPointGridWorldMin());
                shader->SetVec3("u_pointGridCellSize", GlobalIllumination::GetPointGridWorldMax());

                glBindImageTexture(0, lightVolume.m_lightVolumeTextures[lightVolume.m_pingPongReadIndex], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);
                glBindImageTexture(1, lightVolume.m_lightVolumeTextures[lightVolume.m_pingPongWriteIndex], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
                glBindImageTexture(2, lightVolume.m_lightVolumeMaskTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32UI);

                glDispatchCompute((lightVolume.GetProbeCountX() + 8 - 1) / 8, (lightVolume.GetProbeCountY() + 8 - 1) / 8, (lightVolume.GetProbeCountZ() + 8 - 1) / 8);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                // Ping Pong Swap!
                std::swap(lightVolume.m_pingPongReadIndex, lightVolume.m_pingPongWriteIndex);

                frameIndex++;
                if (frameIndex == 4) {
                    frameIndex = 0;
                }
            }
        }
    }

    void DrawGPUBvhSceneNodes(const glm::vec4& color) {
        const std::vector<BvhNode>& sceneNodes = GlobalIllumination::GetSceneNodes();

        for (const BvhNode& node : sceneNodes) {
            AABB aabb(node.boundsMin, node.boundsMax);
            Renderer::DrawAABB(aabb, color);
        }
    }

    void DrawGPUBvhSceneLeafNodes(const glm::vec4& color) {
        const std::vector<BvhNode>& sceneNodes = GlobalIllumination::GetSceneNodes();

        for (const BvhNode& node : sceneNodes) {
            if (node.primitiveCount > 0) {
                AABB aabb(node.boundsMin, node.boundsMax);
                Renderer::DrawAABB(aabb, color);
            }
        }
    }

    void DrawRaytracingBvh() {
        const std::vector<BvhNode>& sceneNodes = GlobalIllumination::GetSceneNodes();
        const std::vector<BvhNode>& meshBvhNodes = Bvh::Gpu::GetMeshGpuBvhNodes();
        const std::vector<float>& triData = Bvh::Gpu::GetTriangleData();

        uint64_t sceneBvhId = GlobalIllumination::GetSceneBvhId();
        const std::vector<GpuPrimitiveInstance>& instances = Bvh::Gpu::GetGpuEntityInstances(sceneBvhId);

        if (sceneNodes.empty()) return;

        uint32_t sceneStack[32];
        uint32_t sceneStackSize = 0;

        // push scene root node
        sceneStack[sceneStackSize++] = 0;

        // walk scene bvh
        while (sceneStackSize > 0) {
            uint32_t sceneNodeIndex = sceneStack[--sceneStackSize];
            const BvhNode& sceneNode = sceneNodes[sceneNodeIndex];

            if (sceneNode.primitiveCount > 0) {
                // walk instances in scene leaf node
                for (uint32_t i = 0; i < sceneNode.primitiveCount; ++i) {
                    uint32_t instanceIdx = sceneNode.firstChildOrPrimitive + i;
                    const GpuPrimitiveInstance& instance = instances[instanceIdx];

                    // skip house
                    // if (instance.rootNodeIndex == 0) continue;

                    uint32_t meshStack[32];
                    uint32_t meshStackSize = 0;

                    // push mesh root node
                    meshStack[meshStackSize++] = instance.rootNodeIndex;

                    // walk mesh bvh
                    while (meshStackSize > 0) {
                        uint32_t meshNodeIndex = meshStack[--meshStackSize];
                        const BvhNode& meshNode = meshBvhNodes[meshNodeIndex];

                        if (meshNode.primitiveCount > 0) {
                            // draw triangles in mesh leaf node
                            for (uint32_t j = 0; j < meshNode.primitiveCount; ++j) {
                                uint32_t floatIdx = meshNode.firstChildOrPrimitive + (j * 12);

                                glm::vec3 p0(triData[floatIdx], triData[floatIdx + 1], triData[floatIdx + 2]);
                                glm::vec3 e1(triData[floatIdx + 3], triData[floatIdx + 4], triData[floatIdx + 5]);
                                glm::vec3 e2(triData[floatIdx + 6], triData[floatIdx + 7], triData[floatIdx + 8]);

                                glm::vec3 p1 = p0 - e1;
                                glm::vec3 p2 = p0 + e2;

                                glm::vec3 worldP0 = instance.worldTransform * glm::vec4(p0, 1.0f);
                                glm::vec3 worldP1 = instance.worldTransform * glm::vec4(p1, 1.0f);
                                glm::vec3 worldP2 = instance.worldTransform * glm::vec4(p2, 1.0f);

                                Renderer::DrawLine(worldP0, worldP1, WHITE);
                                Renderer::DrawLine(worldP1, worldP2, WHITE);
                                Renderer::DrawLine(worldP2, worldP0, WHITE);
                            }
                        }
                        else {
                            // push internal mesh children
                            meshStack[meshStackSize++] = meshNode.firstChildOrPrimitive;
                            meshStack[meshStackSize++] = meshNode.firstChildOrPrimitive + 1;
                        }
                    }
                }
            }
            else {
                // push internal scene children
                sceneStack[sceneStackSize++] = sceneNode.firstChildOrPrimitive;
                sceneStack[sceneStackSize++] = sceneNode.firstChildOrPrimitive + 1;
            }
        }
    }
}