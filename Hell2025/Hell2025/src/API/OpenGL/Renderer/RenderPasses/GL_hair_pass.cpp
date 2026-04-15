#include "API/OpenGL/Renderer/GL_renderer.h"
#include "API/OpenGL/GL_backend.h"
#include "BackEnd/BackEnd.h"
#include "Config/Config.h"
#include "Viewport/ViewportManager.h"
#include "Renderer/RenderDataManager.h"

#include "AssetManagement/AssetManager.h"
#include "Audio/Audio.h"
#include "Input/Input.h"
#include "Renderer/Renderer.h"
#include "Core/Game.h"

namespace OpenGLRenderer {

    void RenderHairLayer(int peelCount);

    void UpdateHairDebugInput() {
        RendererSettings& renderSettings = Renderer::GetCurrentRendererSettings();
        int peelCount = renderSettings.depthPeelCount;
        if (Input::KeyPressed(HELL_KEY_8) && peelCount < 7) {
            renderSettings.depthPeelCount++;
            std::cout << "Depth peel layer count: " << renderSettings.depthPeelCount << "\n";
        }
        if (Input::KeyPressed(HELL_KEY_9) && peelCount > 0) {
            Audio::PlayAudio("UI_Select.wav", 1.0f);
            renderSettings.depthPeelCount--;
            std::cout << "Depth peel layer count: " << renderSettings.depthPeelCount << "\n";
        }
    }

    void HairPass() {
        ProfilerOpenGLZoneFunction();

        const DrawCommandsSet& drawInfoSet = RenderDataManager::GetDrawInfoSet();

        UpdateHairDebugInput();

        OpenGLShader* shader = GetShader("HairFinalComposite");
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLFrameBuffer* hairFrameBuffer = GetFrameBuffer("Hair");

        if (!shader) return;
        if (!gBuffer) return;
        if (!hairFrameBuffer) return;

        // Setup state
        hairFrameBuffer->Bind();
        hairFrameBuffer->ClearAttachment("Composite", 0, 0, 0, 0);
        hairFrameBuffer->SetViewport();

        glBindVertexArray(OpenGLBackEnd::GetVertexDataVAO());

        RendererSettings& renderSettings = Renderer::GetCurrentRendererSettings();

        // Render all top then all Bottom layers
        RenderHairLayer(renderSettings.depthPeelCount);

        shader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hairFrameBuffer->GetColorAttachmentHandleByName("Composite"));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gBuffer->GetColorAttachmentHandleByName("FinalLighting"));

        OpenGLFrameBuffer* waterFrameBuffer = GetFrameBuffer("Water");
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, waterFrameBuffer->GetColorAttachmentHandleByName("Color"));

        glBindImageTexture(0, gBuffer->GetColorAttachmentHandleByName("FinalLighting"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
        glDispatchCompute((gBuffer->GetWidth() + 7) / 8, (gBuffer->GetHeight() + 7) / 8, 1);
    }

    void RenderHairLayer(int peelCount) {
        const DrawCommandsSet& drawInfoSet = RenderDataManager::GetDrawInfoSet();
        const Resolutions& resolutions = Config::GetResolutions();
        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLFrameBuffer* hairFrameBuffer = GetFrameBuffer("Hair");
        OpenGLShader* depthPeelShader = GetShader("HairDepthPeel");
        OpenGLShader* hairLightingShader = GetShader("HairLighting");
        OpenGLShader* hairLayerCompositeShader = GetShader("HairLayerComposite");
        OpenGLShadowCubeMapArray* hiResShadowMaps = GetShadowCubeMapArray("HiRes");
        OpenGLFrameBuffer* miscFullSizeFbo = GetFrameBuffer("MiscFullSize"); // Has gbuffer viewspace depth in here

        if (!gBuffer) return;
        if (!hairFrameBuffer) return;
        if (!depthPeelShader) return;
        if (!hairLightingShader) return;
        if (!miscFullSizeFbo) return;
        if (!hairLayerCompositeShader) return;
        if (!hiResShadowMaps) return;

        BlitFrameBuffer(miscFullSizeFbo, hairFrameBuffer, "ViewspaceDepth", "ViewspaceDepthPrevious", GL_COLOR_BUFFER_BIT, GL_NEAREST);

        hairFrameBuffer->Bind();

        SetRasterizerState("GeometryPass_Default");

        for (int j = 0; j < peelCount; j++) {

            depthPeelShader->Bind();

            BlitFrameBufferDepth(gBuffer, hairFrameBuffer);
            BindImageTexture(0, hairFrameBuffer->GetColorAttachmentHandleByName("ViewspaceDepthPrevious"), GL_READ_ONLY, GL_R32F);
            
            glDrawBuffer(GL_NONE);
            glDepthFunc(GL_LESS);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

            for (int i = 0; i < 4; i++) {
                Viewport* viewport = ViewportManager::GetViewportByIndex(i);
                if (viewport->IsVisible()) {
                    const std::vector<DrawIndexedIndirectCommand>& drawCommands = drawInfoSet.hair[i];

                    OpenGLRenderer::SetViewport(hairFrameBuffer, viewport);

                    if (BackEnd::RenderDocFound()) {
                        SplitMultiDrawIndirect(depthPeelShader, drawCommands, false, false);
                    }
                    else {
                        MultiDrawIndirect(drawCommands);
                    }
                }
            }

            glDepthFunc(GL_EQUAL);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            // Color pass
            hairFrameBuffer->ClearAttachment("Lighting", 0.0f, 0.0f, 0.0f, 0.0f);
            hairFrameBuffer->DrawBuffers({ "Lighting", "ViewspaceDepthPrevious" });

            hairLightingShader->Bind();
            hairLightingShader->SetVec3("u_moonlightDir", Game::GetMoonlightDirection());
            glBindTextureUnit(4, AssetManager::GetTextureByName("Flashlight2")->GetGLTexture().GetHandle());

            glActiveTexture(GL_TEXTURE9);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, hiResShadowMaps->GetDepthTexture());

            SetRasterizerState("HairLighting");

            for (int i = 0; i < 4; i++) {
                Viewport* viewport = ViewportManager::GetViewportByIndex(i);
                if (viewport->IsVisible()) {
                    const std::vector<DrawIndexedIndirectCommand>& drawCommands = drawInfoSet.hair[i];

                    OpenGLRenderer::SetViewport(hairFrameBuffer, viewport);

                    if (BackEnd::RenderDocFound()) {
                        SplitMultiDrawIndirect(hairLightingShader, drawCommands, true, false);
                    }
                    else {
                        MultiDrawIndirect(drawCommands);
                    }
                }
            }

            // Composite
            hairLayerCompositeShader->Bind();
            glBindImageTexture(0, hairFrameBuffer->GetColorAttachmentHandleByName("Lighting"), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
            glBindImageTexture(1, hairFrameBuffer->GetColorAttachmentHandleByName("Composite"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
            int workGroupsX = (hairFrameBuffer->GetWidth() + 7) / 8;
            int workGroupsY = (hairFrameBuffer->GetHeight() + 7) / 8;
            glDispatchCompute(workGroupsX, workGroupsY, 1);
        }
    }
}