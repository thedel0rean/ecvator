#include "EnginePrediction.h"
#include "Fakelag.h"
#include "Tickbase.h"

#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/Localplayer.h"
#include "../SDK/Vector.h"
#include "AntiAim.h"

void Fakelag::run(bool& sendPacket) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (!config->condAA.global)
        return;

    auto chokedPackets = 0;

    const auto netChannel = interfaces->engine->getNetworkChannel();
    if (!netChannel)
        return;

    if (AntiAim::getDidShoot())
    {
        sendPacket = true;
        return;
    }

    if ((*memory->gameRules)->isValveDS()) //-V807
        chokedPackets = interfaces->engine->isVoiceRecording() ? 1 : min(chokedPackets, 6);

    if (config->tickbase.doubletap.isActive() || config->tickbase.hideshots.isActive() && memory->globalVars->realtime - Tickbase::realTime > 0.5f && localPlayer && localPlayer->isAlive() && localPlayer->getActiveWeapon()
        && localPlayer->getActiveWeapon()->nextPrimaryAttack() <= (localPlayer->tickBase() - Tickbase::getTargetTickShift()) * memory->globalVars->intervalPerTick && (config->misc.fakeduck && !config->misc.fakeduckKey.isActive()))
    {
        chokedPackets = 1;
        sendPacket = false;
    }
    else
    {
        chokedPackets = config->fakelag.limit;
        sendPacket = false;
    }

    chokedPackets = std::clamp(chokedPackets, 0, maxUserCmdProcessTicks - Tickbase::getTargetTickShift());
    sendPacket = netChannel->chokedPackets >= chokedPackets;
}