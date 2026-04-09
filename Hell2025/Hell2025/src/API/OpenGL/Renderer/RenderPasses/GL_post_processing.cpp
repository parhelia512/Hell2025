#include "../GL_renderer.h"
#include "AssetManagement/AssetManager.h"
#include "Renderer/Renderer.h"
#include "World/World.h"

namespace OpenGLRenderer {

    void PostProcessingPass() {
        ProfilerOpenGLZoneFunction();

        RendererSettings& rendererSettings = Renderer::GetCurrentRendererSettings();

        // Only post process the following modes
        if (rendererSettings.rendererOverrideState == RendererOverrideState::NONE || // This means the final lit image
            rendererSettings.rendererOverrideState == RendererOverrideState::CAMERA_NDOTL ||
            rendererSettings.rendererOverrideState == RendererOverrideState::INDIRECT_DIFFUSE) {
       
            OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
            OpenGLFrameBuffer* miscFullSizeFBO = GetFrameBuffer("MiscFullSize");
            OpenGLShader* shader = GetShader("PostProcessing");

            if (!miscFullSizeFBO) return;
            if (!gBuffer) return;
            if (!shader) return;

            shader->Bind();
            
            glBindImageTexture(0, gBuffer->GetColorAttachmentHandleByName("FinalLighting"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
            glBindImageTexture(1, gBuffer->GetColorAttachmentHandleByName("Normal"), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
            glBindImageTexture(2, miscFullSizeFBO->GetColorAttachmentHandleByName("ViewportIndex"), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8UI);

            glDispatchCompute(gBuffer->GetWidth() / 8, gBuffer->GetHeight() / 8, 1);
        }
    }
}