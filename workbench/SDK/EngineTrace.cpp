#include "EngineTrace.h"

float HitGroup::getDamageMultiplier(int hitGroup, const WeaponInfo* weaponData, bool hasHeavyArmor, int teamNumber) noexcept
{
    static auto mp_damage_scale_ct_head = interfaces->cvar->findVar("mp_damage_scale_ct_head");
    static auto mp_damage_scale_t_head = interfaces->cvar->findVar("mp_damage_scale_t_head");

    static auto mp_damage_scale_ct_body = interfaces->cvar->findVar("mp_damage_scale_ct_body");
    static auto mp_damage_scale_t_body = interfaces->cvar->findVar("mp_damage_scale_t_body");

    float head_damage_scale = teamNumber == 3 ? mp_damage_scale_ct_head->getFloat() : teamNumber == 2 ? mp_damage_scale_t_head->getFloat() : 1.0f;
    float body_damage_scale = teamNumber == 3 ? mp_damage_scale_ct_body->getFloat() : teamNumber == 2 ? mp_damage_scale_t_body->getFloat() : 1.0f;

    if (hasHeavyArmor)
        head_damage_scale *= 0.5f;

    switch (hitGroup)
    {
    case Head:
        return weaponData->headshotMultiplier * head_damage_scale;
    case Chest:
    case LeftArm:
    case RightArm:
        return body_damage_scale;
    case Stomach:
        return 1.25f * body_damage_scale;
    case LeftLeg:
    case RightLeg:
        return 0.75f * body_damage_scale;
    default:
        return body_damage_scale;
    }
}

bool HitGroup::isArmored(int hitGroup, bool helmet, int armorValue, bool hasHeavyArmor) noexcept
{
    if (armorValue <= 0)
        return false;

    switch (hitGroup) 
    {
    case Head:
        return helmet || hasHeavyArmor;
    case Chest:
    case Stomach:
    case LeftArm:
    case RightArm:
        return true;
    default:
        return hasHeavyArmor;
    }
}