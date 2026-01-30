#include "../GL_renderer.h"
namespace OpenGLRenderer {

    void ComputeTileWorldBounds() {
        ProfilerOpenGLZoneFunction();

        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* shader = GetShader("ComputeTileWorldBounds");

        if (!gBuffer) return;
        if (!shader) return;

        shader->Bind();
        shader->BindTextureUnit(0, gBuffer->GetDepthAttachmentHandle());
        shader->SetInt("u_tileXCount", GetTileCountX());
        shader->SetInt("u_tileYCount", GetTileCountY());

        BindSSBO("RendererData", 1);
        BindSSBO("ViewportData", 2);
        BindSSBO("TileWorldBounds", 6);

        glDispatchCompute(GetTileCountX(), GetTileCountY(), 1);
    }
}