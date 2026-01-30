#include "../GL_renderer.h"

namespace OpenGLRenderer {

    void ComputeTileWorldBounds() {
        ProfilerOpenGLZoneFunction();

        OpenGLFrameBuffer* gBuffer = GetFrameBuffer("GBuffer");
        OpenGLShader* shader = GetShader("LightCulling");

        if (!gBuffer) return;
        if (!shader) return;

        shader->Bind();
        shader->BindTextureUnit(0, gBuffer->GetDepthAttachmentHandle());
        shader->SetInt("u_tileXCount", GetTileCountX());
        shader->SetInt("u_tileYCount", GetTileCountY());

        BindSSBO("TileWorldBounds", 6);

        glDispatchCompute(GetTileCountX(), GetTileCountY(), 1);
    }
}