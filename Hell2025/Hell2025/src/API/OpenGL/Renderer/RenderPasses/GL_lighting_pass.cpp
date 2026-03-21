#include "../GL_renderer.h"
#include "AssetManagement/AssetManager.h"
#include "Core/Game.h"
#include "GlobalIllumination/GlobalIllumination.h"
#include "World/World.h"
#include "Ocean/Ocean.h"

#include <Hell/Logging.h>

namespace OpenGLRenderer {


    void ComputeViewspaceDepth() {
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLFrameBuffer* fullSizeFBO = GetFrameBuffer("MiscFullSize");
        OpenGLShader* shader = GetShader("ViewspaceDepth");

        if (!gBuffer) return;
        if (!fullSizeFBO) return;
        if (!shader) return;

        shader->Bind();
        shader->BindImageTexture(0, fullSizeFBO->GetColorAttachmentHandleByName("ViewspaceDepth"), GL_WRITE_ONLY, GL_R32F);
        shader->BindTextureUnit(1, gBuffer->GetColorAttachmentHandleByName("WorldPosition"));
        shader->BindTextureUnit(2, fullSizeFBO->GetColorAttachmentHandleByName("ViewportIndex"));

        glDispatchCompute((gBuffer->GetWidth() + 7) / 8, (gBuffer->GetHeight() + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    void LightingPass() {
        ProfilerOpenGLZoneFunction();

        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLFrameBuffer* finalImageFBO = GetFrameBuffer("FinalImage");
        OpenGLShadowMap* flashLightShadowMapsFBO = GetShadowMap("FlashlightShadowMaps");
        OpenGLShadowCubeMapArray* hiResShadowMaps = GetShadowCubeMapArray("HiRes");
        OpenGLShader* shader = GetShader("Lighting");
        OpenGLFrameBuffer* miscFullSizeFBO = GetFrameBuffer("MiscFullSize");

        if (!gBuffer) return;
        if (!miscFullSizeFBO) return;
        if (!finalImageFBO) return;
        if (!shader) return;

        // REMOVE ME
        static bool sampleProbes = true;
        if (Input::KeyPressed(HELL_KEY_SLASH)) {
            sampleProbes = !sampleProbes;
		}
		// REMOVE ME

        std::vector<Light>& lights = World::GetLights();
        //for (Light& light : lights) {
        //    light.SetStrength(0.5);
        //}
        if (lights.size() > 6) {
            lights.erase(lights.begin() + 6);
        }
        //std::cout << "Light count: " << lights.size() << '\n';

        shader->Bind();

        shader->SetFloat("u_viewportWidth", gBuffer->GetWidth());
        shader->SetFloat("u_viewportHeight", gBuffer->GetHeight());
        shader->SetInt("u_tileXCount", gBuffer->GetWidth() / TILE_SIZE);
        shader->SetInt("u_tileYCount", gBuffer->GetHeight() / TILE_SIZE);
        shader->SetBool("u_sampleProbes", sampleProbes);

        LightVolume& lightVolume = GlobalIllumination::GetTestLightVolume();
        std::vector<CloudPoint>& pointCloud = GlobalIllumination::GetPointClound();

        shader->SetVec3("u_probeOffset", lightVolume.GetOffset());
        shader->SetFloat("u_probeSpacing", GlobalIllumination::GetProbeSpacing());
        shader->SetInt("u_probeCount", lightVolume.GetProbeCount());
        shader->SetInt("u_probeCountX", lightVolume.GetProbeCountX());
        shader->SetInt("u_probeCountY", lightVolume.GetProbeCountY());
        shader->SetInt("u_probeCountZ", lightVolume.GetProbeCountZ());
        // End GI

        if (World::HasOcean()) {
            shader->SetFloat("u_oceanHeight", Ocean::GetOceanOriginY());
        }
        else {
            shader->SetFloat("u_oceanHeight", -1000);
        }

        // Warning this CSM shit is p1 only atm, especially cause of hardcoded FULL SCREEN viewport dimensions

        float viewportWidth = gBuffer->GetWidth();
        float viewportHeight = gBuffer->GetHeight();
        std::vector<float>& cascadeLevels = GetShadowCascadeLevels();

        shader->SetVec3("u_moonlightDir", Game::GetMoonlightDirection());
        shader->SetFloat("farPlane", FAR_PLANE);
        shader->SetVec2("u_viewportSize", glm::vec2(viewportWidth, viewportHeight));
        shader->SetInt("cascadeCount", cascadeLevels.size() + 1);
        for (size_t i = 0; i < cascadeLevels.size(); ++i) {
            shader->SetFloat("u_cascadePlaneDistances[" + std::to_string(i) + "]", cascadeLevels[i]);
        }

        glBindTextureUnit(0, gBuffer->GetColorAttachmentHandleByName("BaseColor"));
        glBindTextureUnit(1, gBuffer->GetColorAttachmentHandleByName("Normal"));
        glBindTextureUnit(2, gBuffer->GetColorAttachmentHandleByName("RMA"));
        glBindTextureUnit(3, gBuffer->GetDepthAttachmentHandle());
        glBindTextureUnit(4, gBuffer->GetColorAttachmentHandleByName("WorldPosition"));
        glBindTextureUnit(5, miscFullSizeFBO->GetColorAttachmentHandleByName("ViewportIndex"));
        glBindTextureUnit(6, gBuffer->GetColorAttachmentHandleByName("Emissive"));
        glBindTextureUnit(7, AssetManager::GetTextureByName("Flashlight2")->GetGLTexture().GetHandle());
        glBindTextureUnit(8, flashLightShadowMapsFBO->GetDepthTextureHandle());
        //glBindTextureUnit(9, hiResShadowMaps->GetDepthTexture());


        BindSSBO("ProbeSHData", 6);
        BindSSBO("TileChristmasLights", 7);
        BindSSBO("ChristmasLightInstances", 8);
        BindSSBO("ChristmasLightIndices", 9);

        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, hiResShadowMaps->GetDepthTexture());



            shader->SetFloat("u_lightVolumeSpacing", GlobalIllumination::GetProbeSpacing());
            shader->SetVec3("u_lightVolumeOffset", lightVolume.GetOffset());
            shader->SetVec3("u_lightVolumeWorldSize", glm::vec3(lightVolume.GetWorldSpaceWidth(), lightVolume.GetWorldSpaceHeight(), lightVolume.GetWorldSpaceDepth()));


            glActiveTexture(GL_TEXTURE11);
            glBindTexture(GL_TEXTURE_3D, lightVolume.GetLightingTextureHandle());



        OpenGLShadowMapArray* shadowMapArray = GetShadowMapArray("MoonlightCSM");

        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D_ARRAY, shadowMapArray->GetDepthTexture());

        glBindImageTexture(0, gBuffer->GetColorAttachmentHandleByName("FinalLighting"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);

        //glDispatchCompute((gBuffer->GetWidth() + 7) / 8, (gBuffer->GetHeight() + 7) / 8, 1);

        glDispatchCompute(gBuffer->GetWidth() / TILE_SIZE, gBuffer->GetHeight() / TILE_SIZE, 1);
    }
}