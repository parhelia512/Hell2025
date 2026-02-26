#include "Player.h"
#include "AssetManagement/AssetManager.h"
#include "Bible/Bible.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderDataManager.h"
#include "Viewport/ViewportManager.h"
#include "Util.h"

#include "Input/Input.h"


void Player::UpdateWeaponAttachments() {
	AnimatedGameObject* viewWeapon = GetViewWeaponAnimatedGameObject();
	SkinnedModel* skinnedModel = viewWeapon->GetSkinnedModel();

	if (!viewWeapon || Util::IsNaN(viewWeapon->GetModelMatrix())) {
		return;
	}
	{
		glm::mat4 globalBlendedNodeTransform = viewWeapon->GetGlobalBlendedNodeTransfrom("Sight");
		glm::mat4 boneOffset = skinnedModel->GetBoneOffset("Sight");
		glm::mat4 modelMatrix = viewWeapon->GetModelMatrix();

		glm::mat4 finalMatrix = modelMatrix * globalBlendedNodeTransform * boneOffset;
		m_redDot.Update(finalMatrix);

		//const glm::mat4 modelMatrix = viewWeapon->GetBoneWorldMatrixWithBoneOffset("Sight");
		//m_redDot.Update(modelMatrix);
		RenderDataManager::SubmitRenderItems(m_redDot.GetRenderItems());
	}
	{
		const glm::mat4 modelMatrix = viewWeapon->GetBoneWorldMatrixWithBoneOffset("Suppressor");
		m_supressor.Update(modelMatrix);
		RenderDataManager::SubmitRenderItems(m_supressor.GetRenderItems());
	}
}