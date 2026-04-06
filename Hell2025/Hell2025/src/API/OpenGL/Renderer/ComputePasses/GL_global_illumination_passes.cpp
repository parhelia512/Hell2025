#include "API/OpenGL/Renderer/GL_renderer.h"
#include "API/OpenGL/GL_backend.h"
#include "AssetManagement/AssetManager.h"
#include "Bvh/Gpu/Bvh.h"
#include "Input/Input.h" // REMOVE ME!
#include "Renderer/Renderer.h"
#include "Renderer/RenderDataManager.h"
#include "GlobalIllumination/GlobalIllumination.h"
#include "Viewport/ViewportManager.h"
#include "World/World.h"
#include "Util/Util.h"

#include <Hell/Constants.h>
#include <Hell/Logging.h>

namespace OpenGLRenderer {

    GLuint g_pointCloudVao = 0;
    GLuint g_pointCloudVbo = 0;
    OpenGLTextureArray g_probeDistanceTextureArray; 
    OpenGLTextureArray g_probeIrradianceTextureArray;
    bool g_useSH = true;

    void UploadPointCloud(DDGIVolume& ddgiVolume);
    void ComputePointCloudBaseColor(DDGIVolume& ddgiVolume);

    void ResetProbeStates(DDGIVolume& ddgiVolume);
    void UpdateProbeStates(DDGIVolume& ddgiVolume);
    void UpdateDistanceTexture(DDGIVolume& ddgiVolume);
    void UpdateIrradianceTexture(DDGIVolume& ddgiVolume);
    void ComputePointCloudLighting(DDGIVolume& ddgiVolume);
    void ComputeProbeVisibility(DDGIVolume& ddgiVolume);
    void ComputeProbeDistance(DDGIVolume& ddgiVolume);
    void ComputeProbeDistanceBorder(DDGIVolume& ddgiVolume);
    void ComputeProbeIrradiance(DDGIVolume& ddgiVolume);
    void ComputeProbeIrradianceBorder(DDGIVolume& ddgiVolume);
    void ComputeIrradianceTexture(DDGIVolume& ddgiVolume);

    OpenGLTextureArray& GetProbeDistanceTextureArray();
    OpenGLTextureArray& GetProbeIrradianceTextureArray();

    void UpdateGlobalIllumintation() {
        // Hack to toggle between SH and Octral mapping for irradiance
        if (Input::KeyPressed(HELL_KEY_SEMICOLON)) {
            Audio::PlayAudio(AUDIO_SELECT, 1.00f);
            g_useSH = !g_useSH;
        }

        uint64_t id = 0;
        for (DDGIVolume& volume : World::GetDDGIVolumes()) {
            id = volume.GetId();
            break;
        }

        DDGIVolume& ddgiVolume = *World::GetDDGIVolumeByObjectId(id);

        if (ddgiVolume.PointCloudNeedsGPUUpload()) {
            UploadPointCloud(ddgiVolume);
            ComputePointCloudBaseColor(ddgiVolume);
            ResetProbeStates(ddgiVolume);

            ddgiVolume.MarkPointCloudAsUploaded();
        }

        ddgiVolume.UpdateSceneBvh();

        uint64_t sceneBvhId = ddgiVolume.GetSceneBvhId();
        const std::vector<BvhNode>& sceneNodes = ddgiVolume.GetSceneNodes();
        const std::vector<BvhNode>& meshBvhNodes = Bvh::Gpu::GetMeshGpuBvhNodes();
        const std::vector<GpuPrimitiveInstance>& entityInstances = Bvh::Gpu::GetGpuEntityInstances(sceneBvhId);
        const std::vector<float>& triData = Bvh::Gpu::GetTriangleData();

        const DDGIVolumeGPU volume = ddgiVolume.GetGPUData();
        const std::vector<GPUAABB>& dirtyDoorABBBs = World::GetDirtyDoorAABBS();

        // Bvh::Gpu::RenderSceneBvh(sceneBvhId, GREEN);

        UpdateSSBO("SceneBvh", sceneNodes.size() * sizeof(BvhNode), sceneNodes.data());
        UpdateSSBO("MeshesBvh", meshBvhNodes.size() * sizeof(BvhNode), meshBvhNodes.data());
        UpdateSSBO("EntityInstances", entityInstances.size() * sizeof(GpuPrimitiveInstance), entityInstances.data());
        UpdateSSBO("TriangleData", triData.size() * sizeof(float), triData.data());

        UpdateSSBO("DDGIVolume", sizeof(DDGIVolumeGPU), &volume);
        UpdateSSBO("DirtyDoorAABBs", dirtyDoorABBBs.size() * sizeof(GPUAABB), dirtyDoorABBBs.data());

        ReserveSSBO("ProbeSHColor", sizeof(ProbeColor) * ddgiVolume.GetTotalProbeCount());
        ReserveSSBO("ProbeDistanceIndices", sizeof(uint32_t) * ddgiVolume.GetTotalProbeCount());
        ReserveSSBO("ProbeVisibilityIndices", sizeof(uint32_t) * ddgiVolume.GetTotalProbeCount());
        ReserveSSBO("ProbeStates", sizeof(ProbeState) * ddgiVolume.GetTotalProbeCount());

        // Raytracing SSBOs stay persistently bound for whole GI pass
        BindSSBO("EntityInstances", 0);
        BindSSBO("TriangleData", 1);
        BindSSBO("SceneBvh", 2);
        BindSSBO("MeshesBvh", 3);

        UpdateDistanceTexture(ddgiVolume);
        UpdateIrradianceTexture(ddgiVolume);
        UpdateProbeStates(ddgiVolume);

        ComputePointCloudLighting(ddgiVolume);
        ComputeProbeVisibility(ddgiVolume);
        ComputeProbeDistance(ddgiVolume);
        ComputeProbeDistanceBorder(ddgiVolume);
        ComputeProbeIrradiance(ddgiVolume);
        ComputeProbeIrradianceBorder(ddgiVolume);
        ComputeIrradianceTexture(ddgiVolume);

    }

    void ResetProbeStates(DDGIVolume& ddgiVolume) {
        std::vector<ProbeState> probeStates;
        probeStates.reserve(ddgiVolume.GetTotalProbeCount());

        for (uint32_t i = 0; i < ddgiVolume.GetTotalProbeCount(); i++) {
            ProbeState& probeState = probeStates.emplace_back();
            probeState.isActive = true;
            probeState.isVisible = true;
            probeState.distanceCooldown = PROBE_MAX_DISTANCE_COOLDOWN;
            probeState.irradianceCooldown = PROBE_MAX_IRRADIANCE_COOLDOWN;
            probeState.relocationOffset = glm::vec3(0.0f);
        }

        UpdateSSBO("ProbeStates", probeStates.size() * sizeof(ProbeState), probeStates.data());
    }

    void UpdateProbeStates(DDGIVolume& ddgiVolume) {
        ProfilerOpenGLZoneFunction();

        BindSSBO("DDGIVolume", 4);
        BindSSBO("ProbeStates", 5);
        BindSSBO("DirtyDoorAABBs", 6);
        
        BindShader("ProbeStateUpdate");

        OpenGLShader* shader = GetShader("ProbeStateUpdate");
        shader->SetInt("u_dirtyDoorAABBCount", (int)World::GetDirtyDoorAABBS().size());

        DispatchCompute((ddgiVolume.GetTotalProbeCount() + 63) / 64, 1, 1);
    }

    void ComputePointCloudLighting(DDGIVolume& ddgiVolume) {
        ProfilerOpenGLZoneFunction();

        OpenGLShader* shader = GetShader("PointCloudLighting");
        shader->Bind();
        shader->SetInt("u_lightCount", World::GetLightCount());

        BindSSBO("Lights", 4);
        BindSSBO(g_pointCloudVbo, 5);

        glDispatchCompute((ddgiVolume.GetPointClound().size() + 127) / 128, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    void ComputeProbeDistance(DDGIVolume& ddgiVolume) {
        ProfilerOpenGLZoneFunction();

        OpenGLShader* distanceShader = GetShader("ProbeDistance");
        OpenGLShader* listShader = GetShader("ProbeDistanceList");
        OpenGLShader* argsShader = GetShader("ProbeDistanceDispatchArgs");

        if (!distanceShader || !listShader || !argsShader) return;

        OpenGLTextureArray& probeDistanceTexture = GetProbeDistanceTextureArray();
        BindImageTextureArray(0, probeDistanceTexture.GetHandle(), GL_READ_WRITE, GL_RG16F);

        static int frameIndex = 0;
        frameIndex++;

        ClearSSBO("ProbeDistanceCounter");

        BindSSBO("DDGIVolume", 4);
        BindSSBO("ProbeStates", 5);
        BindSSBO("ProbeDistanceCounter", 6);
        BindSSBO("ProbeDistanceIndices", 7);
        BindSSBO("ProbeDistanceDispatchArgs", 8);

        BindShader("ProbeDistanceList");
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute((ddgiVolume.GetTotalProbeCount() + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        BindShader("ProbeDistanceDispatchArgs");
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

        distanceShader->Bind();
        distanceShader->SetInt("u_frameIndex", frameIndex);
        BindDispatchBuffer("ProbeDistanceDispatchArgs");
        glDispatchComputeIndirect(0);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void ComputeProbeDistanceBorder(DDGIVolume& ddgiVolume) {
        ProfilerOpenGLZoneFunction();

        OpenGLShader* shader = GetShader("ProbeDistanceBorder");
        if (!shader) return;

        BindShader("ProbeDistanceBorder");
        BindSSBO("DDGIVolume", 4);

        OpenGLTextureArray& probeDistanceTexture = GetProbeDistanceTextureArray();
        BindImageTextureArray(0, probeDistanceTexture.GetHandle(), GL_READ_WRITE, GL_RG16F);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glDispatchCompute((ddgiVolume.GetTotalProbeCount() + 63) / 64, 1, 1);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void ComputeProbeIrradiance(DDGIVolume& ddgiVolume) {
        ProfilerOpenGLZoneFunction();

        OpenGLShader* shader = GetShader("ProbeIrradiance");
        if (!shader) return;

        static int frameIndex = 0;
        frameIndex++;

        BindSSBO(g_pointCloudVbo, 4);
        BindSSBO("ProbeSHColor", 5);
        BindSSBO("ProbeVisibilityCounter", 6);
        BindSSBO("ProbeVisibilityIndices", 7);
        BindSSBO("DDGIVolume", 8);
        BindSSBO("ProbeStates", 9);

        shader->Bind();
        shader->SetFloat("u_pointCloudSpacing", ddgiVolume.GetPointCloudSpacing());
        shader->SetInt("u_pointCount", ddgiVolume.GetPointCloudCount());
        shader->SetInt("u_frameIndex", frameIndex);
        shader->SetBool("u_useSH", g_useSH);

        OpenGLTextureArray& probeIrradianceTexture = GetProbeIrradianceTextureArray();
        BindImageTextureArray(0, probeIrradianceTexture.GetHandle(), GL_READ_WRITE, GL_RGBA16F);

        BindDispatchBuffer("ProbeIrradianceDispatchArgs");

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
        glDispatchComputeIndirect(0);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void ComputeProbeIrradianceBorder(DDGIVolume& ddgiVolume) {
        ProfilerOpenGLZoneFunction();

        OpenGLShader* shader = GetShader("ProbeIrradianceBorder");
        if (!shader) return;

        BindSSBO("DDGIVolume", 4);
        shader->Bind();

        OpenGLTextureArray& irradianceTexture = GetProbeIrradianceTextureArray();
        BindImageTextureArray(0, irradianceTexture.GetHandle(), GL_READ_WRITE, GL_RGBA16F);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glDispatchCompute((ddgiVolume.GetTotalProbeCount() + 63) / 64, 1, 1);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

	void ComputeProbeVisibility(DDGIVolume& ddgiVolume) {
		ProfilerOpenGLZoneFunction();

		OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* visibilityShader = GetShader("ProbeVisibility");
        OpenGLShader* listShader = GetShader("ProbeVisibilityList");
        OpenGLShader* argsShader = GetShader("ProbeLightingDispatchArgs");

		if (!gBuffer) return;
        if (!visibilityShader) return;
        if (!listShader) return; 
        if (!argsShader) return;

        ClearSSBO("ProbeVisibilityCounter");
        
        BindSSBO("ProbeStates", 4);
        BindSSBO("ProbeVisibilityCounter", 5);
        BindSSBO("ProbeVisibilityIndices", 6);
        BindSSBO("ProbeIrradianceDispatchArgs", 7);
        BindSSBO("DDGIVolume", 8);

        // Iterate each pixel, and mark and probes that influence it as visible
        BindShader("ProbeVisibility");
        SetUniformVec3("u_viewPos", RenderDataManager::GetViewportData()[0].viewPos);

        BindTextureUnit(0, gBuffer->GetDepthAttachmentHandle());
        BindTextureUnit(1, gBuffer->GetColorAttachmentHandleByName("WorldPosition"));
        BindTextureUnit(2, gBuffer->GetColorAttachmentHandleByName("Normal"));

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

        glDispatchCompute((gBuffer->GetWidth() + 7) / 8, (gBuffer->GetHeight() + 7) / 8, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        listShader->Bind();
        listShader->SetInt("u_probeCount", ddgiVolume.GetTotalProbeCount());

        glDispatchCompute((ddgiVolume.GetTotalProbeCount() + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        argsShader->Bind();
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
    }


	void ComputePointCloudBaseColor(DDGIVolume& ddgiVolume) {
		// TODO:
		// If RenderDoc was detected then you gotta do this differently
		if (BackEnd::RenderDocFound()) {
			Logging::Fatal() << "GlobalIllumination::CalculatePointCloudBaseColor() does not work without BIndless yet you fool.\n";
            return;
		}

        OpenGLShader* shader = GetShader("PointCloudBaseColor");
        if (!shader) return;

        const std::vector<CloudPointTextureInfo>& pointCloundTextureInfo = ddgiVolume.GetPointCloudTextureInfo();

        UpdateSSBO("PointCloudTextureInfo", pointCloundTextureInfo.size() * sizeof(CloudPointTextureInfo), pointCloundTextureInfo.data());

        // Ensure bindless texture IDs are in the Samplers ID, which is not the case if this runs the first frame of the renderer
        UpdateSSBO("Samplers", sizeof(GLuint64) * OpenGLBackEnd::GetBindlessTextureIDs().size(), OpenGLBackEnd::GetBindlessTextureIDs().data());

		BindSSBO("Samplers", 0);
		BindSSBO("PointCloudTextureInfo", 1);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_pointCloudVbo);

		GLuint numGroupsX = (ddgiVolume.GetPointCloudCount() + 127) / 128;

		shader->Bind();
        shader->DispatchCompute(numGroupsX, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    }


    
    void UploadPointCloud(DDGIVolume& ddgiVolume) {
        if (g_pointCloudVao == 0) {
            glGenVertexArrays(1, &g_pointCloudVao);
            glGenBuffers(1, &g_pointCloudVbo);
        }

        const std::vector<CloudPoint>& pointCloud = ddgiVolume.GetPointClound();

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

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        Logging::Debug() << "Uploaded point cloud to GPU (" << pointCloud.size() << " points)\n";
    }


    void DrawPointCloud(DDGIVolume& ddgiVolume) {
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

        for (int i = 0; i < 4; i++) {
            Viewport* viewport = ViewportManager::GetViewportByIndex(i);
            if (!viewport->IsVisible()) continue;

            OpenGLRenderer::SetViewport(gBuffer, viewport);
            shader->SetInt("u_viewportIndex", i);
            shader->SetMat4("u_projectionView", viewportData[i].projectionView);

            glBindVertexArray(g_pointCloudVao);
            glDrawArrays(GL_POINTS, 0, ddgiVolume.GetPointCloudCount());
            glBindVertexArray(0);
        }
    }

    void DrawProbes(DDGIVolume& ddgiVolume) {
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* shader = GetShader("DebugProbes");

        if (!gBuffer) return;
        if (!shader) return;

        shader->Bind();
        shader->SetBool("u_useSH", g_useSH);

        OpenGLTextureArray& probeDistanceTexture = GetProbeDistanceTextureArray();
        BindTextureUnit(0, probeDistanceTexture.GetHandle());

        OpenGLTextureArray& probeIrradianceTexture = GetProbeIrradianceTextureArray();
        BindTextureUnit(1, probeIrradianceTexture.GetHandle());

        gBuffer->Bind();
        gBuffer->DrawBuffer("FinalLighting");

        BindSSBO("ProbeSHColor", 6);
        BindSSBO("DDGIVolume", 7);
        BindSSBO("ProbeStates", 8);

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

			glDrawElementsInstancedBaseVertex(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * mesh->baseIndex), ddgiVolume.GetTotalProbeCount(), mesh->baseVertex);
        }
    }


    
    

    void RaytracedSceneDebug() {
        ProfilerOpenGLZoneFunction();

        OpenGLFrameBuffer* fbo = GetFrameBuffer("IndirectDiffuse");
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

    void ComputeIrradianceTexture(DDGIVolume& ddgiVolume) {
        ProfilerOpenGLZoneFunction();

        const std::vector<ViewportData>& viewportData = RenderDataManager::GetViewportData();

        OpenGLFrameBuffer* fbo = GetFrameBuffer("IndirectDiffuse");
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* shader = GetShader("ProbeSampleDebug");

        if (!fbo) return;
        if (!gBuffer) return;
        if (!shader) return;

        BindSSBO("DDGIVolume", 7);

        shader->Bind();
        shader->SetMat4("u_projectionMatrix", viewportData[0].projection);
        shader->SetVec3("u_cameraPos", viewportData[0].viewPos);
        shader->SetMat4("u_viewMatrix", viewportData[0].view);
        shader->SetBool("u_useSH", g_useSH);

        BindSSBO("EntityInstances", 0);
        BindSSBO("TriangleData", 1);
        BindSSBO("SceneBvh", 2);
        BindSSBO("MeshesBvh", 3);
        BindSSBO("Lights", 4);
        BindSSBO("ProbeSHColor", 5);
        BindSSBO("ProbeStates", 6);

        glBindImageTexture(0, fbo->GetColorAttachmentHandleByName("Color"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindTextureUnit(1, gBuffer->GetColorAttachmentHandleByName("WorldPosition"));
        glBindTextureUnit(2, gBuffer->GetColorAttachmentHandleByName("Normal"));
        glBindTextureUnit(3, gBuffer->GetDepthAttachmentHandle());

        OpenGLTextureArray& probeDistanceTexture = GetProbeDistanceTextureArray();
        BindTextureUnit(4, probeDistanceTexture.GetHandle());

        OpenGLTextureArray& probeIrradianceTexture = GetProbeIrradianceTextureArray();
        BindTextureUnit(5, probeIrradianceTexture.GetHandle());

        glDispatchCompute(fbo->GetWidth() / TILE_SIZE, fbo->GetHeight() / TILE_SIZE, 1);
    }

    void UpdateDistanceTexture(DDGIVolume& ddgiVolume) {
        uint32_t probeCountX = ddgiVolume.GetProbeCountX();
        uint32_t probeCountY = ddgiVolume.GetProbeCountY();
        uint32_t probeCountZ = ddgiVolume.GetProbeCountZ();

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

        float maxDist = ddgiVolume.GetProbeSpacing() * 1.5f;
        float clearValues[4] = { maxDist, maxDist * maxDist, 0.0f, 0.0f };

        // Pre-fill entire texture array to max distance
        // Using glClearTexImage clears all layers at once
        glClearTexImage(g_probeDistanceTextureArray.GetHandle(), 0, GL_RG, GL_FLOAT, clearValues);
    }

    void UpdateIrradianceTexture(DDGIVolume& ddgiVolume) {
        uint32_t probeCountX = ddgiVolume.GetProbeCountX();
        uint32_t probeCountY = ddgiVolume.GetProbeCountY();
        uint32_t probeCountZ = ddgiVolume.GetProbeCountZ();

        // Each layer represents a horizontal plane of probes (X/Z plane).
        // Irradiance uses 6x6 interior + 1px borders = 8x8 pixels per probe.
        uint32_t layerWidth = probeCountX * 8;
        uint32_t layerHeight = probeCountZ * 8;
        uint32_t layerCount = probeCountY;

        // Skip if texture is already the correct size
        if (g_probeIrradianceTextureArray.GetWidth() == layerWidth &&
            g_probeIrradianceTextureArray.GetHeight() == layerHeight &&
            g_probeIrradianceTextureArray.GetCount() == layerCount) {
            return;
        }

        // Allocate RGBA16F. (NVIDIA sometimes uses RGB10A2 for memory, but 16F is much safer 
        // for precision when accumulating light values > 1.0)
        g_probeIrradianceTextureArray.AllocateMemory(layerWidth, layerHeight, GL_RGBA16F, 1, layerCount);

        // Bilinear filtering is ABSOLUTELY CRITICAL for DDGI irradiance sampling
        g_probeIrradianceTextureArray.SetMinFilter(TextureFilter::LINEAR);
        g_probeIrradianceTextureArray.SetMagFilter(TextureFilter::LINEAR);

        // CLAMP_TO_EDGE is strictly required so the hardware sampler doesn't wrap 
        // around to the other side of the texture array when sampling the border pixels.
        g_probeIrradianceTextureArray.SetWrapMode(TextureWrapMode::CLAMP_TO_EDGE);

        // Pre-fill entire texture array to pitch black (no light yet)
        float clearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glClearTexImage(g_probeIrradianceTextureArray.GetHandle(), 0, GL_RGBA, GL_FLOAT, clearValues);
    }

    /*void DrawGPUBvhSceneNodes(const glm::vec4& color) {
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
    }*/

    OpenGLTextureArray& GetProbeDistanceTextureArray() {
        return g_probeDistanceTextureArray;
    }

    OpenGLTextureArray& GetProbeIrradianceTextureArray() {
        return g_probeIrradianceTextureArray;
    }
}