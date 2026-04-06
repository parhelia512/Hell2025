#pragma once
#include <Hell/CreateInfo.h>
#include "glm/vec3.hpp"

struct Bullet {
    Bullet() = default;
    Bullet(BulletCreateInfo& createInfo) {
        m_origin = createInfo.origin;
        m_direction = createInfo.direction;
        m_weaponIndex = createInfo.weaponIndex;
        m_damage = createInfo.damage;
        m_ownerObjectId = createInfo.ownerObjectId;
        m_rayLength = createInfo.rayLength;
        m_createsDecals = createInfo.createsDecals;
        m_createsFollowThroughBulletOnGlassHit = createInfo.createsFollowThroughBulletOnGlassHit;
        m_playsPiano = createInfo.playsPiano;
        m_createsDecalTexturePaintedWounds = createInfo.createsDecalTexturePaintedWounds;
    }

    const bool PlaysPiano() const { return m_playsPiano; }
    const bool CreatesDecalTexturePaintedWounds() const     { return m_createsDecalTexturePaintedWounds; }
    const bool CreatesDecals() const                        { return m_createsDecals; }
    const bool CreatesFolloWThroughBulletOnGlassHit() const { return m_createsFollowThroughBulletOnGlassHit; }
    const float GetRayLength() const                        { return m_rayLength; }
    const glm::vec3 GetOrigin() const                       { return m_origin; }
    const glm::vec3 GetDirection() const                    { return m_direction; }
    const int32_t GetWeaponIndex() const                    { return m_weaponIndex; }
    const uint32_t GetDamage() const                        { return m_damage; }
    const uint64_t GetOwnerObjectId() const                 { return m_ownerObjectId; }

private:
    bool m_playsPiano = true;
    float m_rayLength = 1000.0f;
    glm::vec3 m_origin = glm::vec3(0);
    glm::vec3 m_direction = glm::vec3(0);
    int32_t m_weaponIndex = 0;
    uint32_t m_damage = 0;
    uint64_t m_ownerObjectId = 0;
    bool m_createsDecals = true;
    bool m_createsFollowThroughBulletOnGlassHit = true;
    bool m_createsDecalTexturePaintedWounds = true;
};