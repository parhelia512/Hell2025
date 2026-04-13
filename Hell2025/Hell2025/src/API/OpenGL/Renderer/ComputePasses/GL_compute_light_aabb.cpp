#include "API/OpenGL/Renderer/GL_renderer.h"
#include "Renderer/RenderDataManager.h"
#include "Viewport/ViewportManager.h"
#include "World/World.h"

#include <Hell/Logging.h>

namespace OpenGLRenderer {

	void ReserveLightAABBSSBOStorage() {
		uint32_t size = World::GetLightCount() * sizeof(glm::vec4) * 2;
		ReserveSSBO("LightAABBs", size);
	}

    void RenderWorldPosition(uint32_t lightIndex);
    void ComputeMinMax(uint32_t lightIndex);

    void ComputeLightAABBs() {
        RenderWorldPosition(3);
        ComputeMinMax(3);
    }

    void RenderWorldPosition(uint32_t lightIndex) {
        ProfilerOpenGLZoneFunctionLightGreen();

        OpenGLShader* shader = GetShader("LightAABBPosition");
        if (!shader) return;

        Light* light = World::GetLightByIndex(lightIndex);
        if (!light) return;

        OpenGLCubemapFrameBuffer& fbo = GetCubemapFrameBuffer("LightAABB");
        fbo.Bind();
        fbo.SetViewport();

        shader->Bind();
        BindSSBO("Lights", 4);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        OpenGLMeshBuffer& houseMeshBuffer = World::GetHouseMeshBuffer().GetGLMeshBuffer();
        glBindVertexArray(houseMeshBuffer.GetVAO());

        glm::vec3 lightPos = light->GetPosition();
        float radius = light->GetRadius();

        shader->SetInt("u_lightIndex", lightIndex);
        shader->SetFloat("u_lightRadius", radius);
        shader->SetVec3("u_lightPosition", lightPos);

        const glm::vec3 faceDirs[6] = {
            { 1,  0,  0}, // +X
            {-1,  0,  0}, // -X
            { 0,  1,  0}, // +Y
            { 0, -1,  0}, // -Y
            { 0,  0,  1}, // +Z
            { 0,  0, -1}  // -Z
        };

        const std::vector<HouseRenderItem>& renderItems = RenderDataManager::GetHouseRenderItems();

        for (int face = 0; face < 6; ++face) {
            fbo.BindFaceByIndex(face);
            fbo.ClearFaceDepth(1.0f);

            // Clear to the far extent of the light radius for this face
            glm::vec3 farPoint = lightPos + (faceDirs[face] * radius);
            fbo.ClearFaceColor(glm::vec4(farPoint, 1.0f));

            shader->SetInt("u_faceIndex", face);
            shader->SetMat4("u_shadowMatrix", light->GetProjectionView(face));
            shader->SetMat4("u_model", glm::mat4(1.0f));

            for (const HouseRenderItem& renderItem : renderItems) {
                glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    renderItem.indexCount,
                    GL_UNSIGNED_INT,
                    (void*)(sizeof(unsigned int) * renderItem.baseIndex),
                    renderItem.baseVertex
                );
            }
        }
        glBindVertexArray(0);
    }

    uint32_t FloatToUint(float f) {
        uint32_t u;
        memcpy(&u, &f, sizeof(float));
        return (u & 0x80000000) ? ~u : u | 0x80000000;
    }

    void ComputeMinMax(uint32_t lightIndex) {
        OpenGLShader* shader = GetShader("LightAABBMinMax");
        OpenGLSSBO* ssbo = GetSSBO("LightAABBs");
        Light* light = World::GetLightByIndex(lightIndex);

        if (!shader) return;
        if (!ssbo) return;
        if (!light) return;

        OpenGLCubemapFrameBuffer& fbo = GetCubemapFrameBuffer("LightAABB");

        unsigned int minBits = 0xFFFFFFFF;
        unsigned int maxBits = 0;

        // Reset with flipped bits
        struct { uint32_t x, y, z, w; } minU, maxU;
        minU.x = maxU.x = FloatToUint(light->GetPosition().x);
        minU.y = maxU.y = FloatToUint(light->GetPosition().y);
        minU.z = maxU.z = FloatToUint(light->GetPosition().z);
        minU.w = maxU.w = 0;

        size_t baseOffset = lightIndex * sizeof(glm::vec4) * 2;
        glNamedBufferSubData(ssbo->GetHandle(), baseOffset, sizeof(minU), &minU);
        glNamedBufferSubData(ssbo->GetHandle(), baseOffset + sizeof(glm::vec4), sizeof(maxU), &maxU);

        shader->Bind();
        shader->SetInt("u_lightIndex", lightIndex);
        shader->SetInt("u_resolution", fbo.GetSize());

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, fbo.GetColorHandle());
        shader->SetInt("u_WorldPosCubemap", 0);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssbo->GetHandle());

        uint32_t numGroups = fbo.GetSize() / 16;
        //glDispatchCompute(numGroups, numGroups, 1);
        glDispatchCompute(1, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

	void nothing() {

//
//
//	OpenGLMeshBuffer& houseMeshBuffer = World::GetHouseMeshBuffer().GetGLMeshBuffer();
//	glBindVertexArray(houseMeshBuffer.GetVAO());
//	shader->SetBool("u_useInstanceData", false);
//	shader->SetMat4("u_modelMatrix", glm::mat4(1.0f));
//
//	// OPTIMIZE ME!
//	// Make lights store a list of their HouseRenderItems per frustum face that is only updated when the map changes
//	// That will be when a HousePlane or Wall is added/modified
//
//	for (int i = 0; i < gpuLightsHighRes.size(); i++) {
//		const GPULight& gpuLight = gpuLightsHighRes[i];
//
//		Light* light = World::GetLightByIndex(gpuLight.lightIndex);
//		if (!light || !light->IsDirty()) continue;
//
//		shader->SetFloat("farPlane", light->GetRadius());
//		shader->SetVec3("lightPosition", light->GetPosition());
//		shader->SetMat4("shadowMatrices[0]", light->GetProjectionView(0));
//		shader->SetMat4("shadowMatrices[1]", light->GetProjectionView(1));
//		shader->SetMat4("shadowMatrices[2]", light->GetProjectionView(2));
//		shader->SetMat4("shadowMatrices[3]", light->GetProjectionView(3));
//		shader->SetMat4("shadowMatrices[4]", light->GetProjectionView(4));
//		shader->SetMat4("shadowMatrices[5]", light->GetProjectionView(5));
//
//		for (int face = 0; face < 6; ++face) {
//			shader->SetInt("faceIndex", face);
//			int shadowMapIndex = i;
//			GLuint layer = shadowMapIndex * 6 + face;
//
//			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, hiResShadowMaps->GetDepthTexture(), 0, layer);
//
//			Frustum* frustum = light->GetFrustumByFaceIndex(face);
//			if (!frustum) return;
//
//			const std::vector<HouseRenderItem>& renderItems = RenderDataManager::GetHouseRenderItems();
//			for (const HouseRenderItem& renderItem : renderItems) {
//
//				if (!frustum->IntersectsAABBFast(renderItem)) continue;
//
//				int indexCount = renderItem.indexCount;
//				int baseVertex = renderItem.baseVertex;
//				int baseIndex = renderItem.baseIndex;
//				glDrawElementsBaseVertex(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * baseIndex), baseVertex);
//			}
//		}
//	}

	}








	void DebugDrawLightAABBs() {
		static GLuint vao = 0;
		if (vao == 0) {
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);
		}

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
		gBuffer->Bind();
		gBuffer->DrawBuffer("FinalLighting");

		BindShader("DebugLightAABB");

		BindSSBO("Lights", 4);
		BindSSBO("LightAABBs", 5);

		glBindVertexArray(vao);

		for (int i = 0; i < 4; i++) {
			Viewport* viewport = ViewportManager::GetViewportByIndex(i);
			if (viewport->IsVisible()) {
				OpenGLRenderer::SetViewport(gBuffer, viewport);
				SetUniformMat4("u_projectionView", RenderDataManager::GetViewportData()[i].projectionView);

				for (int j = 0; j < World::GetLightCount(); j++) {

					if (j != 3) continue;

					SetUniformInt("u_lightIndex", j);
					glDrawArrays(GL_LINE_STRIP, 0, 16);
				}
			}
		}
	}
}