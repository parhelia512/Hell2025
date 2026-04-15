#include "Player.h"
#include "Input/Input.h"

void Player::InitCharacterModel() {
    AnimatedGameObject* characterModel = GetCharacterModelAnimatedGameObject();
    if (!characterModel) return;

    characterModel->SetSkinnedModel("UniSexGuyScaled");
    characterModel->SetMeshMaterialByMeshName("CC_Base_Body", "UniSexGuyBody");
    characterModel->SetMeshMaterialByMeshName("CC_Base_Eye", "UniSexGuyBody");
    characterModel->SetMeshMaterialByMeshName("Biker_Jeans", "UniSexGuyJeans");
    characterModel->SetMeshMaterialByMeshName("CC_Base_Eye", "UniSexGuyEyes");
    characterModel->SetMeshMaterialByMeshName("Glock", "Glock");
    characterModel->SetMeshMaterialByMeshName("SM_Knife_01", "Knife");
    characterModel->SetMeshMaterialByMeshName("Shotgun_Mesh", "Shotgun");
    characterModel->SetMeshMaterialByMeshIndex(13, "UniSexGuyHead");
    characterModel->SetMeshMaterialByMeshIndex(14, "UniSexGuyLashes");
    //characterModel->EnableBlendingByMeshIndex(14);
    characterModel->SetMeshMaterialByMeshName("FrontSight_low", "AKS74U_0");
    characterModel->SetMeshMaterialByMeshName("Receiver_low", "AKS74U_1");
    characterModel->SetMeshMaterialByMeshName("BoltCarrier_low", "AKS74U_1");
    characterModel->SetMeshMaterialByMeshName("SafetySwitch_low", "AKS74U_0");
    characterModel->SetMeshMaterialByMeshName("MagRelease_low", "AKS74U_0");
    characterModel->SetMeshMaterialByMeshName("Pistol_low", "AKS74U_2");
    characterModel->SetMeshMaterialByMeshName("Trigger_low", "AKS74U_1");
    characterModel->SetMeshMaterialByMeshName("Magazine_Housing_low", "AKS74U_3");
    characterModel->SetMeshMaterialByMeshName("BarrelTip_low", "AKS74U_4");
}

void Player::UpdateCharacterModelHacks() {
    AnimatedGameObject* characterModel = GetCharacterModelAnimatedGameObject();
    WeaponInfo* weaponInfo = GetCurrentWeaponInfo();
    WeaponState* weaponState = GetCurrentWeaponState();

    if (!characterModel) return;
    if (!weaponInfo) return;
    if (!weaponState) return;

    characterModel->EnableDrawingForAllMesh();

    if (IsAlive()) {

        if (weaponInfo->type == WeaponType::MELEE) {
            HideAKS74UMesh();
            HideGlockMesh();
            HideShotgunMesh();
            if (IsMoving()) {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_Knife_Walk", 1.0f);
            }
            else {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_Knife_Idle", 1.0f);
            }
            if (IsCrouching()) {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_Knife_Crouch", 1.0f);
            }
        }
        if (weaponInfo->type == WeaponType::PISTOL) {
            HideAKS74UMesh();
            HideShotgunMesh();
            HideKnifeMesh();
            if (IsMoving()) {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_Glock_Walk", 1.0f);
            }
            else {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_Glock_Idle", 1.0f);
            }
            if (IsCrouching()) {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_Glock_Crouch", 1.0f);
            }
        }
        if (weaponInfo->type == WeaponType::AUTOMATIC) {
            HideShotgunMesh();
            HideKnifeMesh();
            HideGlockMesh();
            if (IsMoving()) {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_AKS74U_Walk", 1.0f);
            }
            else {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_AKS74U_Idle", 1.0f);
            }
            if (IsCrouching()) {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_AKS74U_Crouch", 1.0f);
            }
        }
        if (weaponInfo->type == WeaponType::SHOTGUN) {
            HideAKS74UMesh();
            HideKnifeMesh();
            HideGlockMesh();
            if (IsMoving()) {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_Shotgun_Walk", 1.0f);
            }
            else {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_Shotgun_Idle", 1.0f);
            }
            if (IsCrouching()) {
                characterModel->PlayAndLoopAnimation("MainLayer", "UnisexGuy_Shotgun_Crouch", 1.0f);
            }
        }

        characterModel->SetPosition(GetFootPosition());
        characterModel->SetRotationY(m_camera.GetEulerRotation().y + HELL_PI);
    }
    else {
        HideKnifeMesh();
        HideGlockMesh();
        HideShotgunMesh();
        HideAKS74UMesh();
    }
}

void Player::HideKnifeMesh() {
    AnimatedGameObject* characterModel = GetCharacterModelAnimatedGameObject();
    if (!characterModel) return;

    characterModel->DisableDrawingForMeshByMeshName("SM_Knife_01");
}

void Player::HideGlockMesh() {
    AnimatedGameObject* characterModel = GetCharacterModelAnimatedGameObject();
    if (!characterModel) return;

    characterModel->DisableDrawingForMeshByMeshName("Glock");
}

void Player::HideShotgunMesh() {
    AnimatedGameObject* characterModel = GetCharacterModelAnimatedGameObject();
    if (!characterModel) return;

    characterModel->DisableDrawingForMeshByMeshName("Shotgun_Mesh");
}

void Player::HideAKS74UMesh() {
    AnimatedGameObject* characterModel = GetCharacterModelAnimatedGameObject();
    if (!characterModel) return;

    characterModel->DisableDrawingForMeshByMeshName("FrontSight_low");
    characterModel->DisableDrawingForMeshByMeshName("Receiver_low");
    characterModel->DisableDrawingForMeshByMeshName("BoltCarrier_low");
    characterModel->DisableDrawingForMeshByMeshName("SafetySwitch_low");
    characterModel->DisableDrawingForMeshByMeshName("MagRelease_low");
    characterModel->DisableDrawingForMeshByMeshName("Pistol_low");
    characterModel->DisableDrawingForMeshByMeshName("Trigger_low");
    characterModel->DisableDrawingForMeshByMeshName("Magazine_Housing_low");
    characterModel->DisableDrawingForMeshByMeshName("BarrelTip_low");
}