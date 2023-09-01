
#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../includes.hpp"
#include "Aimbot.h"
#include "Tickbase.h"
#include "AntiAim.h"
#include "../SDK/ClientState.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/NetworkChannel.h"
#include "../Hacks/EnginePrediction.h"
#include "../SDK/Input.h"
#include "../SDK/Prediction.h"
#include "../Logger.h"

UserCmd* command;
EnginePrediction::NetvarData netvars{ };
static bool doDefensive{ false };

void Tickbase::getCmd(UserCmd* cmd)
{
    command = cmd;
}

void Tickbase::start(UserCmd* cmd) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
    {
        hasHadTickbaseActive = false;
        return;
    }

    if (const auto netChannel = interfaces->engine->getNetworkChannel(); netChannel)
        netChannel->chokedPackets >= chokedPackets;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon)
    {
        hasHadTickbaseActive = false;
        return;
    }

    if (!config->tickbase.doubletap.isActive() && !config->tickbase.hideshots.isActive())
    {
        if (hasHadTickbaseActive)
            shiftTicks(cmd, ticksAllowedForProcessing);

        hasHadTickbaseActive = false;
        return;
    }

    if (config->tickbase.doubletap.isActive())
    {
        targetTickShift = 13;
        // breakLagComp(cmd);
    }
    else if (config->tickbase.hideshots.isActive() && !config->tickbase.doubletap.isActive())
        targetTickShift = ((*memory->gameRules)->isValveDS()) ? 6 : 9;

    targetTickShift = std::clamp(targetTickShift, 0, 13);
    hasHadTickbaseActive = true;
}

void Tickbase::end(UserCmd* cmd) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    auto weapon = localPlayer->getActiveWeapon();
    if (!weapon)
        return;

    if (!config->tickbase.doubletap.isActive() && !config->tickbase.hideshots.isActive())
    {
        targetTickShift = 0;
        return;
    }

    if (cmd->buttons & UserCmd::IN_ATTACK && config->tickbase.doubletap.isActive())
        shiftTicks(cmd, targetTickShift);
    else if (cmd->buttons & UserCmd::IN_ATTACK && config->tickbase.hideshots.isActive())
        shiftTicks(cmd, targetTickShift);
}

bool Tickbase::shiftTicks(UserCmd* cmd, int shiftAmount) noexcept
{
    if (!canShift(shiftAmount))
        return false;

    realTime = memory->globalVars->realtime;
    shiftedTickbase = shiftAmount;
    shiftCommand = cmd->commandNumber;
    tickShift = shiftAmount;

    return true;
}

void Tickbase::breakLagComp(UserCmd* cmd)
{
    static int defensive_tick = memory->clientState->lastServerTickTime;
    static bool can_defensive = false;

    if (abs(memory->clientState->m_clock_drift_mgr.m_server_tick - defensive_tick) > 1)
    {
        defensive_tick = memory->clientState->m_clock_drift_mgr.m_server_tick;

        if (isPeeking)
        {
            if (!can_defensive)
            {
                can_defensive = true;
                sendPacket = true;

                /* set shift amnt. */
                targetTickShift = 13;
            }
        }
        else
        {
            can_defensive = false;
            isPeeking = false;

            /* set shift amnt. */
            targetTickShift = 13;
        }
    }
    else
        targetTickShift = 13;
}

bool Tickbase::canRun() noexcept
{
    static float spawnTime = 0.f;

    if (!interfaces->engine->isInGame() || !interfaces->engine->isConnected())
    {
        ticksAllowedForProcessing = 0;
        chokedPackets = 0;
        pauseTicks = 0;
        return true;
    }

    if (!localPlayer || !localPlayer->isAlive() || !targetTickShift)
    {
        ticksAllowedForProcessing = 0;
        return true;
    }

    if ((*memory->gameRules)->freezePeriod())
    {
        realTime = memory->globalVars->realtime;
        return true;
    }

    if (spawnTime != localPlayer->spawnTime())
    {
        spawnTime = localPlayer->spawnTime();
        ticksAllowedForProcessing = 0;
        pauseTicks = 0;
    }

    if (config->misc.fakeduck && config->misc.fakeduckKey.isActive())
    {
        realTime = memory->globalVars->realtime;
        shiftedTickbase = 0;
        shiftCommand = 0;
        tickShift = 0;
        return true;
    }

    if ((ticksAllowedForProcessing < targetTickShift || chokedPackets > 13 - targetTickShift) && memory->globalVars->realtime - realTime > 0.5f)
    {
        ticksAllowedForProcessing = min(ticksAllowedForProcessing++, 13);
        chokedPackets = max(chokedPackets--, 0);
        pauseTicks++;
        isRecharging = true;
        return false;
    }

    return true;
}

bool Tickbase::canShift(int shiftAmount) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return false;

    if (!shiftAmount || shiftAmount > ticksAllowedForProcessing || memory->globalVars->realtime - realTime <= 0.5f)
        return false;

    if (config->misc.fakeduck && config->misc.fakeduckKey.isActive())
        return false;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return false;

    if (activeWeapon->isGrenade() || activeWeapon->isBomb()
        || activeWeapon->itemDefinitionIndex2() == WeaponId::Healthshot)
        return false;

    const float shiftTime = (localPlayer->tickBase() - shiftAmount) * memory->globalVars->intervalPerTick;
    if (localPlayer->nextAttack() > shiftTime)
        return false;

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
        return false;

    return activeWeapon->nextPrimaryAttack() <= shiftTime;
}

int calc_correction_ticks() 
{
    //return valve::g_global_vars->m_max_clients <= 1
    //	? -1 : valve::to_ticks(std::clamp(g_context->cvars().m_sv_clockcorrection_msecs->get_float() / 1000.f, 0.f, 1.f));

    float correction_ms; // st7
    float v4; // xmm0_4
    float v6; // [esp+0h] [ebp-10h]

    auto clock = interfaces->cvar->findVar(skCrypt("sv_clockcorrection_msecs"));

    if (!clock || memory->globalVars->maxClients <= 1)
        return -1;

    correction_ms = clock->getFloat();
    v4 = 1.0f;
    v6 = correction_ms / 1000.0f;

    if (v6 <= 1.0f)
    {
        v4 = 0.0f;
        if (v6 >= 0.0f)
            v4 = correction_ms / 1000.0f;
    }

    return ((v4 / memory->globalVars->intervalPerTick) + 0.5f);
}

int Tickbase::adjust_tick_base(const int old_new_cmds, const int total_new_cmds, const int delta) noexcept
{
    // russian autists can't paste from src leak.
    // https://github.com/perilouswithadollarsign/cstrike15_src/blob/f82112a2388b841d72cb62ca48ab1846dfcc11c8/game/server/player.cpp#L3475

    // fixed antipaste
    auto ret = 0;

    //const auto flCorrectionSeconds = std::clamp(g_context->cvars().m_sv_clockcorrection_msecs->get_float() / 1000.0f, 0.0f, 1.0f);
    const auto correction_ticks = calc_correction_ticks();

    if (correction_ticks != -1) 
    {
        // const auto& prev_local_data = g_eng_pred->local_data().at(valve::g_client_state->m_last_cmd_out % 150);

        if (netvars.spawn_time == localPlayer->spawnTime())
        {
            ret = netvars.tickbase + 1;

            const auto tick_count = ret + old_new_cmds - targetTickShift;
            const auto ideal_final_tick = tick_count + correction_ticks;

            const auto too_fast_limit = ideal_final_tick + correction_ticks;
            const auto too_slow_limit = ideal_final_tick - correction_ticks;

            const auto adjusted_final_tick = ret + total_new_cmds;

            if (adjusted_final_tick > too_fast_limit || adjusted_final_tick < too_slow_limit)
                ret = ideal_final_tick - total_new_cmds;
        }
    }

    if (ret != 0)
        return ret;

    return (netvars.spawn_time != localPlayer->spawnTime() ? netvars.tickbase : localPlayer->tickBase()) - (delta + 3); // fixed antipaste (was adding 3 cause meme russian code)
}

int Tickbase::getCorrectTickbase(int commandNumber) noexcept
{
    const int tickBase = localPlayer->tickBase();

    if (isRecharging)
        return tickBase + ticksAllowedForProcessing;

    /* i dont really understand why but whatever */
    memory->globalVars->currenttime = timeToTicks(tickBase);

    if (commandNumber == shiftCommand)
        return tickBase - shiftedTickbase + memory->globalVars->m_simticksthisframe + 1;
    else if (commandNumber == shiftCommand + 1)
    {
        if (!config->tickbase.teleport)
            return tickBase + shiftedTickbase - memory->globalVars->m_simticksthisframe + 1;

        return tickBase;
    }

    if (pauseTicks)
        return tickBase + pauseTicks;

    return tickBase;
}

//If you have dt enabled, you need to shift 13 ticks, so it will return 13 ticks
//If you have hs enabled, you need to shift 9 ticks, so it will return 7 ticks
int Tickbase::getTargetTickShift() noexcept
{
    return targetTickShift;
}

int Tickbase::getTickshift() noexcept
{
    isRecharging = false;
    return tickShift;
}

void Tickbase::resetTickshift() noexcept
{
    shiftedTickbase = tickShift;
    if (config->tickbase.teleport && config->tickbase.doubletap.isActive())
        ticksAllowedForProcessing = max(ticksAllowedForProcessing - tickShift, 0);

    tickShift = 0;
}

int& Tickbase::pausedTicks() noexcept
{
    return pauseTicks;
}

bool& Tickbase::isFinalTick() noexcept
{
    return finalTick;
}

bool& Tickbase::isShifting() noexcept
{
    return shifting;
}

void Tickbase::updateInput() noexcept
{
    config->tickbase.doubletap.handleToggle();
    config->tickbase.hideshots.handleToggle();
}

void Tickbase::reset() noexcept
{
    isRecharging = false;
    shifting = false;
    finalTick = false;
    hasHadTickbaseActive = false;
    sendPacket = false;

    pauseTicks = 0;
    chokedPackets = 0;
    tickShift = 0;
    shiftCommand = 0;
    shiftedTickbase = 0;
    ticksAllowedForProcessing = 0;
    nextShiftAmount = 0;
    realTime = 0.0f;
}