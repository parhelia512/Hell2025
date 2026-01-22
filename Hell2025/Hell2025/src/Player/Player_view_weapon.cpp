#include "Player.h"
#include "Input/InputMulti.h"
#include "Core/Debug.h"
#include "Input/Input.h"
#include "Util/Util.h"

#include "AssetManagement/AssetManager.h"

void Player::UpdateViewWeapon(float deltaTime) {    
    AnimatedGameObject* viewWeapon = GetViewWeaponAnimatedGameObject();
    if (!viewWeapon) return;


    SkinnedModel* model = viewWeapon->GetSkinnedModel();

    glm::mat4 dmMaster = glm::mat4(1);
    glm::mat4 cameraMatrix = glm::mat4(1);
    glm::mat4 cameraBindMatrix = glm::mat4(1);
    glm::mat4 root = glm::mat4(1);

    for (int i = 0; i < model->m_nodes.size(); i++) {
        if (model->m_nodes[i].name == "camera") {
            cameraBindMatrix = model->m_nodes[i].inverseBindTransform;
        }
    }

    //if (Input::KeyPressed(HELL_KEY_SPACE) && m_viewportIndex == 0) {
    //    //viewWeapon->PrintNodeNames();
    //    viewWeapon->GetSkinnedModel()->PrintNodeInfo();
    //
    //    std::cout << "count:  " << viewWeapon->GetBoneSkinningMatrices().size() << "\n";
    //    std::cout << "matrix: " << Util::Mat4ToString(viewWeapon->GetBoneWorldMatrix("Knife")) << "\n";
    //
    //    for (auto meshIndex : viewWeapon->GetSkinnedModel()->GetMeshIndices()) {
    //        SkinnedMesh* skinnedMesh = AssetManager::GetSkinnedMeshByIndex(meshIndex);
    //        if (skinnedMesh->name == "Knife") {
    //            int baseVertex = skinnedMesh->baseVertexGlobal;
    //            int vertexCount = skinnedMesh->vertexCount;
    //            auto vertices = AssetManager::GetWeightedVertices();
    //            for (int i = baseVertex; i < baseVertex + vertexCount; i++) {
    //                std::cout << vertices[i].position << " (";
    //                std::cout << vertices[i].weight.x << ", ";
    //                std::cout << vertices[i].weight.y << ", ";
    //                std::cout << vertices[i].weight.z << ", ";
    //                std::cout << vertices[i].weight.w << ") (";
    //                std::cout << vertices[i].boneID.x << ", ";
    //                std::cout << vertices[i].boneID.y << ", ";
    //                std::cout << vertices[i].boneID.z << ", ";
    //                std::cout << vertices[i].boneID.w << ") \n";
    //                break;
    //            }
    //        }
    //    }
    //
    //    std::cout << viewWeapon->GetSkinnedModel()->GetName() << " bind matrices:\n\n";
    //
    //    for (int i = 0; i < model->m_nodes.size(); i++) {
    //        if (model->m_nodes[i].name == "Dm-Master") {
    //            std::cout << "\n";
    //            std::cout << "Dm-Master\n";
    //            std::cout << Util::Mat4ToString(model->m_nodes[i].inverseBindTransform) << "\n" << "\n";
    //        }
    //    }
    //
    //    for (int i = 0; i < model->m_nodes.size(); i++) {
    //        if (model->m_nodes[i].name == "RootNode") {
    //            std::cout << "RootNode\n";
    //            std::cout << Util::Mat4ToString(model->m_nodes[i].inverseBindTransform) << "\n" << "\n";
    //        }
    //    }
    //
    //    std::cout << "Camera bind matrix\n";
    //    std::cout << Util::Mat4ToString(cameraBindMatrix) << "\n" << "\n";
    //
    //}

    // Weapon sway
    float xMax = 5.0;
    float SWAY_AMOUNT = 0.125f;
    float SMOOTH_AMOUNT = 4.0f;
    float SWAY_MIN_X = -xMax;
    float SWAY_MAX_X = xMax;
    float SWAY_MIN_Y = -2;
    float SWAY_MAX_Y = 0.95f;
    float xOffset = (float)InputMulti::GetMouseXOffset(m_mouseIndex);
    float yOffset = (float)InputMulti::GetMouseYOffset(m_mouseIndex);
    float movementX = xOffset * SWAY_AMOUNT;
    float movementY = -yOffset * SWAY_AMOUNT;


    if (GetCurrentWeaponInfo()->itemInfoName == "AKS74U") {
        xMax = 10.0f;
    }

    movementX = std::min(movementX, SWAY_MAX_X);
    movementX = std::max(movementX, SWAY_MIN_X);
    movementY = std::min(movementY, SWAY_MAX_Y);
    movementY = std::max(movementY, SWAY_MIN_Y);

    if (HasControl()) {
        m_weaponSwayX = Util::FInterpTo(m_weaponSwayX, movementX, deltaTime, SMOOTH_AMOUNT);
        m_weaponSwayY = Util::FInterpTo(m_weaponSwayY, movementY, deltaTime, SMOOTH_AMOUNT);
    }

    if (ViewportIsVisible()) {
       //Debug::AddText("m_weaponSwayX: " + std::to_string(m_weaponSwayX));
       //Debug::AddText("m_weaponSwayY: " + std::to_string(m_weaponSwayY));
       //Debug::AddText("xOffset: " + std::to_string(xOffset));
       //Debug::AddText("yOffset: " + std::to_string(yOffset));
    }

    float weaponScale = 0.001f;
    float weaponSwayScale = 0.001f;
    
    weaponScale = 0.01f;


    if (GetCurrentWeaponInfo()->itemInfoName == "Knife") {
       weaponScale = 1.0;
    }

    // Final transform
    Transform transform;
    transform.position = m_camera.GetPosition();
    transform.position += (m_weaponSwayX * weaponSwayScale) * m_camera.GetRight();
    transform.position += (m_weaponSwayY * weaponSwayScale) * m_camera.GetUp();
    transform.rotation.x = m_camera.GetEulerRotation().x;
    transform.rotation.y = m_camera.GetEulerRotation().y;
    transform.scale = glm::vec3(weaponScale);
    
    // HACK!!!!!!!!!!!!!
    glm::mat4 hackMatrix = glm::mat4(1.0f);
    if (GetCurrentWeaponInfo()->itemInfoName == "AKS74U") {
        Transform hackTransform;
        hackTransform.rotation.x += HELL_PI * 0.5f;
        hackTransform.rotation.z += HELL_PI;
        hackMatrix = hackTransform.to_mat4();
    }

    viewWeapon->SetCameraMatrix(transform.to_mat4() * glm::inverse(cameraBindMatrix) * hackMatrix * glm::inverse(dmMaster));
    viewWeapon->EnableModelMatrixOverride();
}


bool Player::ShouldRenderViewWeapon() {
    if (InventoryIsOpen() && GetInvetoryState() == InventoryState::EXAMINE_ITEM) return false;
    if (InventoryIsOpen() && GetInvetoryState() == InventoryState::EXAMINE_ITEM) return false;
    if (IsInShop())                                                              return false;

    return true;
}