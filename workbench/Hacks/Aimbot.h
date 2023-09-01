#pragma once

#include "../SDK/Entity.h"
#include "../SDK/Vector.h"
#include "../SDK/EngineTrace.h"
#include "../includes.hpp"

struct UserCmd;
struct Vector;
struct SurfaceData;
struct StudioBbox;
struct StudioHitboxSet;

struct Hitboxess
{
    //"LeftUpperArm","LeftForearm","RightCalf","RightThigh","LeftCalf","LeftThigh"
    bool head{ false };
    bool upperChest{ false };
    bool chest{ false };
    bool lowerChest{ false };
    bool stomach{ false };
    bool pelvis{ false };
    bool rightUpperArm{ false };
    bool rightForeArm{ false };
    bool leftUpperArm{ false };
    bool leftForeArm{ false };
    bool rightCalf{ false };
    bool rightThigh{ false };
    bool leftCalf{ false };
    bool leftThigh{ false };
};

namespace Aimbot
{
    void run(UserCmd* cmd) noexcept;
    void updateInput() noexcept;

    Vector calculateRelativeAngle(const Vector& source, const Vector& destination, const Vector& viewAngles) noexcept;

    bool shouldStop = false;

    struct Enemies 
    {
        int id;
        int health;
        float distance;
        float fov;

        bool operator<(const Enemies& enemy) const noexcept
        {
            if (health != enemy.health)
                return health < enemy.health;

            if (fov != enemy.fov)
                return fov < enemy.fov;

            return distance < enemy.distance;
        }

        Enemies(int id, int health, float distance, float fov) noexcept : id(id), health(health), distance(distance), fov(fov) { }
    };

    struct 
    {
        bool operator()(Enemies a, Enemies b) const
        {
            return a.health < b.health;
        }
    } healthSort;

    struct 
    {
        bool operator()(Enemies a, Enemies b) const
        {
            return a.distance < b.distance;
        }
    } distanceSort;

    struct 
    {
        bool operator()(Enemies a, Enemies b) const
        {
            return a.fov < b.fov;
        }
    } fovSort;
}