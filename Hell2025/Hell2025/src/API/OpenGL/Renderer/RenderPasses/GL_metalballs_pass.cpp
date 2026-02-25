#include "API/OpenGL/Renderer/GL_renderer.h"
#include "API/OpenGL/GL_backend.h"
#include "Renderer/RenderDataManager.h"

#include "HellLogging.h"

namespace OpenGLRenderer {

    void MetaBallsPass() {
        ProfilerOpenGLZoneFunction();

        const std::vector<ViewportData>& viewportData = RenderDataManager::GetViewportData();

        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* shader = GetShader("MetaBalls");

        if (!gBuffer) return;
        if (!shader) return;

        int i = 0;

        shader->Bind();

        shader->BindImageTexture(0, gBuffer->GetColorAttachmentHandleByName("BaseColor"), GL_READ_WRITE, GL_RGBA8);
        shader->BindImageTexture(1, gBuffer->GetColorAttachmentHandleByName("Normal"), GL_READ_WRITE, GL_RGBA16F);
        shader->BindImageTexture(2, gBuffer->GetColorAttachmentHandleByName("RMA"), GL_READ_WRITE, GL_RGBA8);
        shader->BindTextureUnit(3, gBuffer->GetDepthAttachmentHandle());

        int size = 16;
        int width = gBuffer->GetWidth();
        int height = gBuffer->GetHeight();
        int groupsX = (width + size - 1) / size;
        int groupsY = (height + size - 1) / size;

        glDispatchCompute(groupsX, groupsY, 1);
    }
}