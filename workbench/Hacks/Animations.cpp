#include "../Memory.h"
#include "../Interfaces.h"

#include <intrin.h>
#include "Animations.h"
#include "Backtrack.h"
#include "EnginePrediction.h"
#include "Resolver.h"
#include "AntiAim.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/Cvar.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/ConVar.h"
#include "../SDK/MemAlloc.h"
#include "../SDK/Input.h"
#include "../SDK/Vector.h"
#include "../Hooks.h"
#include "../xor.h"

enum ADVANCED_ACTIVITY : int
{
    ACTIVITY_NONE = 0,
    ACTIVITY_JUMP,
    ACTIVITY_LAND,
    ACTIVE_LAND_LIGHT,
    ACTIVE_LAND_HEAVY
};

static bool lockAngles = false;
static Vector holdAimAngles;
static Vector anglesToAnimate;

static std::array<Animations::Players, 65> players{};
static std::array<matrix3x4, MAXSTUDIOBONES> fakematrix{};
static std::array<matrix3x4, MAXSTUDIOBONES> fakelagmatrix{};
static std::array<matrix3x4, MAXSTUDIOBONES> realmatrix{};

static Vector localAngle{};
static Vector sentViewangles{};

static bool updatingLocal{ true };
static bool updatingEntity{ false };
static bool updatingFake{ false };
static bool sendPacket{ true };
static bool gotMatrix{ false };
static bool gotMatrixFakelag{ false };
static bool gotMatrixReal{ false };

static Vector viewangles{};
static Vector correctAngle{};

static int buildTransformsIndex = -1;
static std::array<AnimationLayer, 13> staticLayers{};
static std::array<AnimationLayer, 13> layers{};
static float primaryCycle{ 0.0f };
static float moveWeight{ 0.0f };
static float footYaw{};

static std::array<float, 24> poseParameters{};
static std::array<AnimationLayer, 13> sendPacketLayers{};

void Animations::init() noexcept
{
    
    static auto threadedBoneSetup = interfaces->cvar->findVar("cl_threaded_bone_setup");
    threadedBoneSetup->setValue(1);

    static auto extrapolate = interfaces->cvar->findVar("cl_extrapolate");
    extrapolate->setValue(0);
}

void Animations::reset() noexcept
{
    for (auto& record : players)
        record.reset();

    fakematrix = {};
    fakelagmatrix = {};
    localAngle = Vector{};
    updatingLocal = true;
    updatingEntity = false;
    sendPacket = true;
    gotMatrix = false;
    gotMatrixFakelag = false;
    gotMatrixReal = false;
    viewangles = Vector{};
    correctAngle = Vector{};
    buildTransformsIndex = -1;
    staticLayers = {};
    layers = {};
    primaryCycle = 0.0f;
    moveWeight = 0.0f;
    footYaw = {};
    poseParameters = {};
    sendPacketLayers = {};
}

void Animations::update(UserCmd* cmd, bool& _sendPacket) noexcept
{
    static float spawnTime = 0.f;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (interfaces->engine->isHLTV())
        return;

    if (spawnTime != localPlayer->spawnTime())
    {
        spawnTime = localPlayer->spawnTime();

        for (int i = 0; i < 13; i++)
        {
            if (i == ANIMATION_LAYER_FLINCH || i == ANIMATION_LAYER_FLASHED || i == ANIMATION_LAYER_WHOLE_BODY || i == ANIMATION_LAYER_WEAPON_ACTION || i == ANIMATION_LAYER_WEAPON_ACTION_RECROUCH)
                continue;

            auto& animLayers = *localPlayer->getAnimationLayer(i);
            if (!&animLayers)
                continue;

            animLayers.reset();
        }
    }

    if (!localPlayer->getAnimstate())
        return;

    viewangles = cmd->viewangles;
    sendPacket = _sendPacket;
    localPlayer->getAnimstate()->buttons = cmd->buttons;

    if (sendPacket)
        sentViewangles = cmd->viewangles;

    updatingLocal = true;

    // allow animations to be animated in the same frame
    if (localPlayer->getAnimstate()->lastUpdateFrame == memory->globalVars->framecount)
        localPlayer->getAnimstate()->lastUpdateFrame -= 1;

    if (localPlayer->getAnimstate()->lastUpdateTime == memory->globalVars->currenttime)
        localPlayer->getAnimstate()->lastUpdateTime += ticksToTime(1);

    localPlayer->getEFlags() &= ~0x1000;
    localPlayer->getAbsVelocity() = EnginePrediction::getVelocity();

    localPlayer->updateState(localPlayer->getAnimstate(), viewangles);
    localPlayer->updateClientSideAnimation();

    if (config->misc.moonwalk_style == 2)
    {
        static auto alpha = 1.0f;
        static auto switch_alpha = false;

        if (alpha <= 0.0f || alpha >= 1.0f)
            switch_alpha = !switch_alpha;

        alpha += switch_alpha ? 1.75f * memory->globalVars->frametime : -1.75f * memory->globalVars->frametime;
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        if (localPlayer->flags() & FL_ONGROUND)
        {
            if (switch_alpha)
            {
                localPlayer->setPoseParameter(-1, 0);
                localPlayer->setPoseParameter(1, 1);
            }
            else if (!switch_alpha)
            {
                localPlayer->setPoseParameter(1, 0);
                localPlayer->setPoseParameter(-1, 1);
            }
        }
    }

    std::memcpy(&layers, localPlayer->animOverlays(), sizeof(AnimationLayer) * localPlayer->getAnimationLayersCount());

    if (sendPacket)
    {
        Animations::sentViewangles = cmd->viewangles;

        if (config->misc.moonwalk_style == 2)
        {
            static auto alpha = 1.0f;
            static auto switch_alpha = false;

            if (alpha <= 0.0f || alpha >= 1.0f)
                switch_alpha = !switch_alpha;

            alpha += switch_alpha ? 1.75f * memory->globalVars->frametime : -1.75f * memory->globalVars->frametime;
            alpha = std::clamp(alpha, 0.0f, 1.0f);

            if (localPlayer->flags() & FL_ONGROUND)
            {
                if (switch_alpha)
                {
                    localPlayer->setPoseParameter(-1, 0);
                    localPlayer->setPoseParameter(1, 1);
                }
                else if (!switch_alpha)
                {
                    localPlayer->setPoseParameter(1, 0);
                    localPlayer->setPoseParameter(-1, 1);
                }
            }
        }

        if (config->misc.moonwalk_style == 3)
        {
            static auto alpha = 1.0f;
            static auto switch_alpha = false;

            if (alpha <= 0.0f || alpha >= 1.0f)
                switch_alpha = !switch_alpha;

            alpha += switch_alpha ? 1.75f * memory->globalVars->frametime : -1.75f * memory->globalVars->frametime;
            alpha = std::clamp(alpha, 0.0f, 1.0f);

            if (localPlayer->flags() & FL_ONGROUND)
            {
                if (switch_alpha)
                {
                    localPlayer->poseParameters()[0] = -1.f;
                    localPlayer->poseParameters()[1] = 1.f;
                }
                else if (!switch_alpha)
                {
                    localPlayer->poseParameters()[0] = 1.f;
                    localPlayer->poseParameters()[1] = -1.f;
                }
            }
        }

        bool slowwalk = config->misc.slowwalk && config->misc.slowwalkKey.isActive();
        if (config->misc.moonwalk_style == 4)
            localPlayer->setPoseParameter(0, 7);

        localPlayer->poseParameters().data()[2] = std::rand() % 4 * (float)config->condAA.moveBreakers;

        /*breakers*/
        if ((config->condAA.animBreakers & FL_ONGROUND << 0) == 1 << 0)
            localPlayer->setPoseParameter(1, 6);
        else
            localPlayer->setPoseParameter(0, 6);

        if ((config->condAA.animBreakers & FL_ONGROUND << 1) == 1 << 1)
        {
            static float endTime = memory->globalVars->currenttime;
            if (localPlayer->getAnimstate()->landing)
                endTime = memory->globalVars->currenttime + 3.5;
            else
                endTime = memory->globalVars->currenttime;
            if (endTime > memory->globalVars->currenttime)
                localPlayer->setPoseParameter(2, 12);
        }

        if ((config->condAA.animBreakers & FL_ONGROUND << 2) == 1 << 2)
        {
            localPlayer->poseParameters().data()[8] = 0;
            localPlayer->poseParameters().data()[9] = 0;
            localPlayer->poseParameters().data()[10] = 0;
        }

        if ((config->condAA.animBreakers & FL_ONGROUND << 3) == 1 << 3 && !slowwalk)
        {
            if (localPlayer->velocity().length2D() > 2.5f)
                localPlayer->getAnimationLayer(ANIMATION_LAYER_MOVEMENT_MOVE)->weight = 1.f;
        }

        std::memcpy(&sendPacketLayers, localPlayer->animOverlays(), sizeof(AnimationLayer) * localPlayer->getAnimationLayersCount());
        footYaw = localPlayer->getAnimstate()->footYaw;
        poseParameters = localPlayer->poseParameters();
        gotMatrixReal = localPlayer->setupBones(realmatrix.data(), localPlayer->getBoneCache().size, 0x7FF00, memory->globalVars->currenttime);

        const auto origin = localPlayer->getRenderOrigin();
        if (gotMatrixReal)
        {
            for (auto& i : realmatrix)
            {
                i[0][3] -= localPlayer->getRenderOrigin().x;
                i[1][3] -= localPlayer->getRenderOrigin().y;
                i[2][3] -= localPlayer->getRenderOrigin().z;
            }
        }
        localAngle = cmd->viewangles;
    }

    updatingLocal = false;
}

void Animations::fake() noexcept
{
    static AnimState* fakeAnimState = nullptr;
    static bool updateFakeAnim = true;
    static bool initFakeAnim = true;
    static float spawnTime = 0.f;

    if (!localPlayer || !localPlayer->isAlive() || !localPlayer->getAnimstate())
        return;

    if (interfaces->engine->isHLTV())
        return;

    if (spawnTime != localPlayer->spawnTime() || updateFakeAnim)
    {
        spawnTime = localPlayer->spawnTime();
        initFakeAnim = false;
        updateFakeAnim = false;
    }

    if (!initFakeAnim)
    {
        fakeAnimState = static_cast<AnimState*>(memory->memalloc->Alloc(sizeof(AnimState)));

        if (fakeAnimState != nullptr)
            localPlayer->createState(fakeAnimState);

        initFakeAnim = true;
    }

    if (!fakeAnimState)
        return;

    if (sendPacket)
    {
        updatingFake = true;

        std::array<AnimationLayer, 13> layers;

        std::memcpy(&layers, localPlayer->animOverlays(), sizeof(AnimationLayer) * localPlayer->getAnimationLayersCount());
        const auto backupAbs = localPlayer->getAbsAngle();
        const auto backupPoses = localPlayer->poseParameters();

        localPlayer->updateState(fakeAnimState, viewangles);
        if (fabsf(fakeAnimState->footYaw - footYaw) <= 5.f)
        {
            gotMatrix = false;
            updatingFake = false;

            memory->setAbsAngle(localPlayer.get(), Vector{ 0, fakeAnimState->footYaw, 0 });
            std::memcpy(localPlayer->animOverlays(), &layers, sizeof(AnimationLayer) * localPlayer->getAnimationLayersCount());
            localPlayer->getAnimationLayer(ANIMATION_LAYER_LEAN)->weight = std::numeric_limits<float>::epsilon();

            gotMatrixFakelag = localPlayer->setupBones(fakelagmatrix.data(), localPlayer->getBoneCache().size, 0x7FF00, memory->globalVars->currenttime);

            std::memcpy(localPlayer->animOverlays(), &layers, sizeof(AnimationLayer) * localPlayer->getAnimationLayersCount());
            localPlayer->poseParameters() = backupPoses;
            memory->setAbsAngle(localPlayer.get(), Vector{ 0,backupAbs.y,0 });
            return;
        }

        memory->setAbsAngle(localPlayer.get(), Vector{ 0, fakeAnimState->footYaw, 0 });
        std::memcpy(localPlayer->animOverlays(), &layers, sizeof(AnimationLayer) * localPlayer->getAnimationLayersCount());
        localPlayer->getAnimationLayer(ANIMATION_LAYER_LEAN)->weight = std::numeric_limits<float>::epsilon();

        gotMatrix = localPlayer->setupBones(fakematrix.data(), localPlayer->getBoneCache().size, 0x7FF00, memory->globalVars->currenttime);
        gotMatrixFakelag = gotMatrix;

        if (gotMatrix)
        {
            std::copy(fakematrix.begin(), fakematrix.end(), fakelagmatrix.data());
            const auto origin = localPlayer->getRenderOrigin();
            for (auto& i : fakematrix)
            {
                i[0][3] -= origin.x;
                i[1][3] -= origin.y;
                i[2][3] -= origin.z;
            }
        }

        std::memcpy(localPlayer->animOverlays(), &layers, sizeof(AnimationLayer) * localPlayer->getAnimationLayersCount());
        localPlayer->poseParameters() = backupPoses;
        memory->setAbsAngle(localPlayer.get(), Vector{ 0,backupAbs.y,0 });

        updatingFake = false;
    }
}

void Animations::renderStart(FrameStage stage) noexcept
{
    if (stage != FrameStage::RENDER_START)
        return;

    if (!localPlayer)
        return;

    if (interfaces->engine->isHLTV())
        return;

    for (int i = 0; i < 13; i++)
    {
        if (i == ANIMATION_LAYER_FLINCH || i == ANIMATION_LAYER_FLASHED || i == ANIMATION_LAYER_WHOLE_BODY || i == ANIMATION_LAYER_WEAPON_ACTION || i == ANIMATION_LAYER_WEAPON_ACTION_RECROUCH)
            continue;

        auto& animLayers = *localPlayer->getAnimationLayer(i);
        if (!&animLayers)
            continue;

        animLayers = layers.at(i);
    }
}

bool BreakingLagCompensation(Entity* entity)
{
    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++)
    {
        auto& records = players.at(i);

        auto prev_org = records.origin;
        auto skip_first = true;

        for (auto& record : players)
        {
            if (skip_first)
            {
                skip_first = false;
                continue;
            }

            auto delta = records.origin - prev_org;
            if (delta.length2DSqr() > 4096.f)
                return true;

            if (records.simulationTime <= entity->simulationTime())
                break;

            prev_org = records.origin;
        }
    }

    return false;
}

float getExtraTicks() noexcept
{
    if (!config->backtrack.fakeLatency || config->backtrack.fakeLatencyAmount <= 0)
        return 0.f;

    return static_cast<float>(config->backtrack.fakeLatencyAmount) / 1000.f;
}

void Animations::handlePlayers(FrameStage stage) noexcept
{
    static auto gravity = interfaces->cvar->findVar(skCrypt("sv_gravity"));
    const float timeLimit = static_cast<float>(config->backtrack.timeLimit) / 1000.f + getExtraTicks();

    if (stage != FrameStage::NET_UPDATE_END)
        return;

    if (!localPlayer)
    {
        for (auto& record : players)
            record.clear();

        return;
    }

    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++)
    {
        auto& records = players.at(i);
        auto& prev_records = players.at(i);
        const auto entity = interfaces->entityList->getEntity(i);

        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive())
        {
            records.clear();
            prev_records.clear();
            continue;
        }

        if (entity->spawnTime() != records.spawnTime)
        {
            records.spawnTime = entity->spawnTime();
            records.reset();
        }

        if (entity->spawnTime() != prev_records.spawnTime)
        {
            prev_records.spawnTime = entity->spawnTime();
            prev_records.reset();
        }

        std::array<AnimationLayer, 13> layers;
        std::memcpy(&layers, entity->animOverlays(), sizeof(AnimationLayer) * entity->getAnimationLayersCount());

        std::memcpy(&records.centerLayer, entity->animOverlays(), sizeof(AnimationLayer) * entity->getAnimationLayersCount());
        std::memcpy(&records.leftLayer, entity->animOverlays(), sizeof(AnimationLayer) * entity->getAnimationLayersCount());
        std::memcpy(&records.rightLayer, entity->animOverlays(), sizeof(AnimationLayer) * entity->getAnimationLayersCount());

        const auto lowerBodyYawTarget = entity->lby();
        const auto duckAmount = entity->duckAmount();
        const auto flags = entity->flags();

        const auto currentTime = memory->globalVars->currenttime;
        const auto frameTime = memory->globalVars->frametime;
        const auto realTime = memory->globalVars->realtime;
        const auto frameCount = memory->globalVars->framecount;
        const auto absoluteFT = memory->globalVars->absoluteFrameTime;
        const auto tickCount = memory->globalVars->tickCount;
        const auto interpolationAmount = memory->globalVars->interpolationAmount;

        memory->globalVars->currenttime = entity->simulationTime();
        memory->globalVars->frametime = memory->globalVars->intervalPerTick;

        const uintptr_t backupEffects = entity->getEffects();
        entity->getEffects() |= 8;

        bool runPostUpdate = false;

        if (records.simulationTime != entity->simulationTime() && records.simulationTime < entity->simulationTime())
        {
            runPostUpdate = true;

            if (records.simulationTime == -1.0f)
                records.simulationTime = entity->simulationTime();

            if (!records.layers.empty())
                std::memcpy(&records.oldlayers, &records.layers, sizeof(AnimationLayer) * entity->getAnimationLayersCount());

            std::memcpy(&records.layers, entity->animOverlays(), sizeof(AnimationLayer) * entity->getAnimationLayersCount());

            // Get chokedPackets
            const auto simDifference = fabsf(entity->simulationTime() - records.simulationTime);

            records.simulationTime != entity->simulationTime() ? records.chokedPackets = static_cast<int>(simDifference / memory->globalVars->intervalPerTick) - 1 : records.chokedPackets = 0;
            records.chokedPackets = std::clamp(records.chokedPackets, 0, maxUserCmdProcessTicks + 1);

            // Velocity values
            if (records.origin.notNull() && records.simulationTime != entity->simulationTime())
            {
                records.oldVelocity = records.velocity;
                records.velocity = (entity->origin() - records.origin) * (1.0f / simDifference);
            }

            // Misc variables
            records.moveWeight = entity->getAnimstate()->moveWeight;
            records.flags = entity->flags();

            if (records.simulationTime == entity->simulationTime())
            {
                records.duckAmount = entity->duckAmount();
                records.oldDuckAmount = records.duckAmount;
                records.origin = entity->origin();
                records.oldOrigin = records.origin;
            }
            else
            {
                records.oldDuckAmount = records.duckAmount;
                records.duckAmount = entity->duckAmount();
                records.oldOrigin = records.origin;
                records.origin = entity->origin();
            }

            // Velocity calculations
            if (entity->flags() & FL_ONGROUND)
                records.velocity.z = 0.f;
            else
            {
                const float weight = 1.0f - records.layers[ANIMATION_LAYER_ALIVELOOP].weight;

                if (weight > 0.0f)
                {
                    const float previousRate = records.oldlayers[ANIMATION_LAYER_ALIVELOOP].playbackRate;
                    const float currentRate = records.layers[ANIMATION_LAYER_ALIVELOOP].playbackRate;

                    if (previousRate == currentRate)
                    {
                        const int previousSequence = records.oldlayers[ANIMATION_LAYER_ALIVELOOP].sequence;
                        const int currentSequence = records.layers[ANIMATION_LAYER_ALIVELOOP].sequence;

                        if (previousSequence == currentSequence)
                        {
                            const float speedNormalized = (weight / 2.8571432f) + 0.55f;

                            if (speedNormalized > 0.0f)
                            {
                                const auto weapon = entity->getActiveWeapon();
                                const float maxSpeed = weapon ? std::fmaxf(weapon->getMaxSpeed(), 0.001f) : CS_PLAYER_SPEED_RUN;
                                const float speed = speedNormalized * maxSpeed;

                                if (speed > 0.0f && records.velocity.length2D() > 0.0f)
                                    records.velocity = (records.velocity / records.velocity.length()) * speed;
                            }
                        }
                    }
                }

                if (!(entity->flags() & FL_ONGROUND))
                    entity->velocity().z -= gravity->getFloat() * ticksToTime(records.chokedPackets) * 0.5f;
                else
                    entity->velocity().z = 0.0f;
            }

            if (entity->flags() & FL_ONGROUND && records.layers[ANIMATION_LAYER_ALIVELOOP].weight > 0.f && records.layers[ANIMATION_LAYER_ALIVELOOP].weight < 1.f && records.layers[ANIMATION_LAYER_ALIVELOOP].cycle > records.oldlayers[ANIMATION_LAYER_ALIVELOOP].cycle)
            {
                float velocityLengthXY = 0.f;
                const auto weapon = entity->getActiveWeapon();
                const float maxSpeedRun = weapon ? std::fmaxf(weapon->getMaxSpeed(), 0.001f) : CS_PLAYER_SPEED_RUN;

                const auto modifier = 0.35f * (1.0f - records.layers[ANIMATION_LAYER_ALIVELOOP].weight);

                if (modifier > 0.f && modifier < 1.0f)
                    velocityLengthXY = maxSpeedRun * (modifier + 0.55f);

                if (velocityLengthXY > 0.f)
                {
                    velocityLengthXY = entity->velocity().length2D() / velocityLengthXY;

                    records.velocity.x *= velocityLengthXY;
                    records.velocity.y *= velocityLengthXY;
                }
            }

            int last_activity = {}, last_activity_tick = {};

            if (records.layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].weight > 0.0f && prev_records.layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].weight <= 0.0f)
            {
                int act = entity->sequenceDuration(records.layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].sequence);

                if (act == ACTIVE_LAND_LIGHT || act == ACTIVE_LAND_HEAVY)
                {
                    float land_time = records.layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].cycle / records.layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].playbackRate;

                    if (land_time > 0.f) 
                    {
                        last_activity_tick = timeToTicks(records.simulationTime - land_time) + 1;
                        last_activity = ACTIVITY_LAND;
                    }
                }
            }

            if (records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].weight > 0.0f && prev_records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].weight <= 0.0f)
            {
                int act = entity->sequenceDuration(records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].sequence);

                if (act == ACTIVITY_JUMP)
                {
                    float jump_time = records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].cycle / records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].playbackRate;

                    if (jump_time > 0.f) 
                    {
                        last_activity_tick = timeToTicks(records.simulationTime - jump_time) + 1;
                        last_activity = ACTIVITY_JUMP;
                    }
                }
            }

            if (records.layers[ANIMATION_LAYER_ADJUST].sequence == 979)
            {
                records.layers[ANIMATION_LAYER_ADJUST].cycle = 0.0;
                records.layers[ANIMATION_LAYER_ADJUST].weight = 0.0;
            }

            if ((records.layers[ANIMATION_LAYER_MOVEMENT_MOVE].playbackRate <= 0.f || records.layers[ANIMATION_LAYER_MOVEMENT_MOVE].weight <= 0.f) && entity->flags() & FL_ONGROUND)
            {
                records.velocity.x = 0.f;
                records.velocity.y = 0.f;
            }

            // Run animations
            Resolver::runPreUpdate(records, prev_records, entity);
            updatingEntity = true;

            for (int i = 1; i < records.chokedPackets + 1; i++)
            {
                if (i != records.chokedPackets)
                {
                    if (last_activity > 0)
                    {
                        bool on_ground = entity->flags() & FL_ONGROUND;

                        if (last_activity == ACTIVITY_JUMP)
                        {
                            if (timeToTicks(records.simulationTime) == (last_activity_tick - 1))
                                on_ground = true;
                            else if (timeToTicks(records.simulationTime) == last_activity_tick)
                            {
                                auto jump_layer = &records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
                                jump_layer->cycle = 0.f;
                                jump_layer->playbackRate = records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].playbackRate;
                                jump_layer->sequence = records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].sequence;
                                on_ground = false;
                            }
                        }
                        else if (last_activity == ACTIVITY_LAND)
                        {
                            if (timeToTicks(records.simulationTime) == (last_activity_tick - 1))
                                on_ground = true;
                            else if (timeToTicks(records.simulationTime) == last_activity_tick)
                            {
                                auto jump_layer = &records.layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
                                jump_layer->cycle = 0.f;
                                jump_layer->playbackRate = records.layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].playbackRate;
                                jump_layer->sequence = records.layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].sequence;
                                on_ground = false;
                            }
                        }

                        if (on_ground)
                            entity->flags() & FL_ONGROUND;
                        else
                            !(entity->flags() & FL_ONGROUND);
                    }
                }
            }

            if (records.chokedPackets <= 0) // We dont need to simulate commands
            {
                if (entity->getAnimstate()->lastUpdateFrame == memory->globalVars->framecount)
                    entity->getAnimstate()->lastUpdateFrame -= 1;

                if (entity->getAnimstate()->lastUpdateTime == memory->globalVars->currenttime)
                    entity->getAnimstate()->lastUpdateTime += ticksToTime(1);

                entity->getEFlags() &= ~0x1000;
                entity->getAbsVelocity() = records.velocity;
                entity->updateClientSideAnimation();
            }
            else
            {
                // Simulate missing ticks
                // TODO: Improve this drastically
                for (int i = 1; i <= records.chokedPackets + 1; i++)
                {
                    const float simulatedTime = records.simulationTime + (memory->globalVars->intervalPerTick * i);
                    const float lerpValue = 1.f - (entity->simulationTime() - simulatedTime) / (entity->simulationTime() - records.simulationTime);
                    const float currentTimeBackup = memory->globalVars->currenttime;

                    memory->globalVars->currenttime = simulatedTime;
                    memory->globalVars->realtime = simulatedTime;
                    memory->globalVars->frametime = memory->globalVars->intervalPerTick;
                    memory->globalVars->absoluteFrameTime = memory->globalVars->intervalPerTick;
                    memory->globalVars->framecount = timeToTicks(simulatedTime);
                    memory->globalVars->tickCount = timeToTicks(simulatedTime);
                    memory->globalVars->interpolationAmount = 0.0f;

                    entity->getEFlags() &= ~0x1000;
                    entity->getAbsVelocity() = Helpers::lerp(lerpValue, records.velocity, records.oldVelocity);
                    entity->duckAmount() = Helpers::lerp(lerpValue, records.duckAmount, records.oldDuckAmount);

                    if (entity->getAnimstate()->lastUpdateFrame == memory->globalVars->framecount)
                        entity->getAnimstate()->lastUpdateFrame -= 1;

                    if (entity->getAnimstate()->lastUpdateTime == memory->globalVars->currenttime)
                        entity->getAnimstate()->lastUpdateTime += ticksToTime(1);

                    entity->updateClientSideAnimation();

                    memory->globalVars->currenttime = currentTime;
                    memory->globalVars->realtime = realTime;
                    memory->globalVars->frametime = frameTime;
                    memory->globalVars->absoluteFrameTime = absoluteFT;
                    memory->globalVars->framecount = frameCount;
                    memory->globalVars->tickCount = tickCount;
                    memory->globalVars->interpolationAmount = interpolationAmount;
                }

                entity->getEFlags() &= ~0x1000;
                entity->getAbsVelocity() = records.velocity;
                entity->duckAmount() = records.duckAmount;
                entity->updateClientSideAnimation();
            }

            updatingEntity = false;
            Resolver::runPostUpdate(records, prev_records, entity);

            auto max_speed = entity->getActiveWeapon() && entity->getWeaponData() ? std::max<float>((entity->isScoped() ? entity->getWeaponData()->maxSpeedAlt : entity->getWeaponData()->maxSpeed), 0.001f) : CS_PLAYER_SPEED_RUN;

            if (entity->is_walking())
                max_speed *= CS_PLAYER_SPEED_WALK_MODIFIER;

            if (entity->duckAmount() >= 1.f)
                max_speed *= CS_PLAYER_SPEED_DUCK_MODIFIER;

            if (entity->flags() & FL_ONGROUND)
            {
                auto& layer_aliveloop = records.oldlayers[ANIMATION_LAYER_ALIVELOOP];

                auto base_value = 1.f - layer_aliveloop.weight;
                auto fake_speed_portion = base_value / 2.85f;

                if (fake_speed_portion > 0.f)
                    fake_speed_portion += 0.55f;

                auto anim_velocity_length = std::min<float>(records.velocity.length(), CS_PLAYER_SPEED_RUN);

                if (fake_speed_portion > 0.f)
                    records.velocity *= ((fake_speed_portion * max_speed) / anim_velocity_length);
            }

            // Fix jump pose on real
            if (!(entity->flags() & FL_ONGROUND))
            {
                const auto iCurrentActivity = entity->getAnimstate()->getLayerActivity(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL);

                if (iCurrentActivity == ACT_CSGO_FALL || iCurrentActivity == ACT_CSGO_JUMP)
                {
                    float flStartedCycleAnimation = {};

                    if (entity->getAnimstate()->getLayerActivity(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL) != iCurrentActivity)
                    {
                        flStartedCycleAnimation = 0.0f;
                        entity->getAnimstate()->durationInAir = 0.0f;
                    }
                    else if (records.oldlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].cycle < records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].cycle)
                    {
                        flStartedCycleAnimation = 0.0f;
                        entity->getAnimstate()->durationInAir = 0.0f;
                    }
                    else
                    {
                        flStartedCycleAnimation = records.oldlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].cycle;
                        entity->getAnimstate()->durationInAir = records.oldlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].cycle;
                    }

                    int nTicksAllowedForServerCycle = entity->getAnimstate()->getTicksFromCycle(records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].playbackRate, records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].cycle, flStartedCycleAnimation);

                    if (entity->moveType() != MoveType::LADDER && records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].playbackRate > 0.0f)
                    {
                        auto flTargetCycle = records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].cycle;

                        if (flTargetCycle != flStartedCycleAnimation)
                        {
                            auto nTotalsTicks = records.chokedPackets + 1;

                            auto flCurrentCycle = flStartedCycleAnimation;
                            auto flTimeDelta = entity->getAnimstate()->lastUpdateIncrement != 0.0f ? entity->getAnimstate()->lastUpdateIncrement : memory->globalVars->intervalPerTick;

                            while (flCurrentCycle < flTargetCycle)
                            {
                                flCurrentCycle += records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].playbackRate * flTimeDelta;
                                ++nTicksAllowedForServerCycle;
                            }
                        }
                    }

                    if (nTicksAllowedForServerCycle == 0 || records.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].cycle >= 0.999f)
                    {
                        auto nTotalCommands = records.chokedPackets;
                        auto nTicksLeft = nTotalCommands - nTicksAllowedForServerCycle;

                        nTicksAllowedForServerCycle += nTicksLeft;
                    }

                    auto flNewDelta = (entity->getAnimstate()->lastUpdateIncrement * nTicksAllowedForServerCycle);
                    entity->getAnimstate()->durationInAir += flNewDelta;

                    auto flDuration = (entity->getAnimstate()->durationInAir - 0.72f) * 1.25f;
                    flDuration = std::clamp(flDuration, 0.0f, 1.0f);

                    auto flNewPose = (3.0f - (flDuration + flDuration)) * (flDuration * flDuration);
                    flNewPose = std::clamp(flNewPose, 0.0f, 1.0f);

                    entity->getAnimstate()->poseParamMappings[PLAYER_POSE_PARAM_JUMP_FALL].setValue(entity, std::clamp(Helpers::smoothStepBounds(0.72f, 1.25f, entity->getAnimstate()->durationInAir), 0.f, 1.f));
                    entity->setPoseParameter(10, flNewPose);
                }
            }
        }

        // Setupbones
        if (runPostUpdate)
        {
            records.simulationTime = entity->simulationTime();
            records.mins = entity->getCollideable()->obbMins();
            records.maxs = entity->getCollideable()->obbMaxs();

            std::memcpy(entity->animOverlays(), &records.centerLayer, sizeof(AnimationLayer)* entity->getAnimationLayersCount()); entity->updateClientSideAnimation();
            std::memcpy(entity->animOverlays(), &records.leftLayer, sizeof(AnimationLayer)* entity->getAnimationLayersCount()); entity->updateClientSideAnimation();
            std::memcpy(entity->animOverlays(), &records.rightLayer, sizeof(AnimationLayer)* entity->getAnimationLayersCount()); entity->updateClientSideAnimation();

            records.gotMatrix = entity->setupBones(records.matrix.data(), entity->getBoneCache().size, 0x7FF00, memory->globalVars->currenttime);
        }

        // Backtrack records
        if (!config->backtrack.enabled || !entity->isOtherEnemy(localPlayer.get()))
        {
            records.backtrackRecords.clear();
            continue;
        }

        // Backtrack
        if (runPostUpdate)
        {
            if (!records.backtrackRecords.empty() && (records.backtrackRecords.front().simulationTime == entity->simulationTime()))
                continue;

            Players::Record record{ };

            record.origin = records.origin;
            record.absAngle = records.absAngle;
            record.simulationTime = records.simulationTime;

            record.mins = records.mins;
            record.maxs = records.maxs;

            std::copy(records.matrix.begin(), records.matrix.end(), record.matrix);

            for (auto bone : { 8, 4, 3, 7, 6, 5 })
            {
                record.positions.push_back(record.matrix[bone].origin());
            }

            records.backtrackRecords.push_front(record);

            while (records.backtrackRecords.size() > 2 && records.backtrackRecords.size() > static_cast<size_t>(timeToTicks(timeLimit)))
                records.backtrackRecords.pop_back();
        }

        std::memcpy(entity->animOverlays(), &layers, sizeof(AnimationLayer) * entity->getAnimationLayersCount());

        entity->getEffects() = backupEffects;
        entity->lby() = lowerBodyYawTarget;
        entity->duckAmount() = duckAmount;
        entity->flags() = flags;

        memory->globalVars->currenttime = currentTime;
        memory->globalVars->realtime = realTime;
        memory->globalVars->frametime = frameTime;
        memory->globalVars->absoluteFrameTime = absoluteFT;
        memory->globalVars->framecount = frameCount;
        memory->globalVars->tickCount = tickCount;
        memory->globalVars->interpolationAmount = interpolationAmount;

        // bre4ak lc
        if (BreakingLagCompensation(entity))
        {
            if (entity->simulationTime() < entity->oldSimulationTime())
                records.break_lc = true;
        }

         
    }
}

void Animations::packetStart() noexcept
{
    if (!localPlayer || !localPlayer->animOverlays())
        return;

    if (interfaces->engine->isHLTV())
        return;

    std::memcpy(&staticLayers, localPlayer->animOverlays(), sizeof(AnimationLayer) * localPlayer->getAnimationLayersCount());

    if (!localPlayer->getAnimstate())
        return;

    primaryCycle = localPlayer->getAnimstate()->primaryCycle;
    moveWeight = localPlayer->getAnimstate()->moveWeight;
}

void verifyLayer(int32_t layer) noexcept
{
    AnimationLayer currentlayer = *localPlayer->getAnimationLayer(layer);
    AnimationLayer previousLayer = staticLayers.at(layer);

    auto& animLayers = *localPlayer->getAnimationLayer(layer);
    if (!&animLayers)
        return;

    if (currentlayer.order != previousLayer.order)
        animLayers.order = previousLayer.order;

    if (currentlayer.sequence != previousLayer.sequence)
        animLayers.sequence = previousLayer.sequence;

    if (currentlayer.prevCycle != previousLayer.prevCycle)
        animLayers.prevCycle = previousLayer.prevCycle;

    if (currentlayer.weight != previousLayer.weight)
        animLayers.weight = previousLayer.weight;

    if (currentlayer.weightDeltaRate != previousLayer.weightDeltaRate)
        animLayers.weightDeltaRate = previousLayer.weightDeltaRate;

    if (currentlayer.playbackRate != previousLayer.playbackRate)
        animLayers.playbackRate = previousLayer.playbackRate;

    if (currentlayer.cycle != previousLayer.cycle)
        animLayers.cycle = previousLayer.cycle;
}

void Animations::postDataUpdate() noexcept
{
    if (!localPlayer || !localPlayer->animOverlays())
        return;

    if (interfaces->engine->isHLTV())
        return;

    for (int i = 0; i < 13; i++)
    {
        if (i == ANIMATION_LAYER_FLINCH || i == ANIMATION_LAYER_FLASHED || i == ANIMATION_LAYER_WHOLE_BODY || i == ANIMATION_LAYER_WEAPON_ACTION || i == ANIMATION_LAYER_WEAPON_ACTION_RECROUCH)
            continue;

        verifyLayer(i);
    }

    if (!localPlayer->getAnimstate())
        return;

    localPlayer->getAnimstate()->primaryCycle = primaryCycle;
    localPlayer->getAnimstate()->moveWeight = moveWeight;
}

void Animations::saveCorrectAngle(int entityIndex, Vector correctAng) noexcept
{
    buildTransformsIndex = entityIndex;
    correctAngle = correctAng;
}

int& Animations::buildTransformationsIndex() noexcept
{
    return buildTransformsIndex;
}

Vector* Animations::getCorrectAngle() noexcept
{
    return &sentViewangles;
}

Vector* Animations::getViewAngles() noexcept
{
    return &viewangles;
}

Vector* Animations::getLocalAngle() noexcept
{
    return &localAngle;
}

bool Animations::isLocalUpdating() noexcept
{
    return updatingLocal;
}

bool Animations::isEntityUpdating() noexcept
{
    return updatingEntity;
}

bool Animations::isFakeUpdating() noexcept
{
    return updatingFake;
}

bool Animations::gotFakeMatrix() noexcept
{
    return gotMatrix;
}

std::array<matrix3x4, MAXSTUDIOBONES> Animations::getFakeMatrix() noexcept
{
    return fakematrix;
}

bool Animations::gotFakelagMatrix() noexcept
{
    return gotMatrixFakelag;
}

std::array<matrix3x4, MAXSTUDIOBONES> Animations::getFakelagMatrix() noexcept
{
    return fakelagmatrix;
}

bool Animations::gotRealMatrix() noexcept
{
    return gotMatrixReal;
}

std::array<matrix3x4, MAXSTUDIOBONES> Animations::getRealMatrix() noexcept
{
    return realmatrix;
}

float Animations::getFootYaw() noexcept
{
    return footYaw;
}

std::array<float, 24> Animations::getPoseParameters() noexcept
{
    return poseParameters;
}

std::array<AnimationLayer, 13> Animations::getAnimLayers() noexcept
{
    return sendPacketLayers;
}

Animations::Players Animations::getPlayer(int index) noexcept
{
    return players.at(index);
}

Animations::Players* Animations::setPlayer(int index) noexcept
{
    return &players.at(index);
}

std::array<Animations::Players, 65> Animations::getPlayers() noexcept
{
    return players;
}

std::array<Animations::Players, 65>* Animations::setPlayers() noexcept
{
    return &players;
}

const std::deque<Animations::Players::Record>* Animations::getBacktrackRecords(int index) noexcept
{
    return &players.at(index).backtrackRecords;
}