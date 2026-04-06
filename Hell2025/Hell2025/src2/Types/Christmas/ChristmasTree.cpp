#include "ChristmasTree.h"
#include "ChristmasPresent.h"
#include "AssetManagement/AssetManager.h"
#include "Renderer/Renderer.h"
#include "World/World.h"
#include "Util/Util.h"

ChristmasTree::ChristmasTree(const ChristmasTreeCreateInfo& createInfo, const SpawnOffset& spawnOffset) {
    m_position = createInfo.position + spawnOffset.translation;
    m_rotation = createInfo.rotation + glm::vec3(0.0f, spawnOffset.yRotation, 0.0f);
    m_createInfo = createInfo;

    Transform transform;
    transform.position = m_position;
    transform.rotation = m_rotation;
    m_modelMatrix = transform.to_mat4();

    CreateRenderItems();
    SpawnPresents(glm::vec3(0.0f, 1.0f, 0.0f));


    ChristmasLightsCreateInfo christmasLightsCreateInfo;
    //christmasLightsCreateInfo.start = createInfo.position;
    //christmasLightsCreateInfo.end = createInfo.position;


    christmasLightsCreateInfo.sprialTopCenter = createInfo.position + glm::vec3(-0.08f, 1.7f, -0.03f);
    christmasLightsCreateInfo.spiral = true;



    World::AddChristmasLights(christmasLightsCreateInfo, spawnOffset);
}

void ChristmasTree::CreateRenderItems() {
    // Build render items
    m_renderItems.clear();

    m_model = AssetManager::GetModelByName("ChristmasTree");
    if (!m_model) {
        std::cout << "Could not get ChristmasTree model\n";
        return;
    }

    m_material = AssetManager::GetMaterialByName("ChristmasTree");
    if (!m_material) {
        std::cout << "Could not get ChristmasTree mateial\n";
        return;
    }

    for (uint32_t meshIndex : m_model->GetMeshIndices()) {
        RenderItem& renderItem = m_renderItems.emplace_back();
        renderItem.objectType = (int)ObjectType::GAME_OBJECT;
        renderItem.modelMatrix = m_modelMatrix;
        renderItem.inverseModelMatrix = glm::inverse(renderItem.modelMatrix);
        renderItem.meshIndex = meshIndex;
        renderItem.castShadows = false;
        if (m_material) {
            renderItem.baseColorTextureIndex = m_material->m_basecolor;
            renderItem.normalMapTextureIndex = m_material->m_normal;
            renderItem.rmaTextureIndex = m_material->m_rma;
        }
        Util::UpdateRenderItemAABB(renderItem);
    }
}

void ChristmasTree::Update(float deltaTime) {
    // Nothing as of yet
}

void ChristmasTree::CleanUp() {
    // Nothing as of yet
}

void ChristmasTree::SpawnPresents(glm::vec3 localOffset) {
    int w = 4;
    int h = 4;
    int d = 4;
    float spacing = 0.25f;

    for (int x = -w / 2; x < w / 2; x++) {
        for (int y = -h / 2; y < h / 2; y++) {
            for (int z = -d / 2; z < d / 2; z++) {
                ChristmasPresentCreateInfo createInfo;
                createInfo.position = m_position + localOffset + (glm::vec3(x, y, z) * spacing);
                createInfo.rotation.x = Util::RandomFloat(0.0f, HELL_PI);
                createInfo.rotation.y = Util::RandomFloat(0.0f, HELL_PI);
                createInfo.rotation.z = Util::RandomFloat(0.0f, HELL_PI);
                createInfo.type = ChristmasPresentType::SMALL;
                World::AddChristmasPresent(createInfo);
            }
        }
    }
}