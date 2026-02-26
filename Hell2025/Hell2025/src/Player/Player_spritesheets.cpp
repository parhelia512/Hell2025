#include "Player.h"
#include "Util.h"
#include "Renderer/Renderer.h"

void Player::UpdateSpriteSheets(float deltaTime) {
    // Clear any render items from previous frame
    m_spriteSheetRenderItems.clear();

    // Muzzle flash
    if (ViewportIsVisible() && GetCurrentWeaponType() != WeaponType::MELEE) {
        AnimatedGameObject* viewWeapon = GetViewWeaponAnimatedGameObject();
        WeaponInfo* weaponInfo = GetCurrentWeaponInfo();
        if (!weaponInfo) {
            std::cout << "Player::UpdateSpriteSheets(float deltaTime) failed: weaponInfo was nullptr!\n";
            return;
        }
        glm::vec3 boneWorldPosition = viewWeapon->GetModelMatrix() * viewWeapon->GetGlobalBlendedNodeTransfrom(weaponInfo->muzzleFlashBoneName)[3];
        m_muzzleFlash.SetPosition(boneWorldPosition);
        m_muzzleFlash.Update(deltaTime);

        //Renderer::DrawPoint(boneWorldPosition, YELLOW);

        if (m_muzzleFlash.IsRenderingEnabled() && m_muzzleFlash.GetTimeAsPercentage() < 0.1325f) {
            m_spriteSheetRenderItems.push_back(m_muzzleFlash.GetRenderItem());
        }
    }
}