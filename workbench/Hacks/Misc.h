#pragma once

#include "../SDK/ClientMode.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/Vector.h"

enum class FrameStage;
class GameEvent;
struct ImDrawList;
struct ViewSetup;

class CTeslaInfo
{
public:
    Vector m_vPos;
    Vector m_vAngles;
    int m_nEntIndex;
    const char* m_pszSpriteName;
    float m_flBeamWidth;
    int m_nBeams;
    Vector m_vColor;
    float m_flTimeVisible;
    float m_flRadius;
    float m_flRed;
    float m_flGreen;
    float m_flBlue;
    float m_flBrightness;
};

namespace Misc
{
    bool hcovsex{ false };
    static Vector peekPosition{};
    std::string spotifytitle = "";
    void getCmd(UserCmd* cmd) noexcept;
    bool isInChat() noexcept;
    void gatherDataOnTick(UserCmd* cmd) noexcept;
    void handleKeyEvent(int keynum, const char* currentBinding) noexcept;
    void drawKeyDisplay(ImDrawList* drawList) noexcept;
    void drawVelocity(ImDrawList* drawList) noexcept;
    void chatRevealer(GameEvent& event, GameEvent* events) noexcept;
    void drawPlayerList() noexcept;
    void viewModelChanger(ViewSetup* setup) noexcept;
    void drawAutoPeek() noexcept;
    void autoPeek(UserCmd* cmd, Vector currentViewAngles) noexcept;
    void unlockHiddenCvars() noexcept;
    void fakeDuck(UserCmd* cmd, bool& sendPacket) noexcept;
    void slowwalk(UserCmd* cmd) noexcept;
    void inverseRagdollGravity() noexcept;
    void aaArrows(ImDrawList* drawList) noexcept;
    void Indictators() noexcept;
    void customScope() noexcept;
    void updateClanTag(bool = false) noexcept;
    void showKeybinds() noexcept;
    void spectatorList() noexcept;
    void noscopeCrosshair() noexcept;
    void recoilCrosshair() noexcept;
    void spotifyInd() noexcept;
    void watermark() noexcept;
    void prepareRevolver(UserCmd*) noexcept;
    void fastStop(UserCmd*) noexcept;
    void drawBombTimer() noexcept;
    void hurtIndicator() noexcept;
    void stealNames() noexcept;
    void disablePanoramablur() noexcept;
    bool changeName(bool, const char*, float) noexcept;
    void bunnyHop(UserCmd*) noexcept;
    void fixTabletSignal() noexcept;
    void killfeedChanger(GameEvent& event) noexcept;
    void fixMovement(UserCmd* cmd, float yaw) noexcept;
    void antiAfkKick(UserCmd* cmd) noexcept;
    void fixAnimationLOD(FrameStage stage) noexcept;
    void autoPistol(UserCmd* cmd) noexcept;
    void autoReload(UserCmd* cmd) noexcept;
    void revealRanks(UserCmd* cmd) noexcept;
    void autoStrafe(UserCmd* cmd, Vector& currentViewAngles) noexcept;
    void removeCrouchCooldown(UserCmd* cmd) noexcept;
    void moonwalk(UserCmd* cmd, bool& sendPacket) noexcept;
    void playHitSound(GameEvent& event) noexcept;
    void killSound(GameEvent& event) noexcept;
    void autoBuy(GameEvent* event) noexcept;
    void runReportbot() noexcept;
    void resetReportbot() noexcept;
    void preserveKillfeed(bool roundStart = false) noexcept;
    void drawOffscreenEnemies(ImDrawList* drawList) noexcept;
    void autoAccept(const char* soundEntry) noexcept;
    void updateInput() noexcept;
    void reset(int resetType) noexcept;
}
