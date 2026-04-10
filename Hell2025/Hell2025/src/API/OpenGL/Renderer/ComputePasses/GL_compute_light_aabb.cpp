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

	void ComputeLightAABBs() {
		ProfilerOpenGLZoneFunctionLightGreen();

		ReserveLightAABBSSBOStorage();

		// TODO: write the real shader, dont use this one!!!!!!!
		// TODO: write the real shader, dont use this one!!!!!!!
		// TODO: write the real shader, dont use this one!!!!!!!
		// TODO: write the real shader, dont use this one!!!!!!!
		// TODO: write the real shader, dont use this one!!!!!!!
		// TODO: write the real shader, dont use this one!!!!!!!
		OpenGLShader* shader = GetShader("LightAABB");


		BindSSBO("Lights", 4);
		BindSSBO("LightAABBs", 5);

		OpenGLShadowCubeMapArray* cubemapArray = GetShadowCubeMapArray("LightAABB");
		const DrawCommandsSet& drawInfoSet = RenderDataManager::GetDrawInfoSet();

		if (!shader) return;
		if (!cubemapArray) return;

		shader->Bind();

		// This is your cubemap's index in the array.
		int cubemapLayer = 0;

		// This is confusing because you don't even want an array here, you just
		// have no OpenGLCubeMap wrapper object.



		const std::vector<HouseRenderItem>& renderItems = RenderDataManager::GetHouseRenderItems();



		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glCullFace(GL_BACK);

		glBindFramebuffer(GL_FRAMEBUFFER, cubemapArray->GetHandle());

		//if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		//	std::cout << "Framebuffer not complete\n";
		//}
		//else {
		//	std::cout << "Framebuffer complete\n";
		//}


		glViewport(0, 0, cubemapArray->GetSize(), cubemapArray->GetSize());

		OpenGLMeshBuffer& houseMeshBuffer = World::GetHouseMeshBuffer().GetGLMeshBuffer();
		glBindVertexArray(houseMeshBuffer.GetVAO());

		if (houseMeshBuffer.GetIndexCount() <= 0) return; // Bail if there is no house geometry

		for (int i = 0; i < World::GetLightCount(); i++) {
			// Skip any light but light 3
			if (i != 3) continue;

			Light* light = World::GetLightByIndex(i);
			if (!light) continue;

			cubemapArray->ClearDepthLayer(cubemapLayer, 1.0f);


			BindSSBO("Lights", 4);
			BindSSBO("LightAABBs", 5);

			shader->SetInt("u_lightIndex", i);
			shader->SetFloat("u_farPlane", light->GetRadius());
			shader->SetVec3("u_lightPosition", light->GetPosition());
			shader->SetMat4("u_shadowMatrices[0]", light->GetProjectionView(0));
			shader->SetMat4("u_shadowMatrices[1]", light->GetProjectionView(1));
			shader->SetMat4("u_shadowMatrices[2]", light->GetProjectionView(2));
			shader->SetMat4("u_shadowMatrices[3]", light->GetProjectionView(3));
			shader->SetMat4("u_shadowMatrices[4]", light->GetProjectionView(4));
			shader->SetMat4("u_shadowMatrices[5]", light->GetProjectionView(5));

			for (int face = 0; face < 6; ++face) {
				GLuint layer = i * 6 + face;
				shader->SetInt("u_faceIndex", face);
				glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, cubemapArray->GetDepthTexture(), 0, layer);


				//glDrawElementsBaseVertex(GL_TRIANGLES, houseMeshBuffer.GetIndexCount(), GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * 0), 0);

				for (const HouseRenderItem& renderItem : renderItems) {
					int indexCount = renderItem.indexCount;
					int baseVertex = renderItem.baseVertex;
					int baseIndex = renderItem.baseIndex;

					glDrawElementsBaseVertex(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * baseIndex), baseVertex);
				}
				glFinish();
				break;
			}

			Logging::Debug() << "Computed AABB for light index " << i << " ..... supposedly\n";
		}

		glBindVertexArray(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
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