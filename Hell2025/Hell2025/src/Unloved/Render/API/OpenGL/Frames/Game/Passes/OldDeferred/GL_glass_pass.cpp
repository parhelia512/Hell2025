#include "Unloved/Render/API/OpenGL/GL_renderer.h"
#include "Hell/Render/API/OpenGL/GL_back_end.h"
#include "Unloved/Session/Session.h"
#include "Unloved/Render/RenderDataManager.h"
#include "Unloved/Viewport/ViewportManager.h"
#include "World/LegacyWorld.h"

#include "Hell/ResourceManagement/ResourceManager.h"

#include "res/shaders/common/OpenGL/GL_binding_indices.glsl"


#include "Hell/Input.h"
#include "Unloved/Debug/Debug.h"

namespace OpenGL::Renderer {

    static int32_t g_glassMode = 0;
    void GlassMode0();
    void GlassMode1();
    void GlassMode2();
    void GlassMode3();

    void GlassPass() {

        if (Hell::Input::KeyPressed(HELL_KEY_1)) { g_glassMode = 0; Debug::BlitQuickDebugMessage("Glass Mode " + std::to_string(g_glassMode)); }
        if (Hell::Input::KeyPressed(HELL_KEY_2)) { g_glassMode = 1; Debug::BlitQuickDebugMessage("Glass Mode " + std::to_string(g_glassMode)); }
        if (Hell::Input::KeyPressed(HELL_KEY_3)) { g_glassMode = 2; Debug::BlitQuickDebugMessage("Glass Mode " + std::to_string(g_glassMode)); }
        if (Hell::Input::KeyPressed(HELL_KEY_4)) { g_glassMode = 3; Debug::BlitQuickDebugMessage("Glass Mode " + std::to_string(g_glassMode)); }

        if (g_glassMode == 0) GlassMode0();
        if (g_glassMode == 1) GlassMode1();
        if (g_glassMode == 2) GlassMode2();
        if (g_glassMode == 3) GlassMode3();
    }

    void GlassMode0() {
        //ProfilerOpenGLZoneFunction();
        ProfilerOpenGLZoneFunctionRed();

        OpenGL::RasterizerStateManager::ForceRasterizerState("GlassPass");

        const DrawCommandsSet& drawInfoSet = Unloved::RenderDataManager::GetDrawInfoSet();
        const std::vector<ViewportData>& viewportData = Unloved::RenderDataManager::GetViewportData();

        OpenGLShader* shader = OpenGL::ResourceManager::GetShaderPtr("Glass");
        OpenGLShader* compositeShader = OpenGL::ResourceManager::GetShaderPtr("GlassComposite");
        OpenGLFrameBuffer* gBuffer = OpenGL::ResourceManager::GetFrameBufferPtr("GBuffer");
        OpenGLShadowMap* flashLightShadowMapsFBO = OpenGL::ResourceManager::GetShadowMapPtr("FlashlightShadowMaps");

        if (!shader) return;
        if (!compositeShader) return;
        if (!gBuffer) return;
        if (!flashLightShadowMapsFBO) return;

        // TODO: explicitly bind all other ssbos used by this render pass
        OpenGL::BindSSBO(1, "Materials");

        OpenGL::BindShader("Glass");
        OpenGL::SetUniformBool("u_flipNormalMapY", ShouldFlipNormalMapY());

        gBuffer->Bind();
        gBuffer->DrawBuffer("Glass");

        glBindVertexArray(OpenGL::ResourceManager::GetMeshBuffer("AssetGeometry").GetVAO());
        glBindTextureUnit(7, GetTextureHandleByName("Flashlight2"));
        glBindTextureUnit(TEX_IDX_SHADOW_MAP_FLASHLIGHT, flashLightShadowMapsFBO->GetDepthTextureHandle());

        // Forward render each glass render item into each viewport
        for (int i = 0; i < 4; i++) {
            Unloved::Viewport* viewport = Unloved::ViewportManager::GetViewportByIndex(i);
            if (!viewport->IsVisible()) continue;

            OpenGL::Renderer::SetViewport(gBuffer, viewport);
            OpenGL::SetUniformInt("u_viewportIndex", i);

            Unloved::Player* player = Unloved::Session::GetLocalPlayerByViewportIndex(i);

            for (const RenderItem& renderItem : drawInfoSet.glassRenderItemsOLD[i]) {
                OpenGL::SetUniformMat4("u_modelMatrix", renderItem.modelMatrix);

                Mesh* mesh = Hell::ResourceManager::GetMeshBuffer("AssetGeometry").GetMeshById(renderItem.meshId);
                if (!mesh) continue;

                Material* material = Hell::ResourceManager::GetMaterialByIndex(renderItem.materialIndex);
                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_2D, Hell::ResourceManager::GetTextureByBindlessIndex(material->m_basecolor)->GetGLTexture().GetHandle());
                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_2D, Hell::ResourceManager::GetTextureByBindlessIndex(material->m_normal)->GetGLTexture().GetHandle());
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_2D, Hell::ResourceManager::GetTextureByBindlessIndex(material->m_rma)->GetGLTexture().GetHandle());

                glDrawElementsBaseVertex(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * mesh->baseIndex), mesh->baseVertex);
            }
        }

        // Composite that render back into the lighting texture
        gBuffer->SetViewport();
        OpenGL::BindShader("GlassComposite");
        glBindImageTexture(0, gBuffer->GetColorAttachmentHandleByName("Lighting"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
        glBindImageTexture(1, gBuffer->GetColorAttachmentHandleByName("Glass"), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        OpenGL::DispatchCompute(gBuffer->GetWidth() / 16, gBuffer->GetHeight() / 4, 1);

        glDepthMask(GL_TRUE);
    }

    void GlassMode1() {
        // ProfilerOpenGLZoneFunction();
        ProfilerOpenGLZoneFunctionRed();

        OpenGL::RasterizerStateManager::ForceRasterizerState("GlassPass");

        const DrawCommandsSet& drawInfoSet = Unloved::RenderDataManager::GetDrawInfoSet();
        const std::vector<ViewportData>& viewportData = Unloved::RenderDataManager::GetViewportData();

        OpenGLShader* shader = OpenGL::ResourceManager::GetShaderPtr("Glass");
        OpenGLShader* compositeShader = OpenGL::ResourceManager::GetShaderPtr("GlassComposite");
        OpenGLFrameBuffer* gBuffer = OpenGL::ResourceManager::GetFrameBufferPtr("GBuffer");
        OpenGLShadowMap* flashLightShadowMapsFBO = OpenGL::ResourceManager::GetShadowMapPtr("FlashlightShadowMaps");

        if (!shader) return;
        if (!compositeShader) return;
        if (!gBuffer) return;
        if (!flashLightShadowMapsFBO) return;

        // TODO: explicitly bind all other ssbos used by this render pass
        OpenGL::BindSSBO(SSBO_IDX_MATERIALS, "Materials");

        OpenGL::BindShader("Glass");
        OpenGL::SetUniformBool("u_flipNormalMapY", ShouldFlipNormalMapY());

        gBuffer->Bind();
        gBuffer->DrawBuffer("Lighting");

        OpenGLRasterizerState state;
        state.depthTestEnabled = true;
        state.blendEnable = true;
        state.cullfaceEnable = false;
        state.depthMask = false;
        state.colorMask = true;
        state.depthFunc = GL_GREATER;
        state.blendFuncSrcfactor = GL_ONE;
        state.blendFuncDstfactor = GL_ONE;
        OpenGL::RasterizerStateManager::SetRasterizerState(state);

        glBindVertexArray(OpenGL::ResourceManager::GetMeshBuffer("AssetGeometry").GetVAO());
        glBindTextureUnit(7, GetTextureHandleByName("Flashlight2"));
        glBindTextureUnit(TEX_IDX_SHADOW_MAP_FLASHLIGHT, flashLightShadowMapsFBO->GetDepthTextureHandle());

        // Forward render each glass render item into each viewport
        // for (int i = 0; i < 4; i++) {
        //     Unloved::Viewport* viewport = Unloved::ViewportManager::GetViewportByIndex(i);
        //     if (!viewport->IsVisible()) continue;
        //
        //     OpenGL::Renderer::SetViewport(gBuffer, viewport);
        //     OpenGL::SetUniformInt("u_viewportIndex", i);
        //
        //     for (const RenderItem& renderItem : drawInfoSet.glassRenderItemsOLD[i]) {
        //         OpenGL::SetUniformMat4("u_modelMatrix", renderItem.modelMatrix);
        //
        //         Mesh* mesh = Hell::ResourceManager::GetMeshBuffer("AssetGeometry").GetMeshById(renderItem.meshId);
        //         if (!mesh) continue;
        //
        //         Material* material = Hell::ResourceManager::GetMaterialByIndex(renderItem.materialIndex);
        //         glActiveTexture(GL_TEXTURE4);
        //         glBindTexture(GL_TEXTURE_2D, Hell::ResourceManager::GetTextureByBindlessIndex(material->m_basecolor)->GetGLTexture().GetHandle());
        //         glActiveTexture(GL_TEXTURE5);
        //         glBindTexture(GL_TEXTURE_2D, Hell::ResourceManager::GetTextureByBindlessIndex(material->m_normal)->GetGLTexture().GetHandle());
        //         glActiveTexture(GL_TEXTURE6);
        //         glBindTexture(GL_TEXTURE_2D, Hell::ResourceManager::GetTextureByBindlessIndex(material->m_rma)->GetGLTexture().GetHandle());
        //
        //         glDrawElementsBaseVertex(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * mesh->baseIndex), mesh->baseVertex);
        //     }
        // }


        OpenGL::BindSSBO(11, "GlassInstanceData");

        for (int i = 0; i < 4; i++) {
            Unloved::Viewport* viewport = Unloved::ViewportManager::GetViewportByIndex(i);
            if (!viewport->IsVisible()) continue;

            OpenGL::Renderer::SetViewport(gBuffer, viewport);
            OpenGL::SetUniformInt("u_viewportIndex", i);

            MultiDrawIndirect(drawInfoSet.glass[i]);
        }

        // Composite that render back into the lighting texture
        //gBuffer->SetViewport();
        //glBindImageTexture(0, gBuffer->GetColorAttachmentHandleByName("Lighting"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
        //glBindImageTexture(1, gBuffer->GetColorAttachmentHandleByName("Glass"), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        //OpenGL::DispatchCompute(gBuffer->GetWidth() / 16, gBuffer->GetHeight() / 4, 1);
        //
        //glDepthMask(GL_TRUE);
    }


    void GlassMode2() {
        ProfilerOpenGLZoneFunctionRed();

    }

    void GlassMode3() {
        ProfilerOpenGLZoneFunctionRed();

    }
}
