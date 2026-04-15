#pragma once
#include <Hell/Constants.h>
#include <Hell/CreateInfo.h>
#include <Hell/Enums.h>
#include <Hell/Types.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Types/Renderer/SkinnedModel.h"

struct AnimatedMeshNodeCreateInfo {
    std::string meshName;
    std::string materialName = UNDEFINED_STRING;
    BlendingMode blendingMode = BlendingMode::DEFAULT;
    bool castShadows = true;
};

struct AnimatedMeshNode {
    std::string meshName;
    int materialIndex = 0;
    int woundMaterialIndex = 0;
    int emissiveColorTexutreIndex = -1;
    bool renderAsGlass = false;
    int meshIndex = -1;
    float furLength = 0.0f;
    float furShellDistanceAttenuation = 0.0f;
    float furUVScale = 0.0f;
    BlendingMode blendingMode = BlendingMode::DEFAULT;
};

struct AnimatedMeshNodes {
    void Init(uint64_t parentId, const std::string& modelName, const std::vector<AnimatedMeshNodeCreateInfo>& createInfoSet);
    void UpdateRenderItems(const glm::mat4& modelMatrix, const std::vector<glm::mat4>& boneSkinningMatrices);

    void SetSkinnedModel(uint64_t parentId, std::string name); // temp

    uint64_t m_parentId = 0;
    std::vector<AnimatedMeshNode> m_nodes;
    uint32_t m_ignoredViewportIndex = -1;
    uint32_t m_exclusiveViewportIndex = -1;

    std::vector<int32_t> m_woundMaskTextureIndices;

    std::vector<RenderItem> m_deformingRenderItems;
    std::vector<RenderItem> m_nonDeformingRenderItems;
    std::vector<RenderItem> m_nonDeformingRenderItemsDepthPeeledTransparent;

    SkinnedModel* m_skinnedModel = nullptr;
};