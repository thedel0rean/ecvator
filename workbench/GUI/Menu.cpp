#include "Menu.h"

auto s = ImVec2{}, p = ImVec2{}, gs = ImVec2{ 662, 460 };

static int tab;
static int enemy_tab;

//void legit()
//{
//    if (tab == 1)
//    {
//        ImGui::SetCursorPos(ImVec2(78, 130));
//        ImGui::MenuChild("tab 1", ImVec2(280, 325));
//        {
//            ImGui::Spacing();
//
//
//        }
//        ImGui::EndChild();
//
//        ImGui::SetCursorPos(ImVec2(370, 130));
//        ImGui::MenuChild("tab 2", ImVec2(280, 325));
//        {
//            ImGui::Spacing();
//
//
//        }
//        ImGui::EndChild();
//    }
//}
//
//void render_sub()
//{
//    //ImGui::SetCursorPos(ImVec2(90, 30));
//    //ImGui::Image(circle, ImVec2(40, 40));//cirlce
//
//    if (tab == 1)
//    {
//        //ImGui::GetWindowDrawList()->AddText(InterMedium, 16, ImVec2(p.x + 135, p.y + 25), ImColor(255, 255, 255), "Visual");//name tab
//        //ImGui::GetWindowDrawList()->AddText(IconFont, 25, ImVec2(p.x + 90, p.y + 29), ImColor(107, 107, 107), "B");//icon
//
//        ImGui::SetCursorPos(ImVec2{ 143, 50 });
//        if (ImGui::sub("Enemy", !enemy_tab)) enemy_tab = 0;
//
//        ImGui::SetCursorPos(ImVec2{ 190, 50 });
//        if (ImGui::sub("Ally", enemy_tab == 1)) enemy_tab = 1;
//
//        ImGui::SetCursorPos(ImVec2{ 220, 50 });
//        if (ImGui::sub("World", enemy_tab == 2)) enemy_tab = 2;
//    }
//}
//
//void render_tab()
//{
//    ImGui::PushFont(IconFont);
//    {
//        ImGui::SetCursorPos(ImVec2{ 26, 90 });
//        if (ImGui::tab("A", !tab)) tab = 0;
//
//        ImGui::SetCursorPos(ImVec2{ 26, 136 });
//        if (ImGui::tab("B", tab == 1)) tab = 1;
//
//        ImGui::SetCursorPos(ImVec2{ 26, 182 });
//        if (ImGui::tab("C", tab == 2)) tab = 2;
//
//        ImGui::SetCursorPos(ImVec2{ 26, 228 });
//        if (ImGui::tab("D", tab == 3)) tab = 3;
//
//        ImGui::SetCursorPos(ImVec2{ 26, 350 });
//        if (ImGui::tab("F", tab == 4)) tab = 4;
//
//    }
//    ImGui::PopFont();
//
//    switch (tab)
//    {
//    case 1:    
//        legit();      
//        break;
//    }
//}
//
//static char search[64] = "";
//void Search()
//{
//    auto p = ImGui::GetWindowPos();
//
//    ImGui::SetCursorPos(ImVec2(95, 85));
//    ImGui::PushFont(InterMedium);
//    {
//        ImGui::PushItemWidth(160);
//        if (ImGui::InputText("", search, 13))
//            g_Search.find = true;
//        ImGui::PopItemWidth();
//    }
//    ImGui::PopFont();
//
//    if (!ImGui::IsItemActive() && strlen(search) == 0)
//    {
//        ImGui::GetWindowDrawList()->AddText(InterMedium, 17, ImVec2(p.x + 100, p.y + 87), ImColor(83, 86, 93), "search");//search
//        ImGui::GetWindowDrawList()->AddText(IconFont, 17, ImVec2(p.x + 233, p.y + 88), ImColor(83, 86, 93), "V");//search
//    }
//}