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
    OpenGLTextureArray g_probeDistanceTextureArray;

    void UploadPointCloud();
    void ComputePointCloudBaseColor();
    void ComputeProbeLightingIndexed();
    void ComputeProbeDistance();
    void ComputeProbeDistanceBorder();
    void UpdateDistanceTextureSize();

    OpenGLTextureArray& GetProbeDistanceTextureArray() {
        return g_probeDistanceTextureArray;
    }

    void UpdateGlobalIllumintation() {
        if (GlobalIllumination::PointCloudNeedsGpuUpdate()) {
            UploadPointCloud();
            ComputePointCloudBaseColor();
        }

        LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();

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

        ReserveSSBO("ProbeSHColor", sizeof(ProbeColor) * lightVolume.GetProbeCount());
        ReserveSSBO("ProbeSHDistance", sizeof(ProbeDistance) * lightVolume.GetProbeCount());
        ReserveSSBO("ProbeVisibility", sizeof(uint32_t) * lightVolume.GetProbeCount());
        ReserveSSBO("ProbeVisibilityIndices", sizeof(uint32_t) * lightVolume.GetProbeCount());
        ReserveSSBO("ProbeState", sizeof(uint32_t) * lightVolume.GetProbeCount()); 
        
        BindSSBO("EntityInstances", 0);
        BindSSBO("TriangleData", 1);
        BindSSBO("SceneBvh", 2);
        BindSSBO("MeshesBvh", 3);
   
        ComputePointCloudLighting();
        ComputeProbeLightingIndexed();
        UpdateDistanceTextureSize();
        ComputeProbeDistance();
        ComputeProbeDistanceBorder();
    }


	void ComputeProbeVisibility() {
		ProfilerOpenGLZoneFunction();

		OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* visibilityShader = GetShader("ProbeVisibility");
        OpenGLShader* listShader = GetShader("ProbeVisibilityList");
        OpenGLShader* argsShader = GetShader("ProbeLightingDispatchArgs");

		if (!gBuffer) return;
        if (!visibilityShader) return;
        if (!listShader) return; 
        if (!argsShader) return;

        LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();

        ClearSSBO("ProbeVisibility");
        ClearSSBO("ProbeVisibilityCounter");
        
        BindSSBO("ProbeVisibility", 4);
        BindSSBO("ProbeVisibilityCounter", 5);
        BindSSBO("ProbeVisibilityIndices", 6);
        BindSSBO("ProbeDispatchArgs", 7);

        // Iterate each pixel, and mark and probes that influence it as visible
		visibilityShader->Bind();
		visibilityShader->BindTextureUnit(0, gBuffer->GetDepthAttachmentHandle());
		visibilityShader->BindTextureUnit(1, gBuffer->GetColorAttachmentHandleByName("WorldPosition"));
		visibilityShader->BindTextureUnit(2, gBuffer->GetColorAttachmentHandleByName("Normal"));

        visibilityShader->SetInt("u_probeCount", lightVolume.GetProbeCount());
		visibilityShader->SetInt("u_probeCountX", lightVolume.GetProbeCountX());
		visibilityShader->SetInt("u_probeCountY", lightVolume.GetProbeCountY());
		visibilityShader->SetInt("u_probeCountZ", lightVolume.GetProbeCountZ());
		visibilityShader->SetVec3("u_probeOffset", lightVolume.GetOffset());
		visibilityShader->SetFloat("u_probeSpacing", GlobalIllumination::GetProbeSpacing());

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glDispatchCompute((gBuffer->GetWidth() + 7) / 8, (gBuffer->GetHeight() + 7) / 8, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        listShader->Bind();
        listShader->SetInt("u_probeCount", lightVolume.GetProbeCount());

        glDispatchCompute((lightVolume.GetProbeCount() + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        argsShader->Bind();
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
    }


	void ComputePointCloudBaseColor() {
		// TODO:
		// If RenderDoc was detected then you gotta do this differently
		if (BackEnd::RenderDocFound()) {
			Logging::Fatal() << "GlobalIllumination::CalculatePointCloudBaseColor() does not work without BIndless yet you fool.\n";
            return;
		}

		std::vector<CloudPointTextureInfo>& data = GlobalIllumination::GetPointCloudTextureInfo();
		
        OpenGLShader* shader = GetShader("PointCloudBaseColor");
        if (!shader) return;

        UpdateSSBO("PointCloudTextureInfo", data.size() * sizeof(CloudPointTextureInfo), data.data());

        // Ensure bindless texture IDs are in the Samplers ID, which is not the case if this runs the first frame of the renderer
        UpdateSSBO("Samplers", sizeof(GLuint64) * OpenGLBackEnd::GetBindlessTextureIDs().size(), OpenGLBackEnd::GetBindlessTextureIDs().data());

		BindSSBO("Samplers", 0);
		BindSSBO("PointCloudTextureInfo", 1);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_pointCloudVbo);

		GLuint numGroupsX = (GlobalIllumination::GetPointClound().size() + 127) / 128;

		shader->Bind();
        shader->DispatchCompute(numGroupsX, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
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

    void ComputePointCloudLighting() {
        ProfilerOpenGLZoneFunction();

        OpenGLShader* shader = GetShader("PointCloudLighting");
        shader->Bind();
        shader->SetInt("u_lightCount", World::GetLightCount());

        BindSSBO("Lights", 4);
        BindSSBO(g_pointCloudVbo, 5);

        GLuint numGroupsX = (GlobalIllumination::GetPointClound().size() + 127) / 128;

        glDispatchCompute(numGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

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

    void DrawProbes() {
		LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* shader = GetShader("DebugProbes");

        if (!gBuffer) return;
        if (!shader) return;

        shader->Bind();

        gBuffer->Bind();
        gBuffer->DrawBuffer("FinalLighting");

        BindSSBO("ProbeSHColor", 6);
        BindSSBO("ProbeSHDistance", 7);
        BindSSBO("ProbeVisibility", 8);

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
			shader->SetFloat("u_spacing", GlobalIllumination::GetProbeSpacing());
			shader->SetVec3("u_offset", lightVolume.GetOffset());
			shader->SetInt("u_probeCount", lightVolume.GetProbeCount());
			shader->SetInt("u_probeCountX", lightVolume.GetProbeCountX());
			shader->SetInt("u_probeCountY", lightVolume.GetProbeCountY());
			shader->SetInt("u_probeCountZ", lightVolume.GetProbeCountZ());

            /// fix me
            shader->SetInt("u_gridWidth", lightVolume.GetProbeCountX());
            shader->SetInt("u_gridHeight", lightVolume.GetProbeCountY());
            shader->SetInt("u_gridDepth", lightVolume.GetProbeCountZ());
            shader->SetVec3("u_gridOffset", lightVolume.GetOffset());

            OpenGLTextureArray& probeDistanceTexture = GetProbeDistanceTextureArray();
            shader->BindTextureUnit(0, probeDistanceTexture.GetHandle());

			glDrawElementsInstancedBaseVertex(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * mesh->baseIndex), lightVolume.GetProbeCount(), mesh->baseVertex);
        }
    }


    void ComputeProbeDistance() {
        ProfilerOpenGLZoneFunction();

		LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();

        OpenGLShader* shader = GetShader("ProbeDistance");
        if (!shader) return;

        static int frameIndex = 0;
        frameIndex++;

        //BindSSBO("ProbeSHDistance", 5);
        //BindSSBO("ProbeVisibilityCounter", 6);
        //BindSSBO("ProbeVisibilityIndices", 7);
        BindSSBO("ProbeState", 6);

        shader->Bind();
        shader->SetInt("u_probeCount", lightVolume.GetProbeCount());
        shader->SetVec3("u_gridOffset", lightVolume.GetOffset());
        shader->SetInt("u_gridWidth", lightVolume.GetProbeCountX());
        shader->SetInt("u_gridHeight", lightVolume.GetProbeCountY());
        shader->SetInt("u_gridDepth", lightVolume.GetProbeCountZ());
        shader->SetInt("u_frameIndex", frameIndex);
        shader->SetFloat("u_spacing", GlobalIllumination::GetProbeSpacing());
        shader->SetFloat("u_pointCloudSpacing", GlobalIllumination::GetPointCloudSpacing());

        OpenGLTextureArray& probeDistanceTexture = GetProbeDistanceTextureArray();
        shader->BindImageTextureArray(0, probeDistanceTexture.GetHandle(), GL_READ_WRITE, GL_RG16F);

        int groupCount = (lightVolume.GetProbeCount() + 63) / 64;
        glDispatchCompute(groupCount, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    void ComputeProbeDistanceBorder() {
        ProfilerOpenGLZoneFunction();

        LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();

        OpenGLShader* shader = GetShader("ProbeDistanceBorder");
        if (!shader) return;

        // wait for the interior ray trace image writes to finish
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        shader->Bind();
        shader->SetInt("u_probeCount", lightVolume.GetProbeCount());
        shader->SetInt("u_gridWidth", lightVolume.GetProbeCountX());
        shader->SetInt("u_gridHeight", lightVolume.GetProbeCountY());
        shader->SetInt("u_gridDepth", lightVolume.GetProbeCountZ());

        // bind the distance atlas for both reading the interior and writing the borders
        OpenGLTextureArray& probeDistanceTexture = GetProbeDistanceTextureArray();
        shader->BindImageTextureArray(0, probeDistanceTexture.GetHandle(), GL_READ_WRITE, GL_RG16F);

        // dispatch one thread per probe
        int groupCount = (lightVolume.GetProbeCount() + 63) / 64;
        glDispatchCompute(groupCount, 1, 1);

        // ensure all texture updates are fully resolved before the lighting pass samples it
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void ComputeProbeLightingIndexed() {
        ProfilerOpenGLZoneFunction();

        LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();
        std::vector<CloudPoint>& pointCloud = GlobalIllumination::GetPointClound();

        OpenGLShader* shader = GetShader("ProbeLightingIndexed");
        if (!shader) return;

        static int frameIndex = 0;
        frameIndex++;

        BindSSBO(g_pointCloudVbo, 4);
        BindSSBO("ProbeSHColor", 5);
        BindSSBO("ProbeVisibilityCounter", 6);
        BindSSBO("ProbeVisibilityIndices", 7);

        shader->Bind();
        shader->SetInt("u_probeCount", lightVolume.GetProbeCount());
        shader->SetVec3("u_gridOffset", lightVolume.GetOffset());
        shader->SetInt("u_gridWidth", lightVolume.GetProbeCountX());
        shader->SetInt("u_gridHeight", lightVolume.GetProbeCountY());
        shader->SetInt("u_gridDepth", lightVolume.GetProbeCountZ());
        shader->SetInt("u_pointCount", pointCloud.size());
        shader->SetInt("u_frameIndex", frameIndex);
        shader->SetFloat("u_spacing", GlobalIllumination::GetProbeSpacing());
        shader->SetFloat("u_pointCloudSpacing", GlobalIllumination::GetPointCloudSpacing());

        BindDispatchBuffer("ProbeDispatchArgs");

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
        glDispatchComputeIndirect(0);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    void RaytracedSceneDebug() {
        ProfilerOpenGLZoneFunction();

        OpenGLFrameBuffer* fbo = GetFrameBuffer("RaytracingDebug");
        OpenGLShader* shader = GetShader("RaytraceScene");

        if (!fbo) return;
        if (!shader) return;

        const std::vector<ViewportData>& viewportData = RenderDataManager::GetViewportData();

        shader->Bind();
        shader->SetMat4("u_projectionMatrix", viewportData[0].projection);
        shader->SetMat4("u_viewMatrix", viewportData[0].view);

        BindSSBO("EntityInstances", 0);
        BindSSBO("TriangleData", 1);
        BindSSBO("SceneBvh", 2);
        BindSSBO("MeshesBvh", 3);
        BindSSBO("Lights", 4);

        glBindImageTexture(0, fbo->GetColorAttachmentHandleByName("Color"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glDispatchCompute(fbo->GetWidth() / 8, fbo->GetHeight() / 8, 1);
    }

    void ProbeSampleDebug() {
        ProfilerOpenGLZoneFunction();
        const std::vector<ViewportData>& viewportData = RenderDataManager::GetViewportData();
        const LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();

        OpenGLFrameBuffer* fbo = GetFrameBuffer("RaytracingDebug");
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* shader = GetShader("ProbeSampleDebug");

        if (!fbo) return;
        if (!gBuffer) return;
        if (!shader) return;

        shader->Bind();
        shader->SetMat4("u_projectionMatrix", viewportData[0].projection);
        shader->SetVec3("u_cameraPos", viewportData[0].viewPos);
        shader->SetMat4("u_viewMatrix", viewportData[0].view);
        shader->SetVec3("u_probeOffset", lightVolume.GetOffset());
        shader->SetFloat("u_probeSpacing", GlobalIllumination::GetProbeSpacing());
        shader->SetInt("u_probeCount", lightVolume.GetProbeCount());
        shader->SetInt("u_probeCountX", lightVolume.GetProbeCountX());
        shader->SetInt("u_probeCountY", lightVolume.GetProbeCountY());
        shader->SetInt("u_probeCountZ", lightVolume.GetProbeCountZ());
        
        shader->SetInt("u_gridWidth", lightVolume.GetProbeCountX());
        shader->SetInt("u_gridHeight", lightVolume.GetProbeCountY());
        shader->SetInt("u_gridDepth", lightVolume.GetProbeCountZ());
        shader->SetVec3("u_gridOffset", lightVolume.GetOffset());
        shader->SetFloat("u_spacing", GlobalIllumination::GetProbeSpacing());

        BindSSBO("EntityInstances", 0);
        BindSSBO("TriangleData", 1);
        BindSSBO("SceneBvh", 2);
        BindSSBO("MeshesBvh", 3);
        BindSSBO("Lights", 4);
        BindSSBO("ProbeSHColor", 5);
        BindSSBO("ProbeState", 6);

        glBindImageTexture(0, fbo->GetColorAttachmentHandleByName("Color"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindTextureUnit(1, gBuffer->GetColorAttachmentHandleByName("WorldPosition"));
        glBindTextureUnit(2, gBuffer->GetColorAttachmentHandleByName("Normal"));
        glBindTextureUnit(3, gBuffer->GetDepthAttachmentHandle());

        OpenGLTextureArray& probeDistanceTexture = GetProbeDistanceTextureArray();
        shader->BindTextureUnit(4, probeDistanceTexture.GetHandle());

        glDispatchCompute(fbo->GetWidth() / TILE_SIZE, fbo->GetHeight() / TILE_SIZE, 1);
    }

    //void UpdateDistanceTextureSize() {
    //    LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();
    //    uint32_t probeCountX = lightVolume.GetProbeCountX();
    //    uint32_t probeCountY = lightVolume.GetProbeCountY();
    //    uint32_t probeCountZ = lightVolume.GetProbeCountZ();
    //
    //    // Pack 3d probe grid into a 2d atlas using 16x16 per probe
    //    int atlasWidth = probeCountX * probeCountY * 16;
    //    int atlasHeight = probeCountZ * 16;
    //
    //    // Skip if texture is already the correct size
    //    if (g_probeDistanceTexture.GetWidth() == atlasWidth && g_probeDistanceTexture.GetHeight() == atlasHeight) {
    //        return;
    //    }
    //
    //    g_probeDistanceTexture.Create(atlasWidth, atlasHeight, GL_RG16F, 1);
    //
    //    float spacing = GlobalIllumination::GetProbeSpacing();
    //    float maxDist = spacing * 1.5f;
    //    float clearValues[4] = { maxDist, maxDist * maxDist, 0.0f, 0.0f };
    //
    //    // Pre-fill texture to max distance to avoid zero-distance artifacts before rays populate
    //    glClearTexImage(g_probeDistanceTexture.GetHandle(), 0, GL_RG, GL_FLOAT, clearValues);
    //}

    void UpdateDistanceTextureSize() {
        LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();
        uint32_t probeCountX = lightVolume.GetProbeCountX();
        uint32_t probeCountY = lightVolume.GetProbeCountY();
        uint32_t probeCountZ = lightVolume.GetProbeCountZ();

        // Each layer represents a horizontal plane of probes (X/Z plane).
        // The number of layers equals the vertical probe count (Y).
        uint32_t layerWidth = probeCountX * 16;
        uint32_t layerHeight = probeCountZ * 16;
        uint32_t layerCount = probeCountY;

        // Skip if texture is already the correct size
        if (g_probeDistanceTextureArray.GetWidth() == layerWidth &&
            g_probeDistanceTextureArray.GetHeight() == layerHeight &&
            g_probeDistanceTextureArray.GetCount() == layerCount) {
            return;
        }

        g_probeDistanceTextureArray.AllocateMemory(layerWidth, layerHeight, GL_RG16F, 1, layerCount);
        g_probeDistanceTextureArray.SetMinFilter(TextureFilter::LINEAR);
        g_probeDistanceTextureArray.SetMagFilter(TextureFilter::LINEAR);
        g_probeDistanceTextureArray.SetWrapMode(TextureWrapMode::CLAMP_TO_EDGE); // DDGI relies on no-wrap for borders

        float spacing = GlobalIllumination::GetProbeSpacing();
        float maxDist = spacing * 1.5f;
        float clearValues[4] = { maxDist, maxDist * maxDist, 0.0f, 0.0f };

        // Pre-fill entire texture array to max distance
        // Using glClearTexImage clears all layers at once
        glClearTexImage(g_probeDistanceTextureArray.GetHandle(), 0, GL_RG, GL_FLOAT, clearValues);
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