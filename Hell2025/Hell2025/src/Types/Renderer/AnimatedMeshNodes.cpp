#include "AnimatedMeshNodes.h"
#include "AssetManagement/AssetManager.h"
#include "Util/Util.h"


void AnimatedMeshNodes::Init(uint64_t parentId, const std::string& modelName, const std::vector<AnimatedMeshNodeCreateInfo>& createInfoSet) {

}

void AnimatedMeshNodes::SetSkinnedModel(uint64_t parentId, std::string name) {
    m_parentId = parentId;

    SkinnedModel* ptr = AssetManager::GetSkinnedModelByName(name);
    if (ptr) {
        //std::cout << "SetSkinnedModel() " << name << " mesh count: " << m_skinnedModel->GetMeshCount() << "\n";

        m_skinnedModel = ptr;
        m_nodes.clear();
        m_woundMaskTextureIndices.resize(m_skinnedModel->GetMeshCount());

        int meshCount = m_skinnedModel->GetMeshCount();

        for (int i = 0; i < meshCount; i++) {
            SkinnedMesh* skinnedMesh = AssetManager::GetSkinnedMeshByIndex(m_skinnedModel->GetMeshIndices()[i]);
            AnimatedMeshNode& node = m_nodes.emplace_back();
            node.meshName = skinnedMesh->name;
            node.meshIndex = m_skinnedModel->GetMeshIndices()[i];


            m_woundMaskTextureIndices[i] = -1;
        }

        // Store bone indices
        //m_boneMapping.clear();
        //for (int i = 0; i < m_skinnedModel->m_nodes.size(); i++) {
        //    m_boneMapping[m_skinnedModel->m_nodes[i].name] = i;
        //}
    }
    else {
        std::cout << "Could not SetSkinnedModel(name) with name: \"" << name << "\", it does not exist\n";
    }
}

void AnimatedMeshNodes::UpdateRenderItems(const glm::mat4& modelMatrix, const std::vector<glm::mat4>& boneSkinningMatrices) {
   // if (!m_skinnedModel) return;

    int meshCount = m_nodes.size();

    m_deformingRenderItems.clear();
    m_nonDeformingRenderItems.clear();
    m_nonDeformingRenderItemsDepthPeeledTransparent.clear();

    for (int i = 0; i < meshCount; i++) {
        if (m_nodes[i].blendingMode == BlendingMode::DO_NOT_RENDER) continue;

        RenderItem renderItem;
        SkinnedMesh* mesh = AssetManager::GetSkinnedMeshByIndex(m_nodes[i].meshIndex);

        Material* material = AssetManager::GetMaterialByIndex(m_nodes[i].materialIndex);
        renderItem.baseColorTextureIndex = material->m_basecolor;
        renderItem.rmaTextureIndex = material->m_rma;
        renderItem.normalMapTextureIndex = material->m_normal;
        renderItem.modelMatrix = modelMatrix;
        renderItem.inverseModelMatrix = glm::inverse(renderItem.modelMatrix);
        renderItem.meshIndex = m_skinnedModel->GetMeshIndices()[i];
        renderItem.ignoredViewportIndex = m_ignoredViewportIndex;
        renderItem.exclusiveViewportIndex = m_exclusiveViewportIndex;
        renderItem.furLength = m_nodes[i].furLength;
        renderItem.furUVScale = m_nodes[i].furUVScale;
        renderItem.furShellDistanceAttenuation = m_nodes[i].furShellDistanceAttenuation;
        renderItem.woundMaskTexutreIndex = m_woundMaskTextureIndices[i];
        renderItem.blockScreenSpaceBloodDecals = (int)true;
        Util::PackUint64(m_parentId, renderItem.objectIdLowerBit, renderItem.objectIdUpperBit);

        if (m_woundMaskTextureIndices[i] != -1) {
            Material* wouldMaterial = AssetManager::GetMaterialByIndex(m_nodes[i].woundMaterialIndex);
            renderItem.woundBaseColorTextureIndex = wouldMaterial->m_basecolor;
            renderItem.woundNormalMapTextureIndex = wouldMaterial->m_normal;
            renderItem.woundRmaTextureIndex = wouldMaterial->m_rma;
        }

        // Put it where it belongs
        if (mesh->requiresSkinning) {
            m_deformingRenderItems.push_back(renderItem);
        }
        else {
            // Update the model matrix to include the animated bone transform
            int boneIndex = mesh->nonDeformingBoneIndex;
            renderItem.modelMatrix = modelMatrix * boneSkinningMatrices[boneIndex];
            renderItem.inverseModelMatrix = glm::inverse(renderItem.modelMatrix);

            if (mesh->name == "P90_Magazine" /* ||
                mesh->name == "Magazine_low2"*/) {
                m_nonDeformingRenderItemsDepthPeeledTransparent.push_back(renderItem);

            }
            else {
                m_nonDeformingRenderItems.push_back(renderItem);
            }
        }
    }
}