#include "Player.h"
#include "AssetManagement/AssetManager.h"
#include "Bible/Bible.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderDataManager.h"
#include "Viewport/ViewportManager.h"
#include "Util.h"

// remove me
#include "Input/Input.h"

void Player::UpdateViewWeaponVisibility() {
    AnimatedGameObject* viewWeapon = GetViewWeaponAnimatedGameObject();
    if (!viewWeapon) return;

    bool shouldRenderViewWeapon = true;

    if (InventoryIsOpen() && GetInvetoryState() == InventoryState::EXAMINE_ITEM) shouldRenderViewWeapon = false;
    if (InventoryIsOpen() && GetInvetoryState() == InventoryState::EXAMINE_ITEM) shouldRenderViewWeapon = false;
    if (IsInShop())                                                              shouldRenderViewWeapon = false;

    if (shouldRenderViewWeapon) {
        viewWeapon->EnableRendering();
    }
    else {
        viewWeapon->DisableRendering();
    }

    // Temporarily always render for all viewports
    viewWeapon->SetExclusiveViewportIndex(-1);
}

/*
const glm::mat4& AnimatedGameObject::GetSkinningMatrixByBoneName(const std::string& boneName) {
    int boneIndex = GetBoneIndex(boneName);

    if (boneIndex < 0 || boneIndex >= m_boneSkinningMatrices.size()) {
        static glm::mat4 identity(1.0f);
        return identity;
    }

    return m_boneSkinningMatrices[boneIndex];
}
*/

void Player::UpdateWeaponAttachments() {
    //WeaponAttachmentInfo* weaponAttachmentInfo = Bible::GetWeaponAttachmentInfoByName(weaponAttachmentName);
    AnimatedGameObject* viewWeapon = GetViewWeaponAnimatedGameObject();

    if (!viewWeapon || Util::IsNaN(viewWeapon->GetModelMatrix())) {
        return;
    }

    {
        const glm::mat4 modelMatrix = viewWeapon->GetBoneWorldMatrixWithBoneOffset("Sight");
        m_redDot.Update(modelMatrix);
        RenderDataManager::SubmitRenderItems(m_redDot.GetRenderItems());
    }
    {
        const glm::mat4 modelMatrix = viewWeapon->GetBoneWorldMatrixWithBoneOffset("Suppressor");
        m_supressor.Update(modelMatrix);
        RenderDataManager::SubmitRenderItems(m_supressor.GetRenderItems());
    }
}

void Player::SubmitRenderItems() {
    WeaponInfo* weaponInfo = GetCurrentWeaponInfo();
    WeaponState* weaponState = GetWeaponStateByName(weaponInfo->itemInfoName);
    AnimatedGameObject* viewWeapon = GetViewWeaponAnimatedGameObject();
    Viewport* viewport = ViewportManager::GetViewportByIndex(m_viewportIndex);

    if (!weaponState) return;
    if (!weaponInfo) return;
    if (!viewWeapon) return;
    if (!viewport) return;
    if (!viewport->IsVisible()) return;

    //                   HELLOOOOOOOOO              if (ShouldRenderViewWeapon() && weaponState->hasSilencer) {
    //                   HELLOOOOOOOOO                  SubmitAttachmentRenderItem(weaponInfo->silencerName);
    //                   HELLOOOOOOOOO              }
    //                   HELLOOOOOOOOO              
    //                   HELLOOOOOOOOO              if (ShouldRenderViewWeapon() && weaponState->hasSight) {
    //                   HELLOOOOOOOOO                  SubmitAttachmentRenderItem(weaponInfo->sightName);
    //                   HELLOOOOOOOOO              }       
}