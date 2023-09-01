#include <mutex>
#include <numeric>
#include <sstream>
#include <string>

#include "../imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../imgui/imgui_internal.h"
#include "../postprocessing.h"
#include "../xor.h"
#include "DLight.h"
#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../Netvars.h"
#include "../GUI.h"
#include "../Helpers.h"
#include "../GameData.h"
#include "Tickbase.h"
#include "AntiAim.h"
#include "../render.hpp"
#include "../Hooks.h"
#include "EnginePrediction.h"
#include "Misc.h"
#include "../SDK/ViewRenderBeams.h"
#include "../IEffects.h"
#include "../SDK/Client.h"
#include "../SDK/ClientMode.h"
#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/FrameStage.h"
#include "../SDK/GameEvent.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/Input.h"
#include "../SDK/ItemSchema.h"
#include "../Localize.h"
#include "../SDK/MaterialSystem.h"
#include "../SDK/Material.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/Panorama.h"
#include "../SDK/Prediction.h"
#include "../SDK/Surface.h"
#include "../SDK/UserCmd.h"
#include "../SDK/ViewSetup.h"
#include "../SDK/WeaponData.h"
#include "../SDK/WeaponSystem.h"
#include "../SDK/Steam.h"
#include "../imguiCustom.h"
#include <TlHelp32.h>
#include "../includes.hpp"
#include "../postprocessing.h"
#include "Visuals.h"

UserCmd* cmd1;
int goofy;

void Misc::getCmd(UserCmd* cmd) noexcept
{
    cmd1 = cmd;
    if (localPlayer && localPlayer->isAlive())
    goofy = get_moving_flag(cmd);
}

bool Misc::isInChat() noexcept
{
    if (!localPlayer)
        return false;

    const auto hudChat = memory->findHudElement(memory->hud, skCrypt("CCSGO_HudChat"));
    if (!hudChat)
        return false;

    const bool isInChat = *(bool*)((uintptr_t)hudChat + 0xD);

    return isInChat;
}

std::string currentForwardKey = "";
std::string currentBackKey = "";
std::string currentRightKey = "";
std::string currentLeftKey = "";
int currentButtons = 0;

void Misc::gatherDataOnTick(UserCmd* cmd) noexcept
{
    currentButtons = cmd->buttons;
}

void Misc::handleKeyEvent(int keynum, const char* currentBinding) noexcept
{
    if (!currentBinding || keynum <= 0 || keynum >= ARRAYSIZE(ButtonCodes))
        return;

    const auto buttonName = ButtonCodes[keynum];

    switch (fnv::hash(currentBinding))
    {
    case fnv::hash("+forward"):
        currentForwardKey = std::string(buttonName);
        break;
    case fnv::hash("+back"):
        currentBackKey = std::string(buttonName);
        break;
    case fnv::hash("+moveright"):
        currentRightKey = std::string(buttonName);
        break;
    case fnv::hash("+moveleft"):
        currentLeftKey = std::string(buttonName);
        break;
    default:
        break;
    }
}

void Misc::drawKeyDisplay(ImDrawList* drawList) noexcept
{
    if (!config->misc.keyBoardDisplay.enabled)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    int screenSizeX, screenSizeY;
    interfaces->engine->getScreenSize(screenSizeX, screenSizeY);
    const float Ypos = static_cast<float>(screenSizeY) * config->misc.keyBoardDisplay.position;

    std::string keys[3][2];
    if (config->misc.keyBoardDisplay.showKeyTiles)
    {
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                keys[i][j] = "_";
            }
        }
    }

    if (currentButtons & UserCmd::IN_DUCK)
        keys[0][0] = skCrypt("C");
    if (currentButtons & UserCmd::IN_FORWARD)
        keys[1][0] = currentForwardKey;
    if (currentButtons & UserCmd::IN_JUMP)
        keys[2][0] = skCrypt("J");
    if (currentButtons & UserCmd::IN_MOVELEFT)
        keys[0][1] = currentLeftKey;
    if (currentButtons & UserCmd::IN_BACK)
        keys[1][1] = currentBackKey;
    if (currentButtons & UserCmd::IN_MOVERIGHT)
        keys[2][1] = currentRightKey;

    const float positions[3] =
    {
       -35.0f, 0.0f, 35.0f
    };

    ImGui::PushFont(gui->getTahoma28Font());
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (keys[i][j] == "")
                continue;

            const auto size = ImGui::CalcTextSize(keys[i][j].c_str());
            drawList->AddText(ImVec2{ (static_cast<float>(screenSizeX) / 2 - size.x / 2 + positions[i]) + 1, (Ypos + (j * 25)) + 1 }, Helpers::calculateColor(Color4{ 0.0f, 0.0f, 0.0f, config->misc.keyBoardDisplay.color.color[3] }), keys[i][j].c_str());
            drawList->AddText(ImVec2{ static_cast<float>(screenSizeX) / 2 - size.x / 2 + positions[i], Ypos + (j * 25) }, Helpers::calculateColor(config->misc.keyBoardDisplay.color), keys[i][j].c_str());
        }
    }

    ImGui::PopFont();
}

void Misc::drawVelocity(ImDrawList* drawList) noexcept
{
    if (!config->misc.velocity.enabled)
        return;

    if (!localPlayer)
        return;

    const auto entity = localPlayer->isAlive() ? localPlayer.get() : localPlayer->getObserverTarget();
    if (!entity)
        return;

    int screenSizeX, screenSizeY;
    interfaces->engine->getScreenSize(screenSizeX, screenSizeY);
    const float Ypos = static_cast<float>(screenSizeY) * config->misc.velocity.position;

    static float colorTime = 0.f;
    static float takeOffTime = 0.f;

    static auto lastVelocity = 0;
    const auto velocity = static_cast<int>(round(entity->velocity().length2D()));

    static auto takeOffVelocity = 0;
    static bool lastOnGround = true;
    const bool onGround = entity->flags() & FL_ONGROUND;
    if (lastOnGround && !onGround)
    {
        takeOffVelocity = velocity;
        takeOffTime = memory->globalVars->realtime + 2.f;
    }

    const bool shouldDrawTakeOff = !onGround || (takeOffTime > memory->globalVars->realtime);
    const std::string finalText = std::to_string(velocity);

    const Color4 trueColor = config->misc.velocity.color.enabled ? Color4{ config->misc.velocity.color.color[0], config->misc.velocity.color.color[1], config->misc.velocity.color.color[2], config->misc.velocity.alpha, config->misc.velocity.color.rainbowSpeed, config->misc.velocity.color.rainbow }
        : (velocity == lastVelocity ? Color4{ 1.0f, 0.78f, 0.34f, config->misc.velocity.alpha } : velocity < lastVelocity ? Color4{ 1.0f, 0.46f, 0.46f, config->misc.velocity.alpha } : Color4{ 0.11f, 1.0f, 0.42f, config->misc.velocity.alpha });

    ImGui::PushFont(gui->getTahoma28Font());

    const auto size = ImGui::CalcTextSize(finalText.c_str());
    drawList->AddText(ImVec2{ (static_cast<float>(screenSizeX) / 2 - size.x / 2) + 1, Ypos + 1.0f }, Helpers::calculateColor(Color4{ 0.0f, 0.0f, 0.0f, config->misc.velocity.alpha }), finalText.c_str());
    drawList->AddText(ImVec2{ static_cast<float>(screenSizeX) / 2 - size.x / 2, Ypos }, Helpers::calculateColor(trueColor), finalText.c_str());

    if (shouldDrawTakeOff)
    {
        const std::string bottomText = "(" + std::to_string(takeOffVelocity) + ")";
        const Color4 bottomTrueColor = config->misc.velocity.color.enabled ? Color4{ config->misc.velocity.color.color[0], config->misc.velocity.color.color[1], config->misc.velocity.color.color[2], config->misc.velocity.alpha, config->misc.velocity.color.rainbowSpeed, config->misc.velocity.color.rainbow }
            : (takeOffVelocity <= 250.0f ? Color4{ 0.75f, 0.75f, 0.75f, config->misc.velocity.alpha } : Color4{ 0.11f, 1.0f, 0.42f, config->misc.velocity.alpha });
        const auto bottomSize = ImGui::CalcTextSize(bottomText.c_str());
        drawList->AddText(ImVec2{ (static_cast<float>(screenSizeX) / 2 - bottomSize.x / 2) + 1, Ypos + 20.0f + 1 }, Helpers::calculateColor(Color4{ 0.0f, 0.0f, 0.0f, config->misc.velocity.alpha }), bottomText.c_str());
        drawList->AddText(ImVec2{ static_cast<float>(screenSizeX) / 2 - bottomSize.x / 2, Ypos + 20.0f }, Helpers::calculateColor(bottomTrueColor), bottomText.c_str());
    }

    ImGui::PopFont();

    if (colorTime <= memory->globalVars->realtime)
    {
        colorTime = memory->globalVars->realtime + 0.1f;
        lastVelocity = velocity;
    }

    lastOnGround = onGround;
}

void textEllipsisInTableCell(const char* text) noexcept
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Need to call textEllipsisInTableCell() after BeginTable()!");
    IM_ASSERT(table->CurrentColumn != -1);

    const char* textEnd = ImGui::FindRenderedTextEnd(text);
    ImVec2 textSize = ImGui::CalcTextSize(text, textEnd, true);
    ImVec2 textPos = window->DC.CursorPos;
    float textHeight = ImMax(textSize.y, table->RowMinHeight - table->CellPaddingY * 2.0f);

    float ellipsisMax = ImGui::TableGetCellBgRect(table, table->CurrentColumn).Max.x;
    ImGui::RenderTextEllipsis(window->DrawList, textPos, ImVec2(ellipsisMax, textPos.y + textHeight + g.Style.FramePadding.y), ellipsisMax, ellipsisMax, text, textEnd, &textSize);

    ImRect bb(textPos, textPos + textSize);
    ImGui::ItemSize(textSize, 0.0f);
    ImGui::ItemAdd(bb, 0);
}


void Misc::chatRevealer(GameEvent& event, GameEvent* events) noexcept
{
    if (!config->misc.chatReveavler)
        return;

    std::string output = "";

    const auto entity = interfaces->entityList->getEntity(interfaces->engine->getPlayerForUserID(events->getInt(skCrypt("userid"))));
    if (!entity)
        return;

    const auto team = entity->team();
    const char* text = event.getString(skCrypt("text"));
    const char* last_location = entity->lastPlaceName();
    std::string penis = "";
    const std::string name = entity->getPlayerName();
    const bool ALIVE = entity->isAlive();
    if (team == localPlayer->team())
        return;
    if (!ALIVE)
    {
        output += team == Team::CT ? "\x0B" : "\x09" + std::string("*DEAD*");
        penis = " : ";
    }
    else
    {
        penis = " @ \x04" + std::string(last_location) + " : \x01";
    }
    switch (team)
    {
    case Team::TT:
        output += "\x09(Terrorist) ";
        break;
    case Team::CT:
        output += "\x0B(Counter-Terrorist) ";
        break;
    }
    output = output + name + penis + text;
    memory->clientMode->getHudChat()->printf(0, output.c_str());

}

void Misc::drawPlayerList() noexcept
{
    if (!config->misc.playerList.enabled)
        return;

    if (config->misc.playerList.pos != ImVec2{}) {
        ImGui::SetNextWindowPos(config->misc.playerList.pos);
        config->misc.playerList.pos = {};
    }

    static bool changedName = true;
    static std::string nameToChange = "";

    if (!changedName && nameToChange != "")
        changedName = changeName(false, (nameToChange + '\x1').c_str(), 1.0f);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (!gui->isOpen())
    {
        windowFlags |= ImGuiWindowFlags_NoInputs;
        return;
    }

    GameData::Lock lock;
    if ((GameData::players().empty()) && !gui->isOpen())
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.5f);
    ImGui::PushStyleColor(ImGuiCol_Border, { config->menu.accentColor.color[0], config->menu.accentColor.color[1], config->menu.accentColor.color[2], config->menu.accentColor.color[3] });
    ImGui::SetNextWindowSize(ImVec2(300.0f, 300.0f), ImGuiCond_Once);

    if (ImGui::Begin("Player List", nullptr, windowFlags))
    {
        PostProcessing::performFullscreenBlur(ImGui::GetWindowDrawList(), 1.f);
        if (ImGui::beginTable("", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) 
        {
            ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 120.0f);
            ImGui::TableSetupColumn("Steam ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Wins", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Health", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Armor", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Money", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetColumnEnabled(2, config->misc.playerList.steamID);
            ImGui::TableSetColumnEnabled(3, config->misc.playerList.rank);
            ImGui::TableSetColumnEnabled(4, config->misc.playerList.wins);
            ImGui::TableSetColumnEnabled(5, config->misc.playerList.health);
            ImGui::TableSetColumnEnabled(6, config->misc.playerList.armor);
            ImGui::TableSetColumnEnabled(7, config->misc.playerList.money);

            ImGui::TableHeadersRow();

            std::vector<std::reference_wrapper<const PlayerData>> playersOrdered{ GameData::players().begin(), GameData::players().end() };
            std::ranges::sort(playersOrdered, [](const PlayerData& a, const PlayerData& b) {
                // enemies first
                if (a.enemy != b.enemy)
                    return a.enemy && !b.enemy;

                return a.handle < b.handle;
                });

            ImGui::PushFont(gui->getUnicodeFont());

            for (const PlayerData& player : playersOrdered) 
            {
                ImGui::TableNextRow();
                ImGui::PushID(ImGui::TableGetRowIndex());

                if (ImGui::TableNextColumn())
                    ImGui::Text("%d", player.userId);

                if (ImGui::TableNextColumn())
                {
                    ImGui::Image(player.getAvatarTexture(), { ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight() });
                    ImGui::SameLine();
                    textEllipsisInTableCell(player.name.c_str());
                }

                if (ImGui::TableNextColumn() && ImGui::smallButtonFullWidth("Copy", player.steamID == 0))
                    ImGui::SetClipboardText(std::to_string(player.steamID).c_str());

                if (ImGui::TableNextColumn()) 
                {
                    ImGui::Image(player.getRankTexture(), { 2.45f /* -> proportion 49x20px */ * ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight() });
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::PushFont(nullptr);
                        ImGui::TextUnformatted(player.getRankName().data());
                        ImGui::PopFont();
                        ImGui::EndTooltip();
                    }            
                }

                if (ImGui::TableNextColumn())
                    ImGui::Text("%d", player.competitiveWins);

                if (ImGui::TableNextColumn())
                {
                    if (!player.alive)
                        ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "%s", "Dead");
                    else
                        ImGui::Text("%d HP", player.health);
                }

                if (ImGui::TableNextColumn())
                    ImGui::Text("%d", player.armor);

                if (ImGui::TableNextColumn())
                    ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "$%d", player.money);

                if (ImGui::TableNextColumn())
                {
                    if (ImGui::smallButtonFullWidth("...", false))
                        ImGui::OpenPopup("");

                    if (ImGui::BeginPopup(""))
                    {
                        if (ImGui::Button("Steal name"))
                        {
                            changedName = changeName(false, (std::string{ player.name } + '\x1').c_str(), 1.0f);
                            nameToChange = player.name;

                            if (PlayerInfo playerInfo; interfaces->engine->isInGame() && localPlayer
                                && interfaces->engine->getPlayerInfo(localPlayer->index(), playerInfo) && (playerInfo.name == std::string{ "?empty" } || playerInfo.name == std::string{ "\n\xAD\xAD\xAD" }))
                                changedName = false;
                        }

                        if (ImGui::Button("Steal clantag"))
                            memory->setClanTag(player.clanTag.c_str(), player.clanTag.c_str());

                        if (GameData::local().exists && player.team == GameData::local().team && player.steamID != 0)
                        {
                            if (ImGui::Button("Kick"))
                            {
                                const std::string cmd = "callvote kick " + std::to_string(player.userId);
                                interfaces->engine->clientCmdUnrestricted(cmd.c_str());
                            }
                        }

                        ImGui::EndPopup();
                    }
                }
                ImGui::PopID();
            }

            ImGui::PopFont();
            ImGui::EndTable();
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::End();
}

static void shadeVertsHSVColorGradientKeepAlpha(ImDrawList* draw_list, int vert_start_idx, int vert_end_idx, ImVec2 gradient_p0, ImVec2 gradient_p1, ImU32 col0, ImU32 col1)
{
    ImVec2 gradient_extent = gradient_p1 - gradient_p0;
    float gradient_inv_length2 = 1.0f / ImLengthSqr(gradient_extent);
    ImDrawVert* vert_start = draw_list->VtxBuffer.Data + vert_start_idx;
    ImDrawVert* vert_end = draw_list->VtxBuffer.Data + vert_end_idx;

    ImVec4 col0HSV = ImGui::ColorConvertU32ToFloat4(col0);
    ImVec4 col1HSV = ImGui::ColorConvertU32ToFloat4(col1);
    ImGui::ColorConvertRGBtoHSV(col0HSV.x, col0HSV.y, col0HSV.z, col0HSV.x, col0HSV.y, col0HSV.z);
    ImGui::ColorConvertRGBtoHSV(col1HSV.x, col1HSV.y, col1HSV.z, col1HSV.x, col1HSV.y, col1HSV.z);
    ImVec4 colDelta = col1HSV - col0HSV;

    for (ImDrawVert* vert = vert_start; vert < vert_end; vert++)
    {
        float d = ImDot(vert->pos - gradient_p0, gradient_extent);
        float t = ImClamp(d * gradient_inv_length2, 0.0f, 1.0f);

        float h = col0HSV.x + colDelta.x * t;
        float s = col0HSV.y + colDelta.y * t;
        float v = col0HSV.z + colDelta.z * t;

        ImVec4 rgb;
        ImGui::ColorConvertHSVtoRGB(h, s, v, rgb.x, rgb.y, rgb.z);
        vert->col = (ImGui::ColorConvertFloat4ToU32(rgb) & ~IM_COL32_A_MASK) | (vert->col & IM_COL32_A_MASK);
    }
}

void Misc::viewModelChanger(ViewSetup* setup) noexcept
{
    if (!localPlayer)
        return;

    auto activeWeapon = localPlayer->getActiveWeapon();

    if (!activeWeapon)
        return;

    if ((activeWeapon->itemDefinitionIndex2() == WeaponId::Aug || activeWeapon->itemDefinitionIndex2() == WeaponId::Sg553) && localPlayer->isScoped())
        return;

    constexpr auto setViewmodel = [](Entity* viewModel, const Vector& angles) constexpr noexcept
    {
        if (viewModel)
        {
            Vector forward = Vector::fromAngle(angles);
            Vector up = Vector::fromAngle(angles - Vector{ 90.0f, 0.0f, 0.0f });
            Vector side = forward.cross(up);
            Vector offset = side * config->visuals.viewModel.x + forward * config->visuals.viewModel.y + up * config->visuals.viewModel.z;
            memory->setAbsOrigin(viewModel, viewModel->getRenderOrigin() + offset);
            memory->setAbsAngle(viewModel, angles + Vector{ 0.0f, 0.0f, config->visuals.viewModel.roll });
        }
    };

    if (localPlayer->isAlive())
    {
        if (config->visuals.viewModel.enabled && !memory->input->isCameraInThirdPerson)
            setViewmodel(interfaces->entityList->getEntityFromHandle(localPlayer->viewModel()), setup->angles);
    }
    else if (auto observed = localPlayer->getObserverTarget(); observed && localPlayer->getObserverMode() == ObsMode::InEye)
    {
        if (config->visuals.viewModel.enabled && !observed->isScoped())
            setViewmodel(interfaces->entityList->getEntityFromHandle(observed->viewModel()), setup->angles);
    }
}

static Vector peekPosition{};
static bool hasShot = false;

void RadialGradient3D(Vector pos, float radius, Color in, Color out, bool one) 
{
    ImVec2 center; ImVec2 g_pos;

    // Use arc with automatic segment count
    static float m_flAnim = 0.f;
    auto drawList = ImGui::GetBackgroundDrawList();
    m_flAnim += ImGui::GetIO().DeltaTime; if (m_flAnim > 1.f) m_flAnim = 0.f;
    Helpers::worldToScreen(Vector(pos), g_pos);
    center = ImVec2(g_pos.x, g_pos.y);
    drawList->_PathArcToFastEx(center, radius, 0, 48, 0);
    const int count = drawList->_Path.Size - 1;
    float step = (3.141592654f * 2.0f) / (count + 1);
    std::vector<ImVec2> point;

    for (float lat = 0.f; lat <= 3.141592654f * 2.0f; lat += step)
    {
        const auto& point3d = Vector(sin(lat), cos(lat), 0.f) * radius;
        ImVec2 point2d;
        if (Helpers::worldToScreen(Vector(pos) + point3d, point2d))
            point.push_back(ImVec2(point2d.x, point2d.y));
    }

    if (in.a() == 0 && out.a() == 0 || radius < 0.5f || point.size() < count + 1)
        return;

    unsigned int vtx_base = drawList->_VtxCurrentIdx;
    drawList->PrimReserve(count * 3, count + 1);

    // Submit vertices
    const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
    drawList->PrimWriteVtx(center, uv, ImColor(in.r(), in.g(), in.b(), in.a()));

    for (int n = 0; n < count; n++)
        drawList->PrimWriteVtx(point[n + 1], uv, ImColor(out.r(), out.g(), out.b(), out.a()));

    // Submit a fan of triangles
    for (int n = 0; n < count; n++)
    {
        drawList->PrimWriteIdx((ImDrawIdx)(vtx_base));
        drawList->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + n));
        drawList->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + ((n + 1) % count)));
    }

    drawList->_Path.Size = 0;
}

void Misc::drawAutoPeek() noexcept
{
    if (!config->misc.autoPeek.enabled)
        return;

    if (!config->misc.autoPeekKey.isActive())
        return;

    if (!peekPosition.notNull())
        return;

    Vector screen;
    ImVec2 quic;

    Vector local_origin = localPlayer->getAbsOrigin();
    ImVec2 localorign;

    RadialGradient3D(peekPosition, 23.5f, Color(config->misc.autoPeek.color[0], config->misc.autoPeek.color[1], config->misc.autoPeek.color[2]), Color(config->misc.autoPeek.color[0], config->misc.autoPeek.color[1], config->misc.autoPeek.color[2], 0.0f), false);

    if (Helpers::worldToScreen(local_origin, localorign) && Helpers::worldToScreen(peekPosition, quic))
        RadialGradient3D(peekPosition, 23.5f, Color(config->misc.autoPeek.color[0], config->misc.autoPeek.color[1], config->misc.autoPeek.color[2], 200.0f), Color(config->misc.autoPeek.color[0], config->misc.autoPeek.color[1], config->misc.autoPeek.color[2], 0.0f), false);
 }

void Misc::autoPeek(UserCmd* cmd, Vector currentViewAngles) noexcept
{
    if (!localPlayer)
        return;

    if (!config->misc.autoPeek.enabled)
    {
        hasShot = false;
        peekPosition = Vector{};
        return;
    }

    if (!localPlayer->isAlive())
    {
        hasShot = false;
        peekPosition = Vector{};
        return;
    }

    auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon)
        return;

    if (const auto mt = localPlayer->moveType(); mt == MoveType::LADDER || mt == MoveType::NOCLIP || !(localPlayer->flags() & 1))
        return;

    if (config->misc.autoPeekKey.isActive())
    {
        if (peekPosition.null())
            peekPosition = localPlayer->getRenderOrigin();

        auto revolver_shoot = activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver && !AntiAim::r8Working && (UserCmd::IN_ATTACK | UserCmd::IN_ATTACK2);

        if (revolver_shoot)
            hasShot = true;

        if (cmd->buttons & UserCmd::IN_ATTACK)
            hasShot = true;

        if (hasShot)
        {
            const float yaw = currentViewAngles.y;
            const auto difference = localPlayer->getRenderOrigin() - peekPosition;

            if (difference.length2D() > 2.5f)
            {
                const auto velocity = Vector{
                    difference.x * std::cos(yaw / 180.0f * 3.141592654f) + difference.y * std::sin(yaw / 180.0f * 3.141592654f),
                    difference.y * std::cos(yaw / 180.0f * 3.141592654f) - difference.x * std::sin(yaw / 180.0f * 3.141592654f),
                    difference.z };

                cmd->forwardmove = -velocity.x * 20.f;
                cmd->sidemove = velocity.y * 20.f;
            }
            else
            {
                hasShot = false;
                peekPosition = Vector{};
            }
        }
    }
    else
    {
        hasShot = false;
        peekPosition = Vector{};
    }
}

void Misc::unlockHiddenCvars() noexcept
{
    auto iterator = **reinterpret_cast<conCommandBase***>(interfaces->cvar + 0x34);
    for (auto c = iterator->next; c != nullptr; c = c->next)
    {
        conCommandBase* cmd = c;
        cmd->flags &= ~(1 << 1);
        cmd->flags &= ~(1 << 4);
    }
}

void Misc::fakeDuck(UserCmd* cmd, bool& sendPacket) noexcept
{
    if (!config->misc.fakeduck || !config->misc.fakeduckKey.isActive())
        return;

    if (!interfaces->engine->isConnected())
        return;

    if (const auto gameRules = (*memory->gameRules); gameRules)
        if ((getGameMode() == GameMode::Casual || getGameMode() == GameMode::Deathmatch || getGameMode() == GameMode::ArmsRace || getGameMode() == GameMode::WarGames || getGameMode() == GameMode::Demolition) && gameRules->isValveDS())
            return;

    if (!localPlayer || !localPlayer->isAlive() || !(localPlayer->flags() & 1))
        return;

    const auto netChannel = interfaces->engine->getNetworkChannel();
    if (!netChannel)
        return;

    cmd->buttons |= UserCmd::IN_BULLRUSH;
    const bool crouch = netChannel->chokedPackets >= (maxUserCmdProcessTicks / 2);

    if (crouch)
        cmd->buttons |= UserCmd::IN_DUCK;
    else
        cmd->buttons &= ~UserCmd::IN_DUCK;

    sendPacket = netChannel->chokedPackets >= maxUserCmdProcessTicks;
}

void Misc::slowwalk(UserCmd* cmd) noexcept
{
    if (!config->misc.slowwalk || !config->misc.slowwalkKey.isActive())
        return;

    if (!interfaces->engine->isConnected())
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon)
        return;

    const auto weaponData = activeWeapon->getWeaponData();
    if (!weaponData)
        return;

    const float maxSpeed = config->misc.slowwalkAmnt ? config->misc.slowwalkAmnt : (localPlayer->isScoped() ? weaponData->maxSpeedAlt : weaponData->maxSpeed) / 3;

    if (cmd->forwardmove && cmd->sidemove) 
    {
        const float maxSpeedRoot = maxSpeed * static_cast<float>(M_SQRT1_2);
        cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
        cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
    } 
    else if (cmd->forwardmove) 
        cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeed : maxSpeed; 
    else if (cmd->sidemove) 
        cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeed : maxSpeed;
}

void Misc::inverseRagdollGravity() noexcept
{
    static auto ragdollGravity = interfaces->cvar->findVar(skCrypt("cl_ragdoll_gravity"));
    static auto backup = ragdollGravity->getInt();
    ragdollGravity->setValue(config->visuals.inverseRagdollGravity ? config->misc.ragdollGravity : backup);
}

enum
{
    IDoubleTap = 0,
    IOnShot = 1,
    IDA = 2,
    IDmgOverride = 3,
    IManual = 4,
    IFreestand = 5,
    IBaim = 6,
    ISafe = 7,
    IAutoPeek = 8,
    IFakeDuck = 9
};

void Misc::Indictators() noexcept
{
    if (!interfaces->engine->isConnected())
        return;

    if (!config->misc.indicators.enabled)
        return;

    ImVec2 s = ImGui::GetIO().DisplaySize;

    const auto [width, height] = interfaces->surface->getScreenSize();
    int h = 0;
    static auto percent_col = [](int per) -> Color {
        int red = per < 50 ? 255 : floorf(255 - (per * 2 - 100) * 255.f / 100.f);
        int green = per > 50 ? 255 : floorf((per * 2) * 255.f / 100.f);

        return Color(red, green, 0);
    };

    if ((config->misc.indicators.toShow & 1 << IDA) == 1 << IDA)
    {
        if (config->rageAntiAim[current_category].atTargets)
        {
            std::string indicator = c_xor("DA");
            Color color = Color(210, 240, 0, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }

    if ((config->misc.indicators.toShow & 1 << IManual) == 1 << IManual)
    {
        if (config->manualBackward.isActive())
        {
            std::string indicator = c_xor("AA BACKWARD");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }

        if (config->manualForward.isActive())
        {
            std::string indicator = c_xor("AA FORWARD");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }

        if (config->manualLeft.isActive())
        {
            std::string indicator = c_xor("AA LEFT");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }

        if (config->manualRight.isActive())
        {
            std::string indicator = c_xor("AA RIGHT");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }

    if ((config->misc.indicators.toShow & 1 << IDoubleTap) == 1 << IDoubleTap)
    {
        if (config->tickbase.doubletap.isActive())
        {
            std::string indicator = c_xor("DT");
            Color color;

            if (memory->globalVars->realtime - Tickbase::realTime > 0.24625f && localPlayer && localPlayer->isAlive() && localPlayer->getActiveWeapon() && localPlayer->getActiveWeapon()->nextPrimaryAttack() <= (localPlayer->tickBase() - Tickbase::getTargetTickShift()) * memory->globalVars->intervalPerTick
                && (config->misc.fakeduck && !config->misc.fakeduckKey.isActive()))
                color = Color(255, 255, 255, 200);
            else
                color = Color(255, 0, 0, 200);

            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }

    if ((config->misc.indicators.toShow & 1 << IOnShot) == 1 << IOnShot)
    {
        if (config->tickbase.hideshots.isActive())
        {
            std::string indicator = c_xor("H$");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }

    if ((config->misc.indicators.toShow & 1 << IFreestand) == 1 << IFreestand)
    {
        if (config->freestandKey.isActive() && config->rageAntiAim[goofy].freestand)
        {
            std::string indicator = c_xor("FS");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }

    if ((config->misc.indicators.toShow & 1 << IBaim) == 1 << IBaim)
    {
        if (config->forceBaim.isActive())
        {
            std::string indicator = c_xor("BAIM");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }

   /* if ((config->misc.indicators.toShow & 1 << ISafe) == 1 << ISafe)
    {
        if (config->forceSafePoints.isActive())
        {
            std::string indicator = c_xor("SAFE");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }*/

    if ((config->misc.indicators.toShow & 1 << IBaim) == 1 << IBaim)
    {
        if (config->forceBaim.isActive())
        {
            std::string indicator = c_xor("BAIM");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }

    //if ((config->misc.indicators.toShow & 1 << IHcOverride) == 1 << IHcOverride)
    //{
    //    if (config->hitchanceOverride.isActive())
    //    {
    //        std::string indicator = c_xor("HC OVR");
    //        Color color = Color(255, 255, 255, 200);
    //        Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
    //        Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
    //        Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
    //        Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
    //        h += 36;
    //    }
    //}

    if ((config->misc.indicators.toShow & 1 << IDmgOverride) == 1 << IDmgOverride)
    {
        if (config->minDamageOverrideKey.isActive())
        {
            std::string indicator = c_xor("DMG");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }

    if ((config->misc.indicators.toShow & 1 << IAutoPeek) == 1 << IAutoPeek)
    {
        if (config->misc.autoPeek.enabled && config->misc.autoPeekKey.isActive())
        {
            std::string indicator = c_xor("PEEK");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }

    if ((config->misc.indicators.toShow & 1 << IFakeDuck) == 1 << IFakeDuck)
    {
        if (config->misc.fakeduck && config->misc.fakeduckKey.isActive())
        {
            std::string indicator = c_xor("DUCK");
            Color color = Color(255, 255, 255, 200);
            Render::gradient(14, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 0), Color(0, 0, 0, 165), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::gradient(14 + Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, height - 340 - h - 3, Render::textWidth(hooks->IndShadow, indicator.c_str()) / 2, 33, Color(0, 0, 0, 165), Color(0, 0, 0, 0), Render::GradientType::GRADIENT_HORIZONTAL);
            Render::drawText(hooks->IndShadow, Color(0, 0, 0, 200), ImVec2{ 23 + 1, (float)(height - 340 - h + 1) }, indicator.c_str());
            Render::drawText(hooks->IndFont, color, ImVec2{ 23, (float)(height - 340 - h) }, indicator.c_str());
            h += 36;
        }
    }
}

static void staticScope(ImDrawList* drawList, const ImVec2& pos, ImU32 c) noexcept
{
    //left
    drawList->AddRectFilled(ImVec2{ pos.x + 0, pos.y + 0 }, ImVec2{ pos.x - 3900, pos.y + 1 }, c);
    //right
    drawList->AddRectFilled(ImVec2{ pos.x + 0, pos.y + 0 }, ImVec2{ pos.x + 3900, pos.y + 1 }, c);
    //top
    drawList->AddRectFilled(ImVec2{ pos.x + 0, pos.y + 0 }, ImVec2{ pos.x + 1, pos.y - 3900 }, c);
    //bottom
    drawList->AddRectFilled(ImVec2{ pos.x + 0, pos.y + 0 }, ImVec2{ pos.x + 1, pos.y + 3900 }, c);
}

void drawTriangleFromCenter(ImDrawList* drawList, const ImVec2& pos, unsigned color, bool outline) noexcept
{
    const auto l = std::sqrtf(ImLengthSqr(pos));
    if (!l) return;
    const auto posNormalized = pos / l;
    const auto center = ImGui::GetIO().DisplaySize / 2 + pos;

    const ImVec2 trianglePoints[] = {
            center + ImVec2{  0.5f * posNormalized.y, -0.5f * posNormalized.x } *config->condAA.visualizeSize,
            center + ImVec2{  1.0f * posNormalized.x,  1.0f * posNormalized.y } *config->condAA.visualizeSize,
            center + ImVec2{ -0.5f * posNormalized.y,  0.5f * posNormalized.x } *config->condAA.visualizeSize,
    };

    drawList->AddConvexPolyFilled(trianglePoints, 3, color);
    if (outline)
        drawList->AddPolyline(trianglePoints, 3, color | IM_COL32_A_MASK, ImDrawFlags_Closed, 1.5f);
}

void Misc::aaArrows(ImDrawList* drawList) noexcept 
{
    if (!config->condAA.visualize.enabled)
        return;

    if (!interfaces->engine->isInGame() || !interfaces->engine->isConnected())
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (memory->input->isCameraInThirdPerson)
        return;

    bool invert;
    bool isInvertToggled = config->invert.isActive();
    if (config->rageAntiAim[static_cast<int>(goofy)].peekMode != 3)
        invert = isInvertToggled;

    ImVec2 pos{ ImGui::GetIO().DisplaySize / 2 };
    ImU32 col{ Helpers::calculateColor(static_cast<Color4>(config->condAA.visualize)) };
    ImU32 col1{ Helpers::calculateColor(0, 0, 0, static_cast<int>(config->condAA.visualize.color[3] * 255.f))};

    if (config->condAA.visualizeType == 0)
    {
        if (config->rageAntiAim[static_cast<int>(goofy)].desync && config->rageAntiAim[static_cast<int>(goofy)].peekMode != 3)
        {
            if (!invert)
                drawTriangleFromCenter(drawList, { config->condAA.visualizeOffset, 0 }, col, config->condAA.visualize.outline);
            else if (invert)
                drawTriangleFromCenter(drawList, { -config->condAA.visualizeOffset, 0 }, col, config->condAA.visualize.outline);
        }
    }

    if (config->condAA.visualizeType == 1)
    {
        if (config->manualForward.isActive())
            drawTriangleFromCenter(drawList, { 0, -config->condAA.visualizeOffset }, col, config->condAA.visualize.outline);
        if (config->manualBackward.isActive())
            drawTriangleFromCenter(drawList, { 0, config->condAA.visualizeOffset }, col, config->condAA.visualize.outline);
        if (config->manualRight.isActive() || (AntiAim::auto_direction_yaw == 1 && config->freestandKey.isActive()))
            drawTriangleFromCenter(drawList, { config->condAA.visualizeOffset, 0 }, col, config->condAA.visualize.outline);
        if (config->manualLeft.isActive() || (AntiAim::auto_direction_yaw == -1 && config->freestandKey.isActive()))
            drawTriangleFromCenter(drawList, { -config->condAA.visualizeOffset, 0 }, col, config->condAA.visualize.outline);
    }
}

void Misc::customScope() noexcept
{
    if (!interfaces->engine->isInGame() || !interfaces->engine->isConnected())
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (!localPlayer->getActiveWeapon())
        return;

    if (localPlayer->getActiveWeapon()->itemDefinitionIndex2() == WeaponId::Sg553 || localPlayer->getActiveWeapon()->itemDefinitionIndex2() == WeaponId::Aug)
        return;

    if (config->visuals.scope.type == 0)
        return;

    if (config->visuals.scope.type == 1)
    {
        if (localPlayer->isScoped())
        {
            const auto [width, height] = interfaces->surface->getScreenSize();
            interfaces->surface->setDrawColor(0, 0, 0, 255);
            interfaces->surface->drawFilledRect(0, height / 2, width, height / 2 + 1);
            interfaces->surface->setDrawColor(0, 0, 0, 255);
            interfaces->surface->drawFilledRect(width / 2, 0, width / 2 + 1, height);
        }
    }

    if (config->visuals.scope.type == 2)
    {
        if (localPlayer->isScoped() && config->visuals.scope.fade)
        {
            auto offset = config->visuals.scope.offset;
            auto leng = config->visuals.scope.length;
            auto accent = Color(config->visuals.scope.color.color[0], config->visuals.scope.color.color[1], config->visuals.scope.color.color[2], config->visuals.scope.color.color[3]);
            auto accent2 = Color(config->visuals.scope.color.color[0], config->visuals.scope.color.color[1], config->visuals.scope.color.color[2], 0.f);
            const auto [width, height] = interfaces->surface->getScreenSize();
            //right
            if (!config->visuals.scope.removeRight)
            Render::gradient(width / 2 + offset, height / 2, leng, 1, accent, accent2, Render::GradientType::GRADIENT_HORIZONTAL);
            //left
            if (!config->visuals.scope.removeLeft)
            Render::gradient(width / 2 - leng - offset, height / 2, leng, 1, accent2, accent, Render::GradientType::GRADIENT_HORIZONTAL);
            //bottom
            if (!config->visuals.scope.removeBottom)
            Render::gradient(width / 2, height / 2 + offset, 1, leng, accent, accent2, Render::GradientType::GRADIENT_VERTICAL);
            //top
            if (!config->visuals.scope.removeTop)
            Render::gradient(width / 2, height / 2 - leng - offset, 1, leng, accent2, accent, Render::GradientType::GRADIENT_VERTICAL);
        }
        else if (localPlayer->isScoped() && !config->visuals.scope.fade)
        {
            auto offset = config->visuals.scope.offset;
            auto leng = config->visuals.scope.length;
            auto accent = Color(config->visuals.scope.color.color[0], config->visuals.scope.color.color[1], config->visuals.scope.color.color[2], config->visuals.scope.color.color[3]);
            const auto [width, height] = interfaces->surface->getScreenSize();
            //right
            if (!config->visuals.scope.removeRight)
            Render::rectFilled(width / 2 + offset, height / 2, leng, 1, accent);
            //left
            if (!config->visuals.scope.removeLeft)
            Render::rectFilled(width / 2 - leng - offset, height / 2, leng, 1, accent);
            //bottom
            if (!config->visuals.scope.removeBottom)
            Render::rectFilled(width / 2, height / 2 + offset, 1, leng, accent);
            //top
            if (!config->visuals.scope.removeTop)
            Render::rectFilled(width / 2, height / 2 - leng - offset, 1, leng, accent);
        }
    }

    if (config->visuals.scope.type == 3)
        return;
}

void Misc::updateClanTag(bool tagChanged) noexcept
{
    static bool wasEnabled = false;
    if (!config->misc.clantag && wasEnabled)
    {
        memory->setClanTag(" ", " ");
        wasEnabled = false;
    }

    if (!config->misc.clantag)
        return; 

    static int lastTime = 0;
    int time = memory->globalVars->currenttime * 3;

    if (config->misc.clantag)
    {
        wasEnabled = true;

        switch (config->misc.clantag_type)
        {
        case 0:
            if (time != lastTime)
            {
                switch (time % 14)
                {
                case 1: { memory->setClanTag(skCrypt(" e| "), skCrypt(" e| ")); break; }
                case 2: { memory->setClanTag(skCrypt(" ec| "), skCrypt(" ec| ")); break; }
                case 3: { memory->setClanTag(skCrypt(" ecv| "), skCrypt(" ecv| ")); break; }
                case 4: { memory->setClanTag(skCrypt(" ecva| "), skCrypt(" ecva| ")); break; }
                case 5: { memory->setClanTag(skCrypt(" ecvat| "), skCrypt(" ecvat| ")); break; }
                case 6: { memory->setClanTag(skCrypt(" ecvato| "), skCrypt(" ecvato| ")); break; }
                case 7: { memory->setClanTag(skCrypt(" ecvator| "), skCrypt(" ecvator| ")); break; }
                case 8: { memory->setClanTag(skCrypt(" ecvator| "), skCrypt(" ecvator| ")); break; }
                case 9: { memory->setClanTag(skCrypt(" ecvato| "), skCrypt(" ecvato| ")); break; }
                case 10: { memory->setClanTag(skCrypt(" ecvat| "), skCrypt(" ecvat| ")); break; }
                case 11: { memory->setClanTag(skCrypt(" ecva| "), skCrypt(" ecva| ")); break; }
                case 12: { memory->setClanTag(skCrypt(" ecv| "), skCrypt(" ecv| ")); break; }
                case 13: { memory->setClanTag(skCrypt(" ec| "), skCrypt(" ec| ")); break; }
                case 14: { memory->setClanTag(skCrypt(" e| "), skCrypt(" e| ")); break; }
                }
            }
            break;
        case 1:
            if (time != lastTime)
            {
                switch (time % 24)
                {
                case 1: { memory->setClanTag(skCrypt(" 3 "), skCrypt(" 3 ")); break; }
                case 2: { memory->setClanTag(skCrypt(" e "), skCrypt(" e ")); break; }
                case 3: { memory->setClanTag(skCrypt(" e<| "), skCrypt(" e< ")); break; }
                case 4: { memory->setClanTag(skCrypt(" ec "), skCrypt(" ec ")); break; }
                case 5: { memory->setClanTag(skCrypt(" ecV "), skCrypt(" ecV| ")); break; }
                case 6: { memory->setClanTag(skCrypt(" ecv "), skCrypt(" ecv ")); break; }
                case 7: { memory->setClanTag(skCrypt(" ecv4 "), skCrypt(" ecv4 ")); break; }
                case 8: { memory->setClanTag(skCrypt(" ecva| "), skCrypt(" ecva ")); break; }
                case 9: { memory->setClanTag(skCrypt(" ecva7| "), skCrypt(" ecva7 ")); break; }
                case 10: { memory->setClanTag(skCrypt(" ecvat "), skCrypt(" ecvat ")); break; }
                case 11: { memory->setClanTag(skCrypt(" ecvat0 "), skCrypt(" ecvat0 ")); break; }
                case 12: { memory->setClanTag(skCrypt(" ecvato "), skCrypt(" ecvato ")); break; }
                case 13: { memory->setClanTag(skCrypt(" ecvato2 "), skCrypt(" ecvato2 ")); break; }
                case 14: { memory->setClanTag(skCrypt(" ecvator "), skCrypt(" ecvator ")); break; }
                case 15: { memory->setClanTag(skCrypt(" ecvato2 "), skCrypt(" ecvato2 ")); break; }
                case 16: { memory->setClanTag(skCrypt(" ecvato "), skCrypt(" ecvato ")); break; }
                case 17: { memory->setClanTag(skCrypt(" ecvat0 "), skCrypt(" ecvat0 ")); break; }
                case 18: { memory->setClanTag(skCrypt(" ecvat "), skCrypt(" ecvat ")); break; }
                case 19: { memory->setClanTag(skCrypt(" ecva7 "), skCrypt(" ecva7 ")); break; }
                case 20: { memory->setClanTag(skCrypt(" ecv4 "), skCrypt(" ecv4 ")); break; }
                case 21: { memory->setClanTag(skCrypt(" ecv "), skCrypt(" ecv ")); break; }
                case 22: { memory->setClanTag(skCrypt(" ecV "), skCrypt(" ecV ")); break; }
                case 23: { memory->setClanTag(skCrypt(" e< "), skCrypt(" e< ")); break; }
                case 24: { memory->setClanTag(skCrypt(" 3 "), skCrypt(" 3 ")); break; }
                }
            }
            break;
        case 2:
            if (time != lastTime)
            {
                switch (time % 9)
                {
                case 1: { memory->setClanTag(skCrypt(" ga "), skCrypt(" ga ")); break; }
                case 2: { memory->setClanTag(skCrypt(" gam"), skCrypt(" gam ")); break; }
                case 3: { memory->setClanTag(skCrypt(" game "), skCrypt(" game ")); break; }
                case 4: { memory->setClanTag(skCrypt(" games "), skCrypt(" games ")); break; }
                case 5: { memory->setClanTag(skCrypt(" gamese "), skCrypt(" gamese ")); break; }
                case 6: { memory->setClanTag(skCrypt(" gamesen "), skCrypt(" gamesen ")); break; }
                case 7: { memory->setClanTag(skCrypt(" gamesens "), skCrypt(" gamesens ")); break; }
                case 8: { memory->setClanTag(skCrypt(" gamesense "), skCrypt(" gamesense ")); break; }
                }
            }
            break;
        }

        lastTime = time;
    }
}

const bool anyActiveKeybinds() noexcept
{
    const bool rageBot = config->ragebotKey.canShowKeybind();
    const bool minDamageOverride = config->minDamageOverrideKey.canShowKeybind();
    const bool fakeAngle = config->rageAntiAim[static_cast<int>(goofy)].desync && config->invert.canShowKeybind();
    const bool antiAimManualForward = config->condAA.global && config->manualForward.canShowKeybind();
    const bool antiAimManualBackward = config->condAA.global && config->manualBackward.canShowKeybind();
    const bool antiAimManualRight = config->condAA.global && config->manualRight.canShowKeybind();
    const bool antiAimManualLeft = config->condAA.global && config->manualLeft.canShowKeybind();
    const bool doubletap = config->tickbase.doubletap.canShowKeybind();
    const bool hideshots = config->tickbase.hideshots.canShowKeybind();
    const bool glow = config->glowKey.canShowKeybind();
    const bool chams = config->chamsKey.canShowKeybind();
    const bool esp = config->streamProofESP.key.canShowKeybind();

    const bool slowwalk = config->misc.slowwalk && config->misc.slowwalkKey.canShowKeybind();
    const bool fakeduck = config->misc.fakeduck && config->misc.fakeduckKey.canShowKeybind();
    const bool autoPeek = config->misc.autoPeek.enabled && config->misc.autoPeekKey.canShowKeybind();
    const bool baim = config->forceBaim.canShowKeybind();
    const bool freestand = config->rageAntiAim[static_cast<int>(goofy)].freestand && config->freestandKey.canShowKeybind();

    return rageBot || minDamageOverride || fakeAngle || antiAimManualForward || antiAimManualBackward || antiAimManualRight || antiAimManualLeft 
        || doubletap || hideshots || chams || glow || esp
        || slowwalk || fakeduck || autoPeek || baim || freestand;
}

void Misc::showKeybinds() noexcept
{
    if (!config->misc.keybindList.enabled)
        return;

    if (!anyActiveKeybinds && !gui->isOpen())
        return;

    if (config->misc.keybindList.pos != ImVec2{}) {
        ImGui::SetNextWindowPos(config->misc.keybindList.pos);
        config->misc.keybindList.pos = {};
    }

    auto windowFlags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    if (!gui->isOpen())
        windowFlags |= ImGuiWindowFlags_NoInputs;

    ImGui::SetNextWindowSize({ 150.f, 0.f }, ImGuiCond_Once);
    ImGui::PushFont(gui->getFIconsFont());

    auto size = ImGui::CalcTextSize(c_xor("Keybinds"));
    ImGui::SetNextWindowSizeConstraints({ 150.f, 0.f }, { 150.f, size.y * 2 - 2 });
    ImGui::GetBackgroundDrawList();
    ImGui::Begin(c_xor("Keybinds list"), nullptr, windowFlags);
    {
        //ImGui::PushFont(gui->getFIconsFont());
        auto draw_list = ImGui::GetBackgroundDrawList();
        auto p = ImGui::GetWindowPos();
        // set keybinds color
        auto bg_clr = ImColor(0.f, 0.f, 0.f, config->menu.transparency / 100);
        auto line_clr = Helpers::calculateColor(config->menu.accentColor);
        auto text_clr = ImColor(255, 255, 255, 255);
        auto glow = Helpers::calculateColor(config->menu.accentColor, 0.0f);
        auto glow1 = Helpers::calculateColor(config->menu.accentColor, config->menu.accentColor.color[3] * 0.5f);
        auto glow2 = Helpers::calculateColor(config->menu.accentColor);
        auto offset = 2;

        draw_list->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + 150, p.y + size.y * 2 - 2), bg_clr, 3.0f);
        // draw line
        // draw_list->AddRect(ImVec2(p.x - 1, p.y - 1), ImVec2(p.x + 151, p.y + size.y * 2 - 1), line_clr, 5.f, 0, 1.5f);

        // draw text
        draw_list->AddText(
            ImVec2(p.x + 75 - size.x / 2, p.y + size.y / 2 - 1),
            text_clr,
            c_xor("Keybinds")
        );

        auto textO = ImGui::CalcTextSize(c_xor("[toggle]"));
        auto textH = ImGui::CalcTextSize(c_xor("[hold]"));
        auto textA = ImGui::CalcTextSize(c_xor("[on]"));

        //dt
        if (config->tickbase.doubletap.isActive())
        {
            if (config->tickbase.doubletap.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Double tap"));
                draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                offset = offset + 1;
            }
            else
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Double tap"));
                if (config->tickbase.doubletap.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->tickbase.doubletap.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //hs
        if (config->tickbase.hideshots.isActive())
        {
            if (config->tickbase.hideshots.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Hide shots"));
                draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                offset = offset + 1;
            }
            else
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Hide shots"));
                if (config->tickbase.hideshots.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->tickbase.hideshots.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //dmg
        if (config->minDamageOverrideKey.isActive())
        {
            if (config->minDamageOverrideKey.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Override DMG"));
                draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                offset = offset + 1;
            }
            else
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Override DMG"));
                if (config->minDamageOverrideKey.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->minDamageOverrideKey.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //baim
        if (config->forceBaim.isActive())
        {
            if (config->forceBaim.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Force baim"));
                draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                offset = offset + 1;
            }
            else
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Force baim"));
                if (config->forceBaim.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->forceBaim.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //fs
        if (config->freestandKey.isActive() && config->rageAntiAim[static_cast<int>(goofy)].freestand)
        {
            if (config->freestandKey.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Freestand"));
                draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                offset = offset + 1;
            }
            else
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Freestand"));
                if (config->freestandKey.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->freestandKey.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //manual
        if (config->condAA.global)
        {
            if (config->manualBackward.isActive())
            {
                if (config->manualBackward.CSB())
                {
                    draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Backward"));
                    draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                    offset = offset + 1;
                }
                else
                {
                    draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Backward"));
                    if (config->manualBackward.isDown())
                        draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                    else if (config->manualBackward.isToggled())
                        draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                    offset = offset + 1;
                }
            }
            if (config->manualForward.isActive())
            {
                if (config->manualForward.CSB())
                {
                    draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Forward"));
                    draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                    offset = offset + 1;
                }
                else
                {
                    draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Forward"));
                    if (config->manualForward.isDown())
                        draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                    else if (config->manualForward.isToggled())
                        draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                    offset = offset + 1;
                }
            }
            if (config->manualLeft.isActive())
            {
                if (config->manualLeft.CSB())
                {
                    draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Left"));
                    draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                    offset = offset + 1;
                }
                else
                {
                    draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Left"));
                    if (config->manualLeft.isDown())
                        draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                    else if (config->manualLeft.isToggled())
                        draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                    offset = offset + 1;
                }
            }
            if (config->manualRight.isActive())
            {
                if (config->manualRight.CSB())
                {
                    draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Right"));
                    draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                    offset = offset + 1;
                }
                else
                {
                    draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Right"));
                    if (config->manualRight.isDown())
                        draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                    else if (config->manualRight.isToggled())
                        draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                    offset = offset + 1;
                }
            }
        }
        //invert
        if (config->invert.isActive() && config->rageAntiAim[static_cast<int>(goofy)].desync)
        {
            if (config->invert.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Desync invert"));
                draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                offset = offset + 1;
            }
            else
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Desync invert"));
                if (config->invert.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->invert.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //fake duck
        if (config->misc.fakeduckKey.isActive() && config->misc.fakeduck)
        {
            if (config->misc.fakeduckKey.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Fake duck"));
                draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                offset = offset + 1;
            }
            else if (!config->misc.fakeduckKey.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Fake duck"));
                if (config->misc.fakeduckKey.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->misc.fakeduckKey.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //auto peek
        if (config->misc.autoPeek.enabled && config->misc.autoPeekKey.isActive())
        {
            if (config->misc.autoPeekKey.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Auto peek"));
                draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                offset = offset + 1;
            }
            else
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Auto peek"));
                if (config->misc.autoPeekKey.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->misc.autoPeekKey.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //esp
        if (config->streamProofESP.key.isActive())
        {
            if (!config->streamProofESP.key.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("ESP"));
                if (config->streamProofESP.key.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->streamProofESP.key.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //glow
        if (config->glowKey.isActive())
        {
            if (!config->glowKey.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Glow"));
                if (config->glowKey.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->glowKey.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //chams
        if (config->chamsKey.isActive())
        {
            if (!config->chamsKey.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Chams"));
                if (config->chamsKey.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->chamsKey.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }
        //slowwalk
        if (config->misc.slowwalk && config->misc.slowwalkKey.isActive())
        {
            if (config->misc.slowwalkKey.CSB())
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Slow walk"));
                draw_list->AddText(ImVec2(p.x - textA.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[on]"));
                offset = offset + 1;
            }
            else
            {
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("Slow walk"));
                if (config->misc.slowwalkKey.isDown())
                    draw_list->AddText(ImVec2(p.x - textH.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[hold]"));
                else if (config->misc.slowwalkKey.isToggled())
                    draw_list->AddText(ImVec2(p.x - textO.x - 4.5f + 150, p.y + 13.5f * offset), ImColor(1.f, 1.f, 1.f, 1.f), c_xor("[toggle]"));
                offset = offset + 1;
            }
        }

        ImGui::PopFont();
    }
    ImGui::End();
}

void Misc::spectatorList() noexcept
{
    if (!config->misc.spectatorList.enabled)
        return;

    GameData::Lock lock;

    const auto& observers = GameData::observers();

    if (std::ranges::none_of(observers, [](const auto& obs) { return obs.targetIsLocalPlayer; }) && !gui->isOpen())
        return;

    if (config->misc.spectatorList.pos != ImVec2{}) {
        ImGui::SetNextWindowPos(config->misc.spectatorList.pos);
        config->misc.spectatorList.pos = {};
    }

    ImGui::SetNextWindowSize({ 150.f, 0.f }, ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints({ 150.f, 0.f }, { 150.f, FLT_MAX });

    auto windowFlags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    if (!gui->isOpen())
        windowFlags += ImGuiWindowFlags_NoInputs;

    windowFlags |= ImGuiWindowFlags_NoTitleBar;
    ImGui::Begin(skCrypt("Spectator list"), nullptr, windowFlags);
    {        
        ImGui::PushFont(gui->getFIconsFont());
        auto draw_list = ImGui::GetBackgroundDrawList();
        auto p = ImGui::GetWindowPos();
        // set keybinds color
        auto bg_clr = ImColor(0.f, 0.f, 0.f, config->menu.transparency / 100);
        auto line_clr = Helpers::calculateColor(config->menu.accentColor);
        auto text_clr = ImColor(255, 255, 255, 255);
        auto glow = Helpers::calculateColor(config->menu.accentColor, 0.0f);
        auto glow1 = Helpers::calculateColor(config->menu.accentColor, config->menu.accentColor.color[3] * 0.5f);
        auto glow2 = Helpers::calculateColor(config->menu.accentColor);
        auto size = ImGui::CalcTextSize(skCrypt("Spectators"));
        auto offset = 2;

        // draw bg
        draw_list->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + 150, p.y + size.y * 2 - 2), bg_clr, 3.0f);

        // draw text
        draw_list->AddText(
            ImVec2(p.x + 75 - size.x / 2, p.y + size.y / 2 - 1),
            text_clr,
            c_xor("Spectators"));

        // draw_list->AddRect(ImVec2(p.x - 1, p.y - 1), ImVec2(p.x + 151, p.y + size.y * 2 - 1), line_clr, 5.f, 0, 1.5f);

        for (const auto& observer : observers)
        {
            if (!observer.targetIsLocalPlayer)
                continue;

            if (const auto it = std::ranges::find(GameData::players(), observer.playerHandle, &PlayerData::handle); it != GameData::players().cend())
            {
                auto obsMode{ "" };
                ImVec2 text;
                if (it->observerMode == ObsMode::InEye)
                {
                    obsMode = "1st";
                    text = ImGui::CalcTextSize("1st");
                }
                else if (it->observerMode == ObsMode::Chase)
                {
                    obsMode = "3rd";
                    text = ImGui::CalcTextSize("3rd");
                }
                else if (it->observerMode == ObsMode::Roaming)
                {
                    obsMode = "Freecam";
                    text = ImGui::CalcTextSize("Freecam");
                }
                //draw_list->AddText(ImVec2(p.x + 150.f - 4.5f - text.x, p.y + 15 * offset), ImColor(1.f, 1.f, 1.f, 1.f), obsMode);
                draw_list->AddText(ImVec2(p.x + 4.5f, p.y + 13.5 * offset), ImColor(1.f, 1.f, 1.f, 1.f), it->name.c_str());
                offset = offset + 1;
            }
        }
        ImGui::PopFont();
    }
    ImGui::End();
}

void Misc::noscopeCrosshair() noexcept
{
    static auto showSpread = interfaces->cvar->findVar(skCrypt("weapon_debug_spread_show"));
    showSpread->setValue(config->misc.noscopeCrosshair && localPlayer && !localPlayer->isScoped() ? 3 : 0);
}

void Misc::recoilCrosshair() noexcept
{
    static auto recoilCrosshair = interfaces->cvar->findVar(skCrypt("cl_crosshair_recoil"));
    recoilCrosshair->setValue(config->misc.recoilCrosshair ? 1 : 0);
}

bool offsetSpot{ false };

void Misc::watermark() noexcept
{
    if (!config->misc.wm.enabled)
        return;

    auto draw_list = ImGui::GetBackgroundDrawList();
    const char* name = interfaces->engine->getSteamAPIContext()->steamFriends->getPersonaName();
    std::string namey = name;
    static auto lastTime = 0.0f;
    const auto time = std::time(nullptr);
    static auto frameRate = 1.0f;
    frameRate = 0.9f * frameRate + 0.1f * memory->globalVars->absoluteFrameTime;

    float latency = 0.0f;
    if (auto networkChannel = interfaces->engine->getNetworkChannel(); networkChannel && networkChannel->getLatency(0) > 3.0f) 
        latency = networkChannel->getLatency(0);

    std::time_t t = std::time(nullptr);
    auto localTime = std::localtime(&time);
    const auto [screenWidth, screenHeight] = interfaces->surface->getScreenSize();

    char s[11];
    s[0] = '\0';

    snprintf(s, sizeof(s), skCrypt("%02d:%02d:%02d"), localTime->tm_hour, localTime->tm_min, localTime->tm_sec);

    std::string fuck = s;
    std::ostringstream text; text << c_xor("ecvator") << (" | " + namey) << (" | " + fuck);
    
    ImGui::Begin(c_xor("##WIM"), NULL, ImGuiWindowFlags_::ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_::ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_::ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
    {
        ImGui::PushFont(gui->getFIconsFont());
        // get window pos
        auto p = ImGui::GetWindowPos();

        // set watermark color
        auto bg_clr = ImColor(0.f, 0.f, 0.f, config->menu.transparency / 100);
        auto line_clr = Helpers::calculateColor(config->menu.accentColor);
        auto text_clr = ImColor(255, 255, 255, 255);
        auto glow = Helpers::calculateColor(config->menu.accentColor, 0.0f);
        auto glow1 = Helpers::calculateColor(config->menu.accentColor, config->menu.accentColor.color[3] * 0.5f);
        auto glow2 = Helpers::calculateColor(config->menu.accentColor);

        // draw bg
        auto calcText = ImGui::CalcTextSize(text.str().c_str());
        ImGui::SetWindowPos({ screenWidth - calcText.x - 19, 9 });

        draw_list->AddRectFilled(ImVec2(p.x - 4, p.y - 2), ImVec2(p.x + calcText.x + 14, p.y + calcText.y * 2 - 4), bg_clr, 3.0f);
        // draw_list->AddRect(ImVec2(p.x - 5, p.y - 3), ImVec2(p.x + calcText.x + 15, p.y + calcText.y * 2 - 3), line_clr, 5.f, 0, 1.5f);

        // draw text
        draw_list->AddText(
            ImVec2(p.x + 5, p.y + calcText.y / 2 - 3.f),
            text_clr,
            text.str().c_str()
        );
        offsetSpot = true;
        ImGui::PopFont();
    }
    ImGui::End();
}

void Misc::prepareRevolver(UserCmd* cmd) noexcept
{
    if (!localPlayer)
        return;

    if (cmd->buttons & UserCmd::IN_ATTACK)
        return;

    constexpr float revolverPrepareTime{ 0.234375f };

    if (auto activeWeapon = localPlayer->getActiveWeapon(); activeWeapon && activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver)
    {
        const auto time = memory->globalVars->serverTime();
        AntiAim::r8Working = true;
        if (localPlayer->nextAttack() > time)
            return;

        cmd->buttons &= ~UserCmd::IN_ATTACK2;

        static auto readyTime = time + revolverPrepareTime;
        if (activeWeapon->nextPrimaryAttack() <= time)
        {
            if (readyTime <= time)
            {
                if (activeWeapon->nextSecondaryAttack() <= time)
                    readyTime = time + revolverPrepareTime;
                else
                    cmd->buttons |= UserCmd::IN_ATTACK2;
            }
            else
                cmd->buttons |= UserCmd::IN_ATTACK;
        }
        else
            readyTime = time + revolverPrepareTime;
    }
}

void Misc::fastStop(UserCmd* cmd) noexcept
{
    if (!config->misc.fastStop)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER || !(localPlayer->flags() & 1) || cmd->buttons & UserCmd::IN_JUMP)
        return;

    if (cmd->buttons & (UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT | UserCmd::IN_FORWARD | UserCmd::IN_BACK))
        return;
    
    const auto velocity = localPlayer->velocity();
    const auto speed = velocity.length2D();
    if (speed < 15.0f)
        return;
    
    Vector direction = velocity.toAngle();
    direction.y = cmd->viewangles.y - direction.y;

    const auto negatedDirection = Vector::fromAngle(direction) * -speed;
    cmd->forwardmove = negatedDirection.x;
    cmd->sidemove = negatedDirection.y;
}

void Misc::drawBombTimer() noexcept
{
    if (!config->misc.bombTimer.enabled)
        return;

    GameData::Lock lock;

    const auto& plantedC4 = GameData::plantedC4();
    if (plantedC4.blowTime == 0.0f && !gui->isOpen())
        return;

    if (!gui->isOpen()) 
        ImGui::SetNextWindowBgAlpha(0.5f);
    
    static float windowWidth = 200.0f;
    ImGui::SetNextWindowPos({ (ImGui::GetIO().DisplaySize.x - 170.0f) / 2.0f, 60.0f }, ImGuiCond_Once);
    ImGui::SetNextWindowSize({ 150, 0 }, ImGuiCond_Once);

    if (!gui->isOpen())
        ImGui::SetNextWindowSize({ 170, 0 });

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, { 255, 255, 255, 255 });

    if (config->misc.borders)
        ImGui::PushStyleColor(ImGuiCol_Border, { config->menu.accentColor.color[0], config->menu.accentColor.color[1], config->menu.accentColor.color[2], config->menu.accentColor.color[3] });

    ImGui::SetNextWindowSizeConstraints({ 0, -1 }, { FLT_MAX, -1 });
    ImGui::Begin(skCrypt("Bomb Timer"), nullptr, ImGuiWindowFlags_NoTitleBar | (gui->isOpen() ? 0 : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize));
    PostProcessing::performFullscreenBlur(ImGui::GetWindowDrawList(), 1.f);
    ImDrawList* draw;
    ImVec2 pos;
    pos = ImGui::GetWindowPos();
    draw = ImGui::GetWindowDrawList();
    draw->AddText(gui->getTahoma28Font(), 28.f, ImVec2(pos.x + 10, pos.y + 3), ImColor(255, 255, 255), !plantedC4.bombsite ? "A" : "B");
    std::ostringstream ss; ss << skCrypt("Time : ") << (std::max)(static_cast<int>(plantedC4.blowTime - memory->globalVars->currenttime), static_cast<int>(0)) << " s";
    //draw->AddRect()
    ImGui::textUnformattedCentered(ss.str().c_str());

    if (plantedC4.defuserHandle != -1) {
        const bool canDefuse = plantedC4.blowTime >= plantedC4.defuseCountDown;

        if (plantedC4.defuserHandle == GameData::local().handle) 
        {
            if (canDefuse) 
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
                ImGui::textUnformattedCentered(skCrypt("Defusabale"));
            }
            else 
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                ImGui::textUnformattedCentered(skCrypt("RUN!!!!"));
            }
            ImGui::PopStyleColor();
        }
        else if (const auto defusingPlayer = GameData::playerByHandle(plantedC4.defuserHandle)) {
            std::ostringstream ss; ss <<  skCrypt(" Someone is defusing: ") << std::fixed << std::showpoint << std::setprecision(3) << (std::max)(plantedC4.defuseCountDown - memory->globalVars->currenttime, static_cast<float>(0)) << " s";

            ImGui::textUnformattedCentered(ss.str().c_str());

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, canDefuse ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255));
            ImGui::progressBarFullWidth((plantedC4.defuseCountDown - memory->globalVars->currenttime) / plantedC4.defuseLength, 5.0f);
            ImGui::PopStyleColor();
        }
    }

    windowWidth = ImGui::GetCurrentWindow()->SizeFull.x;
    if (config->misc.borders)
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    ImGui::End();
}

void Misc::hurtIndicator() noexcept
{
    if (!config->misc.hurtIndicator.enabled)
        return;

    GameData::Lock lock;
    const auto& local = GameData::local();
    if ((!local.exists || !local.alive) && !gui->isOpen())
        return;

    if (local.velocityModifier >= 0.99f && !gui->isOpen())
        return;

    if (gui->isOpen()) 
        ImGui::SetNextWindowBgAlpha(0.3f);
    else
        ImGui::SetNextWindowBgAlpha(0.0f);

    static float windowWidth = 140.0f;
    ImGui::SetNextWindowPos({ (ImGui::GetIO().DisplaySize.x - 140.0f) / 2.0f, 260.0f }, ImGuiCond_Once);
    ImGui::SetNextWindowSize({ windowWidth, 0 }, ImGuiCond_Once);

    if (!gui->isOpen())
        ImGui::SetNextWindowSize({ windowWidth, 0 });

    ImGui::SetNextWindowSizeConstraints({ 0, -1 }, { FLT_MAX, -1 });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.5f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.0f, 0.0f, 0.0f, 1.0f });
    ImGui::Begin(skCrypt("Hurt Indicator"), nullptr, ImGuiWindowFlags_NoTitleBar | (gui->isOpen() ? 0 : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration));

    std::ostringstream ss; ss << skCrypt("Slowed down ") << static_cast<int>(local.velocityModifier * 100.f) << "%";
    ImGui::textUnformattedCentered(ss.str().c_str());
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Helpers::calculateColor(config->misc.hurtIndicator));
   
    ImGui::progressBarFullWidth(local.velocityModifier, 1.0f);
    windowWidth = ImGui::GetCurrentWindow()->SizeFull.x;

    if (config->misc.borders)
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::End();
}

void Misc::stealNames() noexcept
{
    if (!config->misc.nameStealer)
        return;

    if (!localPlayer)
        return;

    static std::vector<int> stolenIds;

    for (int i = 1; i <= memory->globalVars->maxClients; ++i) 
    {
        const auto entity = interfaces->entityList->getEntity(i);

        if (!entity || entity == localPlayer.get())
            continue;

        PlayerInfo playerInfo;
        if (!interfaces->engine->getPlayerInfo(entity->index(), playerInfo))
            continue;

        if (playerInfo.fakeplayer || std::find(stolenIds.cbegin(), stolenIds.cend(), playerInfo.userId) != stolenIds.cend())
            continue;

        if (changeName(false, (std::string{ playerInfo.name } +'\x1').c_str(), 1.0f))
            stolenIds.push_back(playerInfo.userId);

        return;
    }

    stolenIds.clear();
}

void Misc::disablePanoramablur() noexcept
{
    static auto blur = interfaces->cvar->findVar(skCrypt("@panorama_disable_blur"));
    blur->setValue(config->misc.disablePanoramablur);
}

bool Misc::changeName(bool reconnect, const char* newName, float delay) noexcept
{
    static auto exploitInitialized{ false };

    static auto name{ interfaces->cvar->findVar("name") };

    if (reconnect) 
    {
        exploitInitialized = false;
        return false;
    }

    if (!exploitInitialized && interfaces->engine->isInGame()) 
    {
        if (PlayerInfo playerInfo; localPlayer && interfaces->engine->getPlayerInfo(localPlayer->index(), playerInfo) && (!strcmp(playerInfo.name, "?empty") || !strcmp(playerInfo.name, "\n\xAD\xAD\xAD")))
        {
            exploitInitialized = true;
        } 
        else 
        {
            name->onChangeCallbacks.size = 0;
            name->setValue("\n\xAD\xAD\xAD");
            return false;
        }
    }

    static auto nextChangeTime{ 0.0f };
    if (nextChangeTime <= memory->globalVars->realtime) 
    {
        name->setValue(newName);
        nextChangeTime = memory->globalVars->realtime + delay;
        return true;
    }

    return false;
}

void Misc::bunnyHop(UserCmd* cmd) noexcept
{
    if (!localPlayer)
        return;

    static bool hasLanded = true;
    static int bhopInSeries = 1;
    static float lastTimeInAir{};
    static int chanceToHit = config->misc.bhHc;

    if (config->misc.jumpBug && config->misc.jumpBugKey.isActive())
        return;

    static auto wasLastTimeOnGround{ localPlayer->flags() & 1 };

    chanceToHit = config->misc.bhHc;

    if (bhopInSeries <= 1) 
        chanceToHit = chanceToHit * 1.5;

    if (config->misc.bunnyHop && !(localPlayer->flags() & 1) && localPlayer->moveType() != MoveType::LADDER && !wasLastTimeOnGround)
    {
        if (rand() % 100 <= chanceToHit)
            cmd->buttons &= ~UserCmd::IN_JUMP;
    }

    //memory->globalVars->realtime - lastTimeInAir <= 2 &&

    if (!wasLastTimeOnGround && hasLanded)
    {
        bhopInSeries++;
        lastTimeInAir = memory->globalVars->realtime;
        hasLanded = false;
    }

    if (wasLastTimeOnGround) 
    {
        hasLanded = true;
        if (memory->globalVars->realtime - lastTimeInAir >= 3) 
            bhopInSeries = 0;
    }

    wasLastTimeOnGround = localPlayer->flags() & 1;
}

void Misc::fixTabletSignal() noexcept
{
    if (auto activeWeapon{ localPlayer->getActiveWeapon() }; activeWeapon && activeWeapon->getClientClass()->classId == ClassId::Tablet)
        activeWeapon->tabletReceptionIsBlocked() = false;
}

void Misc::killfeedChanger(GameEvent& event) noexcept
{
    if (!config->misc.killfeedChanger.enabled)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (const auto localUserId = localPlayer->getUserId(); event.getInt(skCrypt("attacker")) != localUserId || event.getInt(skCrypt("userid")) == localUserId)
        return;

    if (config->misc.killfeedChanger.headshot)
        event.setInt("headshot", 1);

    if (config->misc.killfeedChanger.dominated)
        event.setInt("Dominated", 1);

    if (config->misc.killfeedChanger.revenge)
        event.setInt("Revenge", 1);

    if (config->misc.killfeedChanger.penetrated)
        event.setInt("penetrated", 1);

    if (config->misc.killfeedChanger.noscope)
        event.setInt("noscope", 1);

    if (config->misc.killfeedChanger.thrusmoke)
        event.setInt("thrusmoke", 1);

    if (config->misc.killfeedChanger.attackerblind)
        event.setInt("attackerblind", 1);
}

void Misc::fixMovement(UserCmd* cmd, float yaw) noexcept
{
    float oldYaw = yaw + (yaw < 0.0f ? 360.0f : 0.0f);
    float newYaw = cmd->viewangles.y + (cmd->viewangles.y < 0.0f ? 360.0f : 0.0f);
    float yawDelta = newYaw < oldYaw ? fabsf(newYaw - oldYaw) : 360.0f - fabsf(newYaw - oldYaw);
    yawDelta = 360.0f - yawDelta;

    const float forwardmove = cmd->forwardmove;
    const float sidemove = cmd->sidemove;

    cmd->forwardmove = std::cos(Helpers::deg2rad(yawDelta)) * forwardmove + std::cos(Helpers::deg2rad(yawDelta + 90.0f)) * sidemove;
    cmd->sidemove = std::sin(Helpers::deg2rad(yawDelta)) * forwardmove + std::sin(Helpers::deg2rad(yawDelta + 90.0f)) * sidemove;

    if (localPlayer->moveType() != MoveType::LADDER && (config->misc.moonwalk_style == 0 || config->misc.moonwalk_style == 4))
        cmd->buttons &= ~(UserCmd::IN_FORWARD | UserCmd::IN_BACK | UserCmd::IN_MOVERIGHT | UserCmd::IN_MOVELEFT);
}

void Misc::antiAfkKick(UserCmd* cmd) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (localPlayer->velocity().length2D() >= 5.f)
        return;

    if (cmd->commandNumber % 2)
        cmd->buttons |= 1 << 27;
}

void Misc::fixAnimationLOD(FrameStage stage) noexcept
{
    if (stage != FrameStage::RENDER_START)
        return;

    if (!localPlayer)
        return;

    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++)
    {
        Entity* entity = interfaces->entityList->getEntity(i);
        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive())
            continue;

        *reinterpret_cast<int*>(entity + 0xA28) = 0;
        *reinterpret_cast<int*>(entity + 0xA30) = memory->globalVars->framecount;
    }
}

void Misc::autoPistol(UserCmd* cmd) noexcept
{
    if (localPlayer->getActiveWeapon() && localPlayer->getActiveWeapon()->isPistol() && localPlayer->getActiveWeapon()->nextPrimaryAttack() > memory->globalVars->serverTime())
    {
        if (localPlayer->getActiveWeapon()->itemDefinitionIndex2() == WeaponId::Revolver)
            cmd->buttons &= ~UserCmd::IN_ATTACK2;
        else
            cmd->buttons &= ~UserCmd::IN_ATTACK;
    }
}

void Misc::autoReload(UserCmd* cmd) noexcept
{
    if (config->misc.autoReload && localPlayer) 
    {
        const auto activeWeapon = localPlayer->getActiveWeapon();
        if (activeWeapon && getWeaponIndex(activeWeapon->itemDefinitionIndex2()) && !activeWeapon->clip())
            cmd->buttons &= ~(UserCmd::IN_ATTACK | UserCmd::IN_ATTACK2);
    }
}

void Misc::revealRanks(UserCmd* cmd) noexcept
{
    if (config->misc.revealRanks && cmd->buttons & UserCmd::IN_SCORE)
        interfaces->client->dispatchUserMessage(50, 0, 0, nullptr);
}

bool m_strafe_switch{};
float m_max_player_speed{}, m_max_weapon_speed{}, m_prev_view_yaw{};

void Misc::autoStrafe(UserCmd* cmd, Vector& currentViewAngles) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (!config->misc.autoStrafe || localPlayer->flags() & FL_ONGROUND)
        return;

    if (!config->misc.autoStrafe)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if ((EnginePrediction::getFlags() & 1) || localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER)
        return;

    const float speed = localPlayer->velocity().length2D();
    if (speed < 5.0f)
        return;

    static float angle = 0.f;

    const bool back = cmd->buttons & UserCmd::IN_BACK;
    const bool forward = cmd->buttons & UserCmd::IN_FORWARD;
    const bool right = cmd->buttons & UserCmd::IN_MOVERIGHT;
    const bool left = cmd->buttons & UserCmd::IN_MOVELEFT;

    if (back) 
    {
        angle = -180.f;
        if (left)
            angle -= 45.f;
        else if (right)
            angle += 45.f;
    }
    else if (left) 
    {
        angle = 90.f;
        if (back)
            angle += 45.f;
        else if (forward)
            angle -= 45.f;
    }
    else if (right) 
    {
        angle = -90.f;
        if (back)
            angle -= 45.f;
        else if (forward)
            angle += 45.f;
    }
    else 
        angle = 0.f;

    currentViewAngles.y += angle;

    cmd->forwardmove = 0.f;
    cmd->sidemove = 0.f;

    const auto delta = Helpers::normalizeYaw(currentViewAngles.y - Helpers::rad2deg(std::atan2(EnginePrediction::getVelocity().y, EnginePrediction::getVelocity().x)));

    cmd->sidemove = delta > 0.f ? -450.f : 450.f;

    currentViewAngles.y = Helpers::normalizeYaw(currentViewAngles.y - delta);
}

void Misc::removeCrouchCooldown(UserCmd* cmd) noexcept
{
    if (const auto gameRules = (*memory->gameRules); gameRules)
        if (gameRules->isValveDS())
            return;

    if (config->misc.fastDuck)
        cmd->buttons |= UserCmd::IN_BULLRUSH;
}

int RandomInt_new(int min, int max) noexcept
{
    return (min + 1) + (((int)rand()) / (int)RAND_MAX) * (max - (min + 1));
}

void Misc::moonwalk(UserCmd* cmd, bool& sendPacket) noexcept
{
    const auto netChannel = interfaces->engine->getNetworkChannel();
    if (!netChannel)
        return;

    if (config->misc.moonwalk_style > 0 && config->misc.moonwalk_style != 4 && localPlayer && localPlayer->moveType() != MoveType::LADDER)
    {
        if (!sendPacket)
            cmd->buttons ^= UserCmd::IN_FORWARD | UserCmd::IN_BACK | UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT;
    }
}

void Misc::playHitSound(GameEvent& event) noexcept
{
    if (!config->misc.hitSound)
        return;

    if (!localPlayer)
        return;

    if (const auto localUserId = localPlayer->getUserId(); event.getInt(c_xor("attacker")) != localUserId || event.getInt(c_xor("userid")) == localUserId)
        return;

    if (config->misc.hitSound == 0)
        return;
    else if (config->misc.hitSound == 1)
        interfaces->engine->clientCmdUnrestricted(c_xor("play survival/paradrop_idle_01.wav"));
    else if (config->misc.hitSound == 2)
        interfaces->engine->clientCmdUnrestricted(c_xor("play physics/metal/metal_solid_impact_bullet2"));
    else if (config->misc.hitSound == 3)
        interfaces->engine->clientCmdUnrestricted(c_xor("play buttons/arena_switch_press_02"));
}

void Misc::killSound(GameEvent& event) noexcept
{

}

void Misc::autoBuy(GameEvent* event) noexcept
{
    static const std::array<std::string, 17> primary = 
    {
    "",
    "mac10;buy mp9;",
    "mp7;",
    "ump45;",
    "p90;",
    "bizon;",
    "galilar;buy famas;",
    "ak47;buy m4a1;",
    "ssg08;",
    "sg556;buy aug;",
    "awp;",
    "g3sg1; buy scar20;",
    "nova;",
    "xm1014;",
    "sawedoff;buy mag7;",
    "m249; ",
    "negev;"
    };

    static const std::array<std::string, 6> secondary = 
    {
        "",
        "glock;buy hkp2000;",
        "elite;",
        "p250;",
        "tec9;buy fiveseven;",
        "deagle;buy revolver;"
    };

    static const std::array<std::string, 3> armor =
    {
        "",
        "vest;",
        "vesthelm;",
    };

    static const std::array<std::string, 2> utility =
    {
        "defuser;",
        "taser;"
    };

    static const std::array<std::string, 5> nades = 
    {
        "hegrenade;",
        "smokegrenade;",
        "molotov;buy incgrenade;",
        "flashbang;buy flashbang;",
        "decoy;"
    };

    if (!config->misc.autoBuy.enabled)
        return;

    std::string cmd = "";

    if (event) 
    {
        if (config->misc.autoBuy.primaryWeapon)
            cmd += "buy " + primary[config->misc.autoBuy.primaryWeapon];
        if (config->misc.autoBuy.secondaryWeapon)
            cmd += "buy " + secondary[config->misc.autoBuy.secondaryWeapon];
        if (config->misc.autoBuy.armor)
            cmd += "buy " + armor[config->misc.autoBuy.armor];

        for (size_t i = 0; i < nades.size(); i++)
        {
            if ((config->misc.autoBuy.grenades & 1 << i) == 1 << i)
                cmd += "buy " + nades[i];
        }

        for (size_t i = 0; i < utility.size(); i++)
        {
            if ((config->misc.autoBuy.utility & 1 << i) == 1 << i)
                cmd += "buy " + utility[i];
        }

        interfaces->engine->clientCmdUnrestricted(cmd.c_str());
    }
}

static std::vector<std::uint64_t> reportedPlayers;
static int reportbotRound;

void Misc::runReportbot() noexcept
{
    if (!config->misc.reportbot.enabled)
        return;

    if (!localPlayer)
        return;

    static auto lastReportTime = 0.0f;

    if (lastReportTime + config->misc.reportbot.delay > memory->globalVars->realtime)
        return;

    if (reportbotRound >= config->misc.reportbot.rounds)
        return;

    for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i)
    {
        const auto entity = interfaces->entityList->getEntity(i);

        if (!entity || entity == localPlayer.get())
            continue;

        if (config->misc.reportbot.target != 2 && (entity->isOtherEnemy(localPlayer.get()) ? config->misc.reportbot.target != 0 : config->misc.reportbot.target != 1))
            continue;

        PlayerInfo playerInfo;
        if (!interfaces->engine->getPlayerInfo(i, playerInfo))
            continue;

        if (playerInfo.fakeplayer || std::find(reportedPlayers.cbegin(), reportedPlayers.cend(), playerInfo.xuid) != reportedPlayers.cend())
            continue;

        std::string report;

        if (config->misc.reportbot.textAbuse)
            report += "textabuse,";
        if (config->misc.reportbot.griefing)
            report += "grief,";
        if (config->misc.reportbot.wallhack)
            report += "wallhack,";
        if (config->misc.reportbot.aimbot)
            report += "aimbot,";
        if (config->misc.reportbot.other)
            report += "speedhack,";

        if (!report.empty())
        {
            memory->submitReport(std::to_string(playerInfo.xuid).c_str(), report.c_str());
            lastReportTime = memory->globalVars->realtime;
            reportedPlayers.push_back(playerInfo.xuid);
        }

        return;
    }

    reportedPlayers.clear();
    ++reportbotRound;
}

void Misc::resetReportbot() noexcept
{
    reportbotRound = 0;
    reportedPlayers.clear();
}

void Misc::preserveKillfeed(bool roundStart) noexcept
{
    if (!config->misc.preserveKillfeed.enabled)
        return;

    static auto nextUpdate = 0.0f;

    if (roundStart) 
    {
        nextUpdate = memory->globalVars->realtime + 10.0f;
        return;
    }

    if (nextUpdate > memory->globalVars->realtime)
        return;

    nextUpdate = memory->globalVars->realtime + 2.0f;

    const auto deathNotice = std::uintptr_t(memory->findHudElement(memory->hud, skCrypt("CCSGO_HudDeathNotice")));
    if (!deathNotice)
        return;

    const auto deathNoticePanel = (*(UIPanel**)(*reinterpret_cast<std::uintptr_t*>(deathNotice - 20 + 88) + sizeof(std::uintptr_t)));
    const auto childPanelCount = deathNoticePanel->getChildCount();

    for (int i = 0; i < childPanelCount; ++i) 
    {
        const auto child = deathNoticePanel->getChild(i);
        if (!child)
            continue;

        if (child->hasClass(skCrypt("DeathNotice_Killer")) && (!config->misc.preserveKillfeed.onlyHeadshots || child->hasClass(skCrypt("DeathNoticeHeadShot"))))
            child->setAttributeFloat(skCrypt("SpawnTime"), memory->globalVars->currenttime);
    }
}

void Misc::drawOffscreenEnemies(ImDrawList* drawList) noexcept
{
    if (!config->misc.offscreenEnemies.enabled && !config->misc.offscreenAllies.enabled)
        return;

    const auto yaw = Helpers::deg2rad(interfaces->engine->getViewAngles().y);

    GameData::Lock lock;
    for (auto& player : GameData::players()) 
    {
        if (player.dormant || !player.alive || player.inViewFrustum)
            continue;

        Config::Misc::Offscreen pCfg = player.enemy ? config->misc.offscreenEnemies : config->misc.offscreenAllies;
        if (!pCfg.enabled)
            continue;

        const auto positionDiff = GameData::local().origin - player.origin;

        auto x = std::cos(yaw) * positionDiff.y - std::sin(yaw) * positionDiff.x;
        auto y = std::cos(yaw) * positionDiff.x + std::sin(yaw) * positionDiff.y;
        if (const auto len = std::sqrt(x * x + y * y); len != 0.0f) {
            x /= len;
            y /= len;
        }

        const auto& displaySize = ImGui::GetIO().DisplaySize;
        const auto pos = ImGui::GetIO().DisplaySize / 2 + ImVec2{ x, y } * pCfg.offset;
        const auto trianglePos = pos + ImVec2{ x, y };
        const auto color = Helpers::calculateColor(pCfg);

        const ImVec2 trianglePoints[]{
            trianglePos + ImVec2{  0.5f * y, -0.5f * x } * pCfg.size,
            trianglePos + ImVec2{  1.f * x,  1.f * y } * pCfg.size,
            trianglePos + ImVec2{ -0.5f * y,  0.5f * x } * pCfg.size,
        };

        drawList->AddConvexPolyFilled(trianglePoints, 3, color);
        if (pCfg.outline)
            drawList->AddPolyline(trianglePoints, 3, color | IM_COL32_A_MASK, ImDrawFlags_Closed, 1.5f);
    }
}

void Misc::autoAccept(const char* soundEntry) noexcept
{
    if (!config->misc.autoAccept)
        return;

    if (std::strcmp(soundEntry, skCrypt("UIPanorama.popup_accept_match_beep")))
        return;

    if (const auto idx = memory->registeredPanoramaEvents->find(memory->makePanoramaSymbol(skCrypt("MatchAssistedAccept"))); idx != -1) {
        if (const auto eventPtr = memory->registeredPanoramaEvents->memory[idx].value.makeEvent(nullptr))
            interfaces->panoramaUIEngine->accessUIEngine()->dispatchEvent(eventPtr);
    }

    auto window = FindWindowW(L"Valve001", NULL);
    FLASHWINFO flash{ sizeof(FLASHWINFO), window, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0 };
    FlashWindowEx(&flash);
    ShowWindow(window, SW_RESTORE);
}

void Misc::updateInput() noexcept
{
    config->misc.slowwalkKey.handleToggle();
    config->misc.fakeduckKey.handleToggle();
    config->misc.autoPeekKey.handleToggle();
}

void Misc::reset(int resetType) noexcept
{
    if (resetType == 1)
    {
        static auto ragdollGravity = interfaces->cvar->findVar(skCrypt("cl_ragdoll_gravity"));
        static auto blur = interfaces->cvar->findVar(skCrypt("@panorama_disable_blur"));
        ragdollGravity->setValue(600);
        blur->setValue(0);
    }
}