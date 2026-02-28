#include "Player.h"
#include "Audio/Audio.h"
#include "Bible/Bible.h"


void Player::UseItem(const std::string& itemName) {
	ItemInfo* itemInfo = Bible::GetItemInfoByName(itemName);
	if (!itemInfo || !itemInfo->IsUsable()) return;

	if (itemInfo->GetType() == ItemType::HEAL) {
		m_health += itemInfo->m_healInfo.amount;
		m_health = std::min(m_health, 100);
		Audio::PlayAudio("Heal.wav", 1.0f);
	}
}

bool Player::CanUseItem(const std::string& itemName) {
	ItemInfo* itemInfo = Bible::GetItemInfoByName(itemName);
	if (!itemInfo || !itemInfo->IsUsable()) return false;

	// If you have less than 100 you can use a healing item
	if (itemInfo->GetType() == ItemType::HEAL && m_health < 100) return true;

	return false;
}