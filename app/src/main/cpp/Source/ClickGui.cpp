#include "Modules/ClickGui.h"
#include "Globals.h"
#include "Imgui_Android_Input.h"

#include "Modules/ESP.h"
#include "Modules/Island.h"
#include "Modules/TargetHUD.h"
#include "Modules/ArrayList.h"
#include "Modules/Keyboard.h"
#include "Modules/FloatConfig.h"

// ---------------------------------------------------------
// 外部模块变量引用声明
// ---------------------------------------------------------
namespace ModuleESP {
    extern float esp_thickness, esp_w, esp_h, esp_hp;
    extern bool esp_chroma;
}
namespace ModuleIsland {
    extern float islandScale, islandOffsetX, islandOffsetY;
}
namespace ModuleTargetHUD {
    extern int th_mode, target_health;
    extern float th_scale, th_offsetX, th_offsetY, self_health;
    extern void SimulateHit();
}
namespace ModuleArrayList {
    extern bool al_no_bg, al_no_line, al_glow_enable;
    extern float al_glow_alpha, al_glow_intensity, al_scale, al_offsetX, al_offsetY, al_spacing;
    extern float al_line_width, al_line_height_mult, al_line_offsetX;
}
namespace ModuleKeyboard {
    extern float vk_global_scale, vk_btn_size, vk_text_size;
}
namespace ModuleFloatConfig {
    extern bool fb_reset_pos, fb_chroma;
    extern float fb_scale, fb_font_size, fb_rounding, fb_text_alpha_on, fb_text_alpha_off;
    extern float fb_bg_alpha;
}

namespace ModuleClickGui {
    float clickgui_scale = 1.0f;
    static float anim_clickgui_scale = 1.0f;
    bool show_cg_confirm = false;
    static float cg_confirm_anim = 0.0f, cg_confirm_vel = 0.0f;

    enum GuiState { STATE_CATEGORIES, STATE_MODULES, STATE_SETTINGS };
    static GuiState currentState = STATE_CATEGORIES, nextState = STATE_CATEGORIES;
    static float viewAlpha = 1.0f; 
    static bool isTransitioning = false;      
    static std::string currentCategory = "", currentModule = "";    

    static ImVec2 guiBasePos = ImVec2(50, 100);
    static float currentGuiWidth = 250.0f, currentGuiHeight = 400.0f, targetGuiWidth = 250.0f, targetGuiHeight = 400.0f;
    static float savedExpandedWidth = 450.0f, savedExpandedHeight = 550.0f;
    static bool isDraggingWindow = false, isDraggingResizer = false; 

    struct ModItem { 
        const char* label; bool* state; std::string modName; int* key; 
        ModItem(const char* l, bool* s, const char* n, int* k) { label = l; state = s; modName = std::string(n); key = k; } 
    };

    struct ThemePreset { const char* name; int c1; int c2; };
    static const ThemePreset presetData[] = {
        {"Opal", 0x2DBFFE, 0x2499CB}, {"Spearmint", 0x61C2A2, 0x41826C}, {"Jade Green", 0x00A86B, 0x006942}, {"Green Spirit", 0x9FE2BF, 0x00873E},
        {"Rosy Pink", 0xFF66CC, 0xBF4D99}, {"Magenta", 0xD53F77, 0x9D446E}, {"Hot Pink", 0xE75480, 0xAC4FC6}, {"Lavender", 0xDBA6F7, 0x9873AC},
        {"Amethyst", 0x9063CD, 0x62438C}, {"Purple Fire", 0xB1A2CA, 0x68478D}, {"Sunset Pink", 0xFF9114, 0xF569E7}, {"Blaze Orange", 0xFFA94D, 0xFF8200},
        {"Pink Blood", 0xFFA6C9, 0xE40046}, {"Pastel", 0xFF6D6A, 0xBF5250}, {"Neon Red", 0xD22730, 0xB8192A}, {"Red Coffee", 0xE1223B, 0x4B1313},
        {"Deep Ocean", 0x3C5291, 0x001440}, {"Chambray Blue", 0x3C5291, 0x212EB6}, {"Mint Blue", 0x429E9D, 0x285E5D}, {"Pacific Blue", 0x05A9C7, 0x047387},
        {"Tropical Ice", 0x66FFD1, 0x0695FF}, {"Blue Purple", 0x684DB2, 0x043CAE}
    };
    const int presetCount = sizeof(presetData) / sizeof(presetData[0]);
    static const char* themeNames[22]; static bool themeInit = false;
    static int currentThemeIndex = 0;

    void Render() {
        if (!themeInit) { for(int i = 0; i < presetCount; i++) themeNames[i] = presetData[i].name; themeInit = true; }
        
        auto HexToV4 = [](int hex) -> ImVec4 { return ImVec4(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f, 1.0f); };
        Globals::themeColors[0] = HexToV4(presetData[currentThemeIndex].c1); 
        Globals::themeColors[1] = HexToV4(presetData[currentThemeIndex].c2);

        bool& al_show_clickgui = *Globals::al_mods[0].show_in_al; bool& show_fb_clickgui = *Globals::al_mods[0].show_fb; int& key_clickgui = *Globals::al_mods[0].key;
        bool& al_show_esp      = *Globals::al_mods[1].show_in_al; bool& show_fb_esp      = *Globals::al_mods[1].show_fb; int& key_esp      = *Globals::al_mods[1].key;
        bool& al_show_island   = *Globals::al_mods[2].show_in_al; bool& show_fb_island   = *Globals::al_mods[2].show_fb; int& key_island   = *Globals::al_mods[2].key;
        bool& al_show_interface= *Globals::al_mods[3].show_in_al; bool& show_fb_interface= *Globals::al_mods[3].show_fb; int& key_interface= *Globals::al_mods[3].key;
        bool& al_show_targethud= *Globals::al_mods[4].show_in_al; bool& show_fb_targethud= *Globals::al_mods[4].show_fb; int& key_targethud= *Globals::al_mods[4].key;
        bool& al_show_scaffold = *Globals::al_mods[5].show_in_al; bool& show_fb_scaffold = *Globals::al_mods[5].show_fb; int& key_scaffold = *Globals::al_mods[5].key;
        bool& al_show_arraylist= *Globals::al_mods[6].show_in_al; bool& show_fb_arraylist= *Globals::al_mods[6].show_fb; int& key_arraylist= *Globals::al_mods[6].key;

        anim_clickgui_scale += (clickgui_scale - anim_clickgui_scale) * 10.0f * Globals::io->DeltaTime; 
        const float BASE_UI_SCALE = 1.3f; float scaled_UI = BASE_UI_SCALE * anim_clickgui_scale; 

        if (show_cg_confirm) { Globals::UpdateSpring(cg_confirm_anim, cg_confirm_vel, 1.0f, 18.0f, 0.6f, Globals::io->DeltaTime); } 
        else { cg_confirm_anim -= Globals::io->DeltaTime * 10.0f; if (cg_confirm_anim < 0.0f) cg_confirm_anim = 0.0f; }

        if (Globals::showClickGui) { Globals::globalGuiAlpha += Globals::io->DeltaTime * 5.0f; if (Globals::globalGuiAlpha > 1.0f) Globals::globalGuiAlpha = 1.0f; }
        else { Globals::globalGuiAlpha -= Globals::io->DeltaTime * 5.0f; if (Globals::globalGuiAlpha < 0.0f) Globals::globalGuiAlpha = 0.0f; }

        if (Globals::globalGuiAlpha < 0.001f && cg_confirm_anim < 0.001f) return;

        ImFont* font = Globals::font;
        auto tr = [&](const char* en, const char* cn) { return Globals::clickguiLanguage == 0 ? en : cn; };
        auto getCatCn = [&](const std::string& en) -> std::string {
            if (en == "Render") return "渲染"; if (en == "Combat") return "战斗"; if (en == "Player") return "玩家"; if (en == "Misc") return "杂项"; if (en == "World") return "世界"; if (en == "Visual") return "视觉"; if (en == "Other") return "其他"; return en;
        };
        auto getModCn = [&](const std::string& en) -> std::string {
            if (en == "ESP") return "透视"; if (en == "ClickGui") return "菜单显示"; if (en == "Island") return "灵动岛"; if (en == "Interface") return "菜单"; if (en == "Targethud") return "目标信息"; if (en == "Scaffold") return "脚手架"; if(en == "ArrayList") return "列表"; if (en == "FloatConfig") return "悬浮窗"; if (en == "Keyboard") return "虚拟键盘"; return en;
        };
        auto DrawNativeShadowRect = [&](ImDrawList* draw_list, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, float shadow_size, float shadow_alpha, const ImVec2& shadow_offset, float alpha_mult) {
            if (draw_list == nullptr || shadow_size <= 0.0f || shadow_alpha <= 0.0f || alpha_mult <= 0.0f) return;
            float final_alpha = CLAMP_F(shadow_alpha * alpha_mult, 0.0f, 1.0f);
            draw_list->AddShadowRect(
                rect_min,
                rect_max,
                IM_COL32(0, 0, 0, (int)(final_alpha * 255.0f)),
                shadow_size,
                shadow_offset,
                0,
                rounding
            );
        };

        // ---------------------------------------------------------
        // UI 自绘控件 Lambda 库
        // ---------------------------------------------------------
        auto AnimatedHoverText = [&](const char* label, float scale) -> bool {
            float currentTextSize = ImGui::GetFontSize() * scale * scaled_UI; 
            ImVec2 label_size = font->CalcTextSizeA(currentTextSize, 10000.0f, 0.0f, label); 
            ImVec2 size(ImGui::GetContentRegionAvail().x, label_size.y + 6.0f * scaled_UI); 
            ImVec2 target_pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(label, size); bool hovered = ImGui::IsItemHovered(); bool pressed = hovered && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 5.0f);
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float offset = storage->GetFloat(id, 0.0f); float targetOffset = hovered ? 15.0f * scaled_UI : 0.0f; offset += (targetOffset - offset) * 15.0f * Globals::io->DeltaTime; storage->SetFloat(id, offset);
            float cur_x = storage->GetFloat(id + 1, target_pos.x), cur_y = storage->GetFloat(id + 2, target_pos.y);
            if (ABS_F(cur_x - target_pos.x) > 500.0f || ABS_F(cur_y - target_pos.y) > 500.0f) { cur_x = target_pos.x; cur_y = target_pos.y; }
            cur_x += (target_pos.x - cur_x) * 20.0f * Globals::io->DeltaTime; cur_y += (target_pos.y - cur_y) * 20.0f * Globals::io->DeltaTime; storage->SetFloat(id + 1, cur_x); storage->SetFloat(id + 2, cur_y);
            ImGui::GetWindowDrawList()->AddText(font, currentTextSize, ImVec2(cur_x + offset, cur_y + 3.0f * scaled_UI), IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)), label); return pressed;
        };

        auto CustomButton = [&](const char* label, float overrideWidth) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float width = overrideWidth > 0.0f ? overrideWidth : (label_size.x + 30.0f * scaled_UI); ImVec2 size(width, label_size.y + 16.0f * scaled_UI); ImVec2 target_pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(label, size); bool hovered = ImGui::IsItemHovered(); bool pressed = hovered && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 5.0f);
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float hover_t = storage->GetFloat(id, 0.0f); hover_t += ((hovered ? 1.0f : 0.0f) - hover_t) * 15.0f * Globals::io->DeltaTime; storage->SetFloat(id, hover_t);
            float cur_x = storage->GetFloat(id + 1, target_pos.x), cur_y = storage->GetFloat(id + 2, target_pos.y);
            float cur_w = storage->GetFloat(id + 10, size.x), cur_h = storage->GetFloat(id + 11, size.y);
            if (ABS_F(cur_x - target_pos.x) > 500.0f || ABS_F(cur_y - target_pos.y) > 500.0f) { cur_x = target_pos.x; cur_y = target_pos.y; } if (ABS_F(cur_w - size.x) > 500.0f || ABS_F(cur_h - size.y) > 500.0f) { cur_w = size.x; cur_h = size.y; }
            cur_x += (target_pos.x - cur_x) * 20.0f * Globals::io->DeltaTime; cur_y += (target_pos.y - cur_y) * 20.0f * Globals::io->DeltaTime; cur_w += (size.x - cur_w) * 20.0f * Globals::io->DeltaTime; cur_h += (size.y - cur_h) * 20.0f * Globals::io->DeltaTime;
            storage->SetFloat(id + 1, cur_x); storage->SetFloat(id + 2, cur_y); storage->SetFloat(id + 10, cur_w); storage->SetFloat(id + 11, cur_h);
            ImVec2 draw_pos(cur_x, cur_y), draw_size(cur_w, cur_h); ImVec4 base_bg = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            ImVec4 bg_color = ImVec4(base_bg.x + hover_t * 0.25f, base_bg.y + hover_t * 0.25f, base_bg.z + hover_t * 0.25f, Globals::currentUIAlpha);
            ImGui::GetWindowDrawList()->AddRectFilled(draw_pos, ImVec2(draw_pos.x + draw_size.x, draw_pos.y + draw_size.y), ImGui::ColorConvertFloat4ToU32(bg_color), Globals::clickguiRounding);
            ImVec2 text_pos(draw_pos.x + (draw_size.x - label_size.x) * 0.5f, draw_pos.y + (draw_size.y - label_size.y) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, text_pos, IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)), label); return pressed;
        };

        auto CustomKeybind = [&](const char* label, int* key_ref) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float w = ImGui::GetContentRegionAvail().x; ImVec2 pos = ImGui::GetCursorScreenPos(); ImVec2 size(w, label_size.y + 16.0f * scaled_UI);
            ImGui::InvisibleButton(label, size);
            bool hovered = ImGui::IsItemHovered(); bool pressed = hovered && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 5.0f);
            if (pressed) { if (Globals::active_bind_target == key_ref) Globals::active_bind_target = nullptr; else Globals::active_bind_target = key_ref; }
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float hover_t = storage->GetFloat(id, 0.0f); hover_t += ((hovered ? 1.0f : 0.0f) - hover_t) * 15.0f * Globals::io->DeltaTime; storage->SetFloat(id, hover_t);
            float cur_x = storage->GetFloat(id + 11, pos.x), cur_y = storage->GetFloat(id + 12, pos.y); float cur_w = storage->GetFloat(id + 10, w);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; } if (ABS_F(cur_w - w) > 500.0f) cur_w = w;
            cur_x += (pos.x - cur_x) * 20.0f * Globals::io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * Globals::io->DeltaTime; cur_w += (w - cur_w) * 20.0f * Globals::io->DeltaTime;
            storage->SetFloat(id + 11, cur_x); storage->SetFloat(id + 12, cur_y); storage->SetFloat(id + 10, cur_w);
            ImVec2 draw_pos(cur_x, cur_y); ImVec4 base_bg = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
            ImVec4 bg_color = ImVec4(base_bg.x + hover_t * 0.15f, base_bg.y + hover_t * 0.15f, base_bg.z + hover_t * 0.15f, Globals::currentUIAlpha);
            ImGui::GetWindowDrawList()->AddRectFilled(draw_pos, ImVec2(draw_pos.x + cur_w, draw_pos.y + size.y), ImGui::ColorConvertFloat4ToU32(bg_color), Globals::clickguiRounding);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + 10.0f * scaled_UI, draw_pos.y + 8.0f * scaled_UI), IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)), label);
            std::string bind_str;
            if (Globals::active_bind_target == key_ref) { bind_str = "Wait VK..."; } else if (*key_ref == 0) { bind_str = "None"; } else { const char* n = ImGui::GetKeyName((ImGuiKey)*key_ref); bind_str = n ? n : "Unknown"; }
            ImVec2 val_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, bind_str.c_str());
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + cur_w - val_size.x - 10.0f * scaled_UI, draw_pos.y + 8.0f * scaled_UI), IM_COL32(200, 200, 200, (int)(Globals::currentUIAlpha * 255)), bind_str.c_str());
            return pressed;
        };

        auto ModuleButton = [&](const char* label, bool* v, bool* settingsClicked, const char* modNameLog) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            ImVec2 size(label_size.x + 45.0f * scaled_UI, label_size.y + 20.0f * scaled_UI); ImVec2 target_pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(label, size); bool hovered = ImGui::IsItemHovered(); bool pressed = hovered && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 5.0f);
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float hover_t = storage->GetFloat(id, 0.0f); hover_t += ((hovered ? 1.0f : 0.0f) - hover_t) * 15.0f * Globals::io->DeltaTime; storage->SetFloat(id, hover_t);
            float cur_x = storage->GetFloat(id + 1, target_pos.x), cur_y = storage->GetFloat(id + 2, target_pos.y);
            float cur_w = storage->GetFloat(id + 10, size.x), cur_h = storage->GetFloat(id + 11, size.y);
            if (ABS_F(cur_x - target_pos.x) > 500.0f || ABS_F(cur_y - target_pos.y) > 500.0f) { cur_x = target_pos.x; cur_y = target_pos.y; } if (ABS_F(cur_w - size.x) > 500.0f || ABS_F(cur_h - size.y) > 500.0f) { cur_w = size.x; cur_h = size.y; }
            cur_x += (target_pos.x - cur_x) * 20.0f * Globals::io->DeltaTime; cur_y += (target_pos.y - cur_y) * 20.0f * Globals::io->DeltaTime; cur_w += (size.x - cur_w) * 20.0f * Globals::io->DeltaTime; cur_h += (size.y - cur_h) * 20.0f * Globals::io->DeltaTime;
            storage->SetFloat(id + 1, cur_x); storage->SetFloat(id + 2, cur_y); storage->SetFloat(id + 10, cur_w); storage->SetFloat(id + 11, cur_h);
            ImVec2 draw_pos(cur_x, cur_y), draw_size(cur_w, cur_h);
            ImVec4 activeColor = ImVec4(Globals::themeColors[0].x, Globals::themeColors[0].y, Globals::themeColors[0].z, 1.0f);
            ImVec4 base_bg = *v ? activeColor : ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            ImVec4 bg_color = ImVec4(base_bg.x + hover_t * 0.25f, base_bg.y + hover_t * 0.25f, base_bg.z + hover_t * 0.25f, Globals::currentUIAlpha);
            ImGui::GetWindowDrawList()->AddRectFilled(draw_pos, ImVec2(draw_pos.x + draw_size.x, draw_pos.y + draw_size.y), ImGui::ColorConvertFloat4ToU32(bg_color), Globals::clickguiRounding);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + 15.0f * scaled_UI, draw_pos.y + (draw_size.y - label_size.y) * 0.5f), IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)), label);
            ImVec2 arrow_min(target_pos.x + size.x - 22.0f * scaled_UI, target_pos.y), arrow_max(target_pos.x + size.x, target_pos.y + size.y); bool arrow_hovered = (Globals::io->MousePos.x >= arrow_min.x && Globals::io->MousePos.x <= arrow_max.x && Globals::io->MousePos.y >= arrow_min.y && Globals::io->MousePos.y <= arrow_max.y);
            if (pressed) { if (arrow_hovered) *settingsClicked = true; else { *v = !*v; if (std::string(modNameLog) != "Scaffold" && std::string(modNameLog) != "ClickGui") Globals::TriggerNotification(modNameLog, *v); } }
            float cx = draw_pos.x + draw_size.x - 13.0f * scaled_UI, cy = draw_pos.y + draw_size.y * 0.5f;
            ImVec2 p1(cx - 3.0f * scaled_UI, cy - 4.0f * scaled_UI), p2(cx + 3.0f * scaled_UI, cy), p3(cx - 3.0f * scaled_UI, cy + 4.0f * scaled_UI);
            ImU32 arrowCol = arrow_hovered ? IM_COL32(200, 200, 255, (int)(Globals::currentUIAlpha * 255)) : IM_COL32(150, 150, 150, (int)(Globals::currentUIAlpha * 255));
            ImGui::GetWindowDrawList()->AddLine(p1, p2, arrowCol, 2.0f * scaled_UI); ImGui::GetWindowDrawList()->AddLine(p2, p3, arrowCol, 2.0f * scaled_UI); return pressed;
        };

        auto CustomCombo = [&](const char* label, int* current_item, const char* const items[], int items_count) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            ImVec2 val_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, items[*current_item]); ImVec2 size(ImGui::GetContentRegionAvail().x, MAX_F(label_size.y, val_size.y) + 20.0f * scaled_UI); ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(label, size); bool hovered = ImGui::IsItemHovered(); bool pressed = hovered && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 5.0f);
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            if (pressed) *current_item = (*current_item + 1) % items_count;
            int prev_idx = storage->GetInt(id + 20, *current_item);
            if (prev_idx != *current_item) { storage->SetInt(id + 22, prev_idx); storage->SetInt(id + 20, *current_item); storage->SetFloat(id + 21, 0.0f); }
            float text_anim_t = storage->GetFloat(id + 21, 1.0f); if (text_anim_t < 1.0f) { text_anim_t += 10.0f * Globals::io->DeltaTime; if (text_anim_t > 1.0f) text_anim_t = 1.0f; } storage->SetFloat(id + 21, text_anim_t);
            int fade_out_idx = storage->GetInt(id + 22, *current_item);
            float hover_t = storage->GetFloat(id, 0.0f); hover_t += ((hovered ? 1.0f : 0.0f) - hover_t) * 15.0f * Globals::io->DeltaTime; storage->SetFloat(id, hover_t);
            float cur_x = storage->GetFloat(id + 1, pos.x), cur_y = storage->GetFloat(id + 2, pos.y);
            float cur_w = storage->GetFloat(id + 10, size.x), cur_h = storage->GetFloat(id + 11, size.y);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; } if (ABS_F(cur_w - size.x) > 500.0f || ABS_F(cur_h - size.y) > 500.0f) { cur_w = size.x; cur_h = size.y; }
            cur_x += (pos.x - cur_x) * 20.0f * Globals::io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * Globals::io->DeltaTime; cur_w += (size.x - cur_w) * 20.0f * Globals::io->DeltaTime; cur_h += (size.y - cur_h) * 20.0f * Globals::io->DeltaTime;
            storage->SetFloat(id + 1, cur_x); storage->SetFloat(id + 2, cur_y); storage->SetFloat(id + 10, cur_w); storage->SetFloat(id + 11, cur_h);
            ImVec4 base_bg = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); ImVec4 bg_color = ImVec4(base_bg.x + hover_t * 0.25f, base_bg.y + hover_t * 0.25f, base_bg.z + hover_t * 0.25f, Globals::currentUIAlpha);
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(cur_x, cur_y), ImVec2(cur_x + cur_w, cur_y + cur_h), ImGui::ColorConvertFloat4ToU32(bg_color), Globals::clickguiRounding);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(cur_x + 15.0f * scaled_UI, cur_y + (cur_h - label_size.y) * 0.5f), IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)), label);
            ImGui::GetWindowDrawList()->PushClipRect(ImVec2(cur_x, cur_y), ImVec2(cur_x + cur_w, cur_y + cur_h), true);
            if (text_anim_t < 1.0f) {
                ImVec2 fade_val_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, items[fade_out_idx]);
                float out_y = cur_y + (cur_h - fade_val_size.y) * 0.5f + (text_anim_t * 15.0f * scaled_UI);
                ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(cur_x + cur_w - fade_val_size.x - 15.0f * scaled_UI, out_y), IM_COL32(200, 200, 200, (int)(Globals::currentUIAlpha * 255.0f * (1.0f - text_anim_t))), items[fade_out_idx]);
            }
            float in_y = cur_y + (cur_h - val_size.y) * 0.5f - ((1.0f - text_anim_t) * 15.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(cur_x + cur_w - val_size.x - 15.0f * scaled_UI, in_y), IM_COL32(200, 200, 200, (int)(Globals::currentUIAlpha * 255.0f * text_anim_t)), items[*current_item]);
            ImGui::GetWindowDrawList()->PopClipRect(); return pressed;
        };

        auto CustomSliderFloat = [&](const char* label, float* v, float v_min, float v_max, const char* fmt = "%.2f") -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float w = ImGui::GetContentRegionAvail().x; ImVec2 pos = ImGui::GetCursorScreenPos(); ImVec2 size(w, label_size.y + 16.0f * scaled_UI); 
            ImGui::InvisibleButton(label, size); bool active = ImGui::IsItemActive(); ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float cur_x = storage->GetFloat(id + 11, pos.x), cur_y = storage->GetFloat(id + 12, pos.y); float cur_w = storage->GetFloat(id + 10, w);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; } if (ABS_F(cur_w - w) > 500.0f) cur_w = w;
            cur_x += (pos.x - cur_x) * 20.0f * Globals::io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * Globals::io->DeltaTime; cur_w += (w - cur_w) * 20.0f * Globals::io->DeltaTime;
            storage->SetFloat(id + 11, cur_x); storage->SetFloat(id + 12, cur_y); storage->SetFloat(id + 10, cur_w);
            ImVec2 draw_pos(cur_x, cur_y), track_min(draw_pos.x, draw_pos.y + label_size.y + 6.0f * scaled_UI), track_max(draw_pos.x + cur_w, draw_pos.y + label_size.y + 10.0f * scaled_UI);
            if (active) { float mouse_x = Globals::io->MousePos.x; float new_t = CLAMP_F((mouse_x - track_min.x) / cur_w, 0.0f, 1.0f); *v = v_min + new_t * (v_max - v_min); }
            float target_t = (*v - v_min) / (v_max - v_min); float current_t = storage->GetFloat(id, target_t); current_t += (target_t - current_t) * 15.0f * Globals::io->DeltaTime; storage->SetFloat(id, current_t);
            char val_buf[32]; snprintf(val_buf, sizeof(val_buf), fmt, *v); ImVec2 val_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, val_buf);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, draw_pos, IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)), label);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + cur_w - val_size.x, draw_pos.y), IM_COL32(200, 200, 200, (int)(Globals::currentUIAlpha * 255)), val_buf);
            ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::themeColors[0].x, Globals::themeColors[0].y, Globals::themeColors[0].z, Globals::currentUIAlpha));
            ImGui::GetWindowDrawList()->AddRectFilled(track_min, track_max, IM_COL32(50, 50, 50, (int)(Globals::currentUIAlpha * 255)), 2.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddRectFilled(track_min, ImVec2(track_min.x + current_t * cur_w, track_max.y), fillCol, 2.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(track_min.x + current_t * cur_w, track_min.y + 2.0f * scaled_UI), 6.0f * scaled_UI, IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)));
            return active;
        };
        
        auto CustomSliderInt = [&](const char* label, int* v, int v_min, int v_max) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float w = ImGui::GetContentRegionAvail().x; ImVec2 pos = ImGui::GetCursorScreenPos(); ImVec2 size(w, label_size.y + 16.0f * scaled_UI); 
            ImGui::InvisibleButton(label, size); bool active = ImGui::IsItemActive(); ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float cur_x = storage->GetFloat(id + 11, pos.x), cur_y = storage->GetFloat(id + 12, pos.y); float cur_w = storage->GetFloat(id + 10, w);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; } if (ABS_F(cur_w - w) > 500.0f) cur_w = w;
            cur_x += (pos.x - cur_x) * 20.0f * Globals::io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * Globals::io->DeltaTime; cur_w += (w - cur_w) * 20.0f * Globals::io->DeltaTime;
            storage->SetFloat(id + 11, cur_x); storage->SetFloat(id + 12, cur_y); storage->SetFloat(id + 10, cur_w);
            ImVec2 draw_pos(cur_x, cur_y), track_min(draw_pos.x, draw_pos.y + label_size.y + 6.0f * scaled_UI), track_max(draw_pos.x + cur_w, draw_pos.y + label_size.y + 10.0f * scaled_UI);
            if (active) { float mouse_x = Globals::io->MousePos.x; float new_t = CLAMP_F((mouse_x - track_min.x) / cur_w, 0.0f, 1.0f); *v = (int)CLAMP_F(v_min + new_t * (v_max - v_min) + 0.5f, (float)v_min, (float)v_max); }
            float target_t = (float)(*v - v_min) / (v_max - v_min); float current_t = storage->GetFloat(id, target_t); current_t += (target_t - current_t) * 15.0f * Globals::io->DeltaTime; storage->SetFloat(id, current_t);
            char val_buf[32]; snprintf(val_buf, sizeof(val_buf), "%d", *v); ImVec2 val_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, val_buf);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, draw_pos, IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)), label);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + cur_w - val_size.x, draw_pos.y), IM_COL32(200, 200, 200, (int)(Globals::currentUIAlpha * 255)), val_buf);
            ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(Globals::themeColors[0].x, Globals::themeColors[0].y, Globals::themeColors[0].z, Globals::currentUIAlpha));
            ImGui::GetWindowDrawList()->AddRectFilled(track_min, track_max, IM_COL32(50, 50, 50, (int)(Globals::currentUIAlpha * 255)), 2.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddRectFilled(track_min, ImVec2(track_min.x + current_t * cur_w, track_max.y), fillCol, 2.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(track_min.x + current_t * cur_w, track_min.y + 2.0f * scaled_UI), 6.0f * scaled_UI, IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)));
            return active;
        };

        auto CustomInputText = [&](const char* label, char* buf, size_t buf_size) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float w = ImGui::GetContentRegionAvail().x; ImVec2 pos = ImGui::GetCursorScreenPos(); ImVec2 size(w, label_size.y + 35.0f * scaled_UI); 
            ImGui::InvisibleButton(label, size);
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float cur_x = storage->GetFloat(id + 11, pos.x), cur_y = storage->GetFloat(id + 12, pos.y); float cur_w = storage->GetFloat(id + 10, w);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; } if (ABS_F(cur_w - w) > 500.0f) cur_w = w;
            cur_x += (pos.x - cur_x) * 20.0f * Globals::io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * Globals::io->DeltaTime; cur_w += (w - cur_w) * 20.0f * Globals::io->DeltaTime;
            storage->SetFloat(id + 11, cur_x); storage->SetFloat(id + 12, cur_y); storage->SetFloat(id + 10, cur_w);
            ImVec2 draw_pos(cur_x, cur_y);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, draw_pos, IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255)), label);
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x, draw_pos.y + label_size.y + 5.0f * scaled_UI));
            ImGui::PushItemWidth(cur_w); ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Globals::clickguiRounding);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.1f, Globals::currentUIAlpha)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,Globals::currentUIAlpha));
            char hidden_label[128]; snprintf(hidden_label, sizeof(hidden_label), "##%s", label);
            bool ret = ImGui::InputText(hidden_label, buf, buf_size);
            if (ImGui::IsItemClicked() && Globals::input) { Globals::input->openInput(); }
            ImGui::PopStyleColor(2); ImGui::PopStyleVar(); ImGui::PopItemWidth(); return ret;
        };

        // =========================================================
        // 主界面逻辑渲染层
        // =========================================================
        Globals::currentUIAlpha = 1.0f * Globals::globalGuiAlpha;

        if (isTransitioning) { viewAlpha -= Globals::io->DeltaTime * 10.0f; if (viewAlpha <= 0.0f) { viewAlpha = 0.0f; currentState = nextState; isTransitioning = false; } } 
        else { if (viewAlpha < 1.0f) { viewAlpha += Globals::io->DeltaTime * 10.0f; if (viewAlpha > 1.0f) viewAlpha = 1.0f; } }

        if (!isDraggingResizer) {
            if (currentState == STATE_CATEGORIES) { 
                float item_h = font->CalcTextSizeA(ImGui::GetFontSize() * 0.75f * scaled_UI, 10000.0f, 0.0f, "A").y + 6.0f * scaled_UI;
                targetGuiWidth = 250.0f * scaled_UI; targetGuiHeight = 110.0f * scaled_UI + (item_h * 7.0f) + 60.0f * scaled_UI; 
            } else { targetGuiWidth = savedExpandedWidth; targetGuiHeight = savedExpandedHeight; }
        }
        if (isDraggingResizer) { currentGuiWidth = targetGuiWidth; currentGuiHeight = targetGuiHeight; } 
        else { currentGuiWidth += (targetGuiWidth - currentGuiWidth) * 15.0f * Globals::io->DeltaTime; currentGuiHeight += (targetGuiHeight - currentGuiHeight) * 15.0f * Globals::io->DeltaTime; }

        ImGui::SetNextWindowPos(guiBasePos, ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(currentGuiWidth, currentGuiHeight), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); 
        
        ImGui::Begin("Fate_ClickGui", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        if (Globals::input != nullptr) { Globals::input->g_window = ImGui::GetCurrentWindow(); }
        ImDrawList* guiDraw = ImGui::GetWindowDrawList(); ImVec2 guiPos = ImGui::GetWindowPos();

        // 绘制阴影（改为 ImGui 原生 AddShadowRect）
        guiDraw->PushClipRect(ImVec2(guiPos.x - 500.0f, guiPos.y - 500.0f), ImVec2(guiPos.x + currentGuiWidth + 500.0f, guiPos.y + currentGuiHeight + 500.0f), false);
        DrawNativeShadowRect(
            guiDraw,
            guiPos,
            ImVec2(guiPos.x + currentGuiWidth, guiPos.y + currentGuiHeight),
            Globals::clickguiRounding,
            Globals::shadowSize,
            Globals::shadowAlpha,
            ImVec2(0.0f, 0.0f),
            Globals::globalGuiAlpha
        );
        guiDraw->PopClipRect();
        guiDraw->AddRectFilled(guiPos, ImVec2(guiPos.x + currentGuiWidth, guiPos.y + currentGuiHeight), IM_COL32(0, 0, 0, (int)(Globals::bgAlpha * Globals::globalGuiAlpha * 255.0f)), Globals::clickguiRounding);

        // 窗口拖拽逻辑
        static ImVec2 dragOffset; ImVec2 titleBar_min = guiPos, titleBar_max = ImVec2(guiPos.x + currentGuiWidth, guiPos.y + 70.0f * scaled_UI);
        ImVec2 mousePos = Globals::io->MousePos; bool title_hovered = (mousePos.x >= titleBar_min.x && mousePos.x <= titleBar_max.x && mousePos.y >= titleBar_min.y && mousePos.y <= titleBar_max.y);
        if (title_hovered && ImGui::IsMouseClicked(0)) { isDraggingWindow = true; dragOffset = ImVec2(mousePos.x - guiBasePos.x, mousePos.y - guiBasePos.y); }
        if (!ImGui::IsMouseDown(0)) isDraggingWindow = false;
        if (isDraggingWindow) { guiBasePos.x = mousePos.x - dragOffset.x; guiBasePos.y = mousePos.y - dragOffset.y; }

        Globals::currentUIAlpha = 1.0f * Globals::globalGuiAlpha;
        if (currentState == STATE_CATEGORIES) {
            float scaledTitleSize = ImGui::GetFontSize() * 1.75f * scaled_UI; 
            ImVec2 fateSize = font->CalcTextSizeA(scaledTitleSize, 10000.0f, 0.0f, Globals::clientName);
            // 应用 ClientName XY 调节
            guiDraw->AddText(font, scaledTitleSize, 
                ImVec2(guiPos.x + (currentGuiWidth - fateSize.x) * 0.5f + Globals::clientNameOffsetX, 
                       guiPos.y + 15.0f * scaled_UI + Globals::clientNameOffsetY), 
                IM_COL32(255, 255, 255, (int)(Globals::currentUIAlpha * 255.0f)), Globals::clientName);
        } else {
            float dirTextSize = ImGui::GetFontSize() * 1.2f * scaled_UI; std::string dirStr;
            if (currentState == STATE_MODULES) { dirStr = Globals::clickguiLanguage == 0 ? currentCategory : getCatCn(currentCategory); } 
            else if (currentState == STATE_SETTINGS) { dirStr = std::string(Globals::clickguiLanguage == 0 ? currentCategory : getCatCn(currentCategory)) + " / " + (Globals::clickguiLanguage == 0 ? currentModule : getModCn(currentModule)); }
            guiDraw->AddText(font, dirTextSize, ImVec2(guiPos.x + 20.0f * scaled_UI, guiPos.y + 25.0f * scaled_UI), IM_COL32(230, 230, 230, (int)(Globals::currentUIAlpha * 255.0f)), dirStr.c_str());
            const char* backLabel = tr("< Back", "< 返回"); float backBtnW = font->CalcTextSizeA(ImGui::GetFontSize() * scaled_UI, 10000.0f, 0.0f, backLabel).x + 30.0f * scaled_UI;
            ImGui::SetCursorPos(ImVec2(currentGuiWidth - backBtnW - 10.0f * scaled_UI, 15.0f * scaled_UI)); 
            if (CustomButton(backLabel, 0.0f)) { if (currentState == STATE_MODULES) nextState = STATE_CATEGORIES; else if (currentState == STATE_SETTINGS) nextState = STATE_MODULES; isTransitioning = true; }
        }
        guiDraw->AddLine(ImVec2(guiPos.x + 20.0f * scaled_UI, guiPos.y + 70.0f * scaled_UI), ImVec2(guiPos.x + currentGuiWidth - 20.0f * scaled_UI, guiPos.y + 70.0f * scaled_UI), IM_COL32(100, 100, 100, (int)(Globals::bgAlpha * Globals::globalGuiAlpha * 150.0f)), 1.0f);

        Globals::currentUIAlpha = Globals::globalGuiAlpha * viewAlpha; 
        // 应用 Category XY 调节
        ImGui::SetCursorPos(ImVec2(20.0f * scaled_UI + Globals::categoryOffsetX, 80.0f * scaled_UI + Globals::categoryOffsetY));
        bool isCat = (currentState == STATE_CATEGORIES); float scroll_y = 0.0f, scroll_max_y = 0.0f, visible_h = 0.0f;
        ImGui::BeginChild("GuiScrollRegion", ImVec2(currentGuiWidth - 40.0f * scaled_UI, currentGuiHeight - 100.0f * scaled_UI), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        if (!isCat && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0, 3.0f)) { ImGui::SetScrollY(ImGui::GetScrollY() - Globals::io->MouseDelta.y); }

        if (currentState == STATE_CATEGORIES) {
            // 分类顺序调整：Render 移至 World 下方
            const char* catsEN[] = {"Combat", "Player", "Misc", "World", "Render", "Visual", "Other"};
            const char* catsCN[] = {"战斗", "玩家", "杂项", "世界", "渲染", "视觉", "其他"};
            for (int i = 0; i < 7; ++i) {
                ImGui::PushID(i);
                if (AnimatedHoverText(Globals::clickguiLanguage == 0 ? catsEN[i] : catsCN[i], 0.75f)) { currentCategory = catsEN[i]; nextState = STATE_MODULES; isTransitioning = true; }
                ImGui::PopID();
            }
        } 
        else if (currentState == STATE_MODULES) {
            float rightBound = guiPos.x + currentGuiWidth - 20.0f * scaled_UI; 
            if (currentCategory == "Render") {
                ModItem mods[] = { ModItem(tr("ESP", "透视"), &Globals::mod_esp_enable, "ESP", &key_esp) };
                for (int i = 0; i < 1; ++i) {
                    bool settingsClicked = false;
                    if (ModuleButton(mods[i].label, mods[i].state, &settingsClicked, mods[i].modName.c_str())) { if (settingsClicked) { currentModule = mods[i].modName; nextState = STATE_SETTINGS; isTransitioning = true; } }
                }
            }
            else if (currentCategory == "Visual") {
                ModItem mods[] = { 
                    ModItem(tr("ClickGui", "菜单显示"), &Globals::showClickGui, "ClickGui", &key_clickgui), 
                    ModItem(tr("Island", "灵动岛"), &Globals::mod_island_enable, "Island", &key_island), 
                    ModItem(tr("Interface", "界面"), &Globals::mod_interface_enable, "Interface", &key_interface), 
                    ModItem(tr("Target HUD", "目标信息"), &Globals::mod_targethud_enable, "Targethud", &key_targethud), 
                    ModItem(tr("ArrayList", "列表"), &Globals::mod_arraylist_enable, "ArrayList", &key_arraylist) 
                };
                for (int i = 0; i < 5; ++i) {
                    float next_w = font->CalcTextSizeA(ImGui::GetFontSize() * scaled_UI, 10000.0f, 0.0f, mods[i].label).x + 45.0f * scaled_UI;
                    if (i > 0) { float last_x2 = ImGui::GetItemRectMax().x; if (last_x2 + 15.0f * scaled_UI + next_w < rightBound) ImGui::SameLine(0.0f, 15.0f * scaled_UI); else ImGui::Spacing(); }
                    bool settingsClicked = false;
                    if (ModuleButton(mods[i].label, mods[i].state, &settingsClicked, mods[i].modName.c_str())) { if (settingsClicked) { currentModule = mods[i].modName; nextState = STATE_SETTINGS; isTransitioning = true; } }
                }
            } 
            else if (currentCategory == "Player") {
                ModItem mods[] = { ModItem(tr("Scaffold", "脚手架"), &Globals::mod_scaffold_enable, "Scaffold", &key_scaffold) };
                for (int i = 0; i < 1; ++i) {
                    bool settingsClicked = false;
                    if (ModuleButton(mods[i].label, mods[i].state, &settingsClicked, mods[i].modName.c_str())) { if (settingsClicked) { currentModule = mods[i].modName; nextState = STATE_SETTINGS; isTransitioning = true; } }
                }
            }
            else if (currentCategory == "Other") {
                static bool dummy_state = true, dummy_kb = true;
                ModItem mods[] = { ModItem(tr("Float Window", "悬浮窗"), &dummy_state, "FloatConfig", &key_island), ModItem(tr("Keyboard", "虚拟键盘"), &dummy_kb, "Keyboard", &key_island) };
                for (int i = 0; i < 2; ++i) {
                    float next_w = font->CalcTextSizeA(ImGui::GetFontSize() * scaled_UI, 10000.0f, 0.0f, mods[i].label).x + 45.0f * scaled_UI;
                    if (i > 0) { float last_x2 = ImGui::GetItemRectMax().x; if (last_x2 + 15.0f * scaled_UI + next_w < rightBound) ImGui::SameLine(0.0f, 15.0f * scaled_UI); else ImGui::Spacing(); }
                    bool settingsClicked = false;
                    if (ModuleButton(mods[i].label, mods[i].state, &settingsClicked, mods[i].modName.c_str())) { if (settingsClicked) { currentModule = mods[i].modName; nextState = STATE_SETTINGS; isTransitioning = true; } }
                }
            } else { ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, Globals::currentUIAlpha), "%s", tr("No modules yet.", "此分类暂无模块")); }
        }
        else if (currentState == STATE_SETTINGS) {
            if (currentCategory == "Render" && currentModule == "ESP") {
                CustomKeybind(tr("Keybind", "快捷键"), &key_esp); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (CustomButton(al_show_esp ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_esp = !al_show_esp;
                if (CustomButton(show_fb_esp ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_esp = !show_fb_esp;
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (CustomButton(ModuleESP::esp_chroma ? tr("Chroma Glow: ON", "流光变色: 开") : tr("Chroma Glow: OFF", "流光变色: 关"), 0.0f)) ModuleESP::esp_chroma = !ModuleESP::esp_chroma;
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                CustomSliderFloat(tr("Line Thickness", "线条粗细"), &ModuleESP::esp_thickness, 1.0f, 10.0f, "%.1f");
                CustomSliderFloat(tr("Width", "方框宽"), &ModuleESP::esp_w, 50.0f, 400.0f, "%.0f");
                CustomSliderFloat(tr("Height", "方框高"), &ModuleESP::esp_h, 50.0f, 400.0f, "%.0f");
                ImGui::TextUnformatted(tr("Use Interface Shadow sliders", "请到 Interface 里调全局阴影"));
                CustomSliderFloat(tr("Target Health", "目标血量测试"), &ModuleESP::esp_hp, 0.0f, 20.0f, "%.1f");
            }
            else if (currentCategory == "Visual" && currentModule == "ClickGui") {
                CustomKeybind(tr("Keybind", "快捷键"), &key_clickgui); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (CustomButton(al_show_clickgui ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_clickgui = !al_show_clickgui;
                if (CustomButton(show_fb_clickgui ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) { if (show_fb_clickgui) { show_cg_confirm = true; } else { show_fb_clickgui = true; } }
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                CustomSliderFloat(tr("Menu Scale", "菜单整体缩放"), &clickgui_scale, 0.5f, 2.5f, "%.2f"); 
            }
            else if (currentCategory == "Visual" && currentModule == "Island") {
                CustomKeybind(tr("Keybind", "快捷键"), &key_island); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (CustomButton(al_show_island ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_island = !al_show_island;
                if (CustomButton(show_fb_island ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_island = !show_fb_island;
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                CustomSliderFloat(tr("Island Scale", "全局缩放"), &ModuleIsland::islandScale, 0.3f, 3.0f, "%.2f"); 
                CustomSliderFloat(tr("Offset X", "X轴偏移"), &ModuleIsland::islandOffsetX, -((float)Globals::surfaceWidth), (float)Globals::surfaceWidth, "%.0f"); 
                CustomSliderFloat(tr("Offset Y", "Y轴偏移"), &ModuleIsland::islandOffsetY, 0.0f, (float)Globals::surfaceHigh, "%.0f");
            } 
            else if (currentCategory == "Visual" && currentModule == "Interface") {
                CustomKeybind(tr("Keybind", "快捷键"), &key_interface); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (CustomButton(al_show_interface ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_interface = !al_show_interface;
                if (CustomButton(show_fb_interface ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_interface = !show_fb_interface;
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                CustomInputText(tr("Client Name", "客户端名称"), Globals::clientName, sizeof(Globals::clientName));
                
                // --- 位置调节 ---
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                CustomSliderFloat(tr("Title Offset X", "名称X轴偏移"), &Globals::clientNameOffsetX, -300.0f, 300.0f, "%.1f");
                CustomSliderFloat(tr("Title Offset Y", "名称Y轴偏移"), &Globals::clientNameOffsetY, -100.0f, 100.0f, "%.1f");
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                CustomSliderFloat(tr("Category Offset X", "类别X轴偏移"), &Globals::categoryOffsetX, -100.0f, 100.0f, "%.1f");
                CustomSliderFloat(tr("Category Offset Y", "类别Y轴偏移"), &Globals::categoryOffsetY, -100.0f, 100.0f, "%.1f");
                
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                CustomSliderFloat(tr("Bg Alpha", "背景透明度"), &Globals::bgAlpha, 0.0f, 1.0f, "%.2f"); 
                CustomSliderFloat(tr("Shadow Size", "阴影范围"), &Globals::shadowSize, 0.0f, 60.0f, "%.1f");
                CustomSliderFloat(tr("Shadow Alpha", "阴影透明度"), &Globals::shadowAlpha, 0.0f, 1.0f, "%.2f"); 
                CustomSliderFloat(tr("Shadow Offset Y", "阴影Y偏移"), &Globals::shadowOffsetY, -30.0f, 30.0f, "%.2f");
                CustomSliderFloat(tr("Overall Rounding", "全局圆角大小"), &Globals::clickguiRounding, 0.0f, 40.0f, "%.2f");

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                const char* langOptsEN[] = {"English", "Chinese"}; const char* langOptsCN[] = {"英文", "中文"};
                CustomCombo(tr("Language", "界面语言"), &Globals::clickguiLanguage, Globals::clickguiLanguage == 0 ? langOptsEN : langOptsCN, 2);
                if (CustomButton(Globals::enableWave ? tr("Disable Wave", "关闭彩虹波动") : tr("Enable Wave", "启用彩虹波动"), 0.0f)) { Globals::enableWave = !Globals::enableWave; }
                CustomSliderFloat(tr("Wave Speed", "波动速度"), &Globals::waveSpeed, 0.1f, 10.0f, "%.2f"); 
                CustomCombo(tr("Theme", "主题预设"), &currentThemeIndex, themeNames, presetCount);
            }
            else if (currentCategory == "Visual" && currentModule == "Targethud") {
                CustomKeybind(tr("Keybind", "快捷键"), &key_targethud); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (CustomButton(al_show_targethud ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_targethud = !al_show_targethud;
                if (CustomButton(show_fb_targethud ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_targethud = !show_fb_targethud;
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (CustomButton(tr("Simulate Hit", "触发受击动画"), 0.0f)) { ModuleTargetHUD::SimulateHit(); }
                const char* modes[] = {"Fate", "Rhythm"}; CustomCombo(tr("TH Mode", "样式"), &ModuleTargetHUD::th_mode, modes, 2);
                CustomSliderFloat(tr("Self Health", "自身血量"), &ModuleTargetHUD::self_health, 0.0f, 20.0f, "%.0f"); CustomSliderInt(tr("Target Health", "目标血量"), &ModuleTargetHUD::target_health, 0, 20);
                CustomSliderFloat(tr("Scale", "全局缩放比例"), &ModuleTargetHUD::th_scale, 0.5f, 3.0f, "%.2f"); 
                CustomSliderFloat(tr("Offset X", "全局X轴偏移"), &ModuleTargetHUD::th_offsetX, -((float)Globals::surfaceWidth), (float)Globals::surfaceWidth, "%.0f"); 
                CustomSliderFloat(tr("Offset Y", "全局Y轴偏移"), &ModuleTargetHUD::th_offsetY, -((float)Globals::surfaceHigh), (float)Globals::surfaceHigh, "%.0f");
            }
            else if (currentCategory == "Visual" && currentModule == "ArrayList") {
                CustomKeybind(tr("Keybind", "快捷键"), &key_arraylist); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (CustomButton(al_show_arraylist ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_arraylist = !al_show_arraylist;
                if (CustomButton(show_fb_arraylist ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_arraylist = !show_fb_arraylist;
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                float rightBound = guiPos.x + currentGuiWidth - 20.0f * scaled_UI;
                if (CustomButton(ModuleArrayList::al_no_bg ? "No Background: ON" : "No Background: OFF", 0.0f)) ModuleArrayList::al_no_bg = !ModuleArrayList::al_no_bg;
                if (CustomButton(ModuleArrayList::al_no_line ? "No Line: ON" : "No Line: OFF", 0.0f)) ModuleArrayList::al_no_line = !ModuleArrayList::al_no_line;
                if (CustomButton(ModuleArrayList::al_glow_enable ? tr("Text Blur Glow: ON", "高斯模糊辉光: 开") : tr("Text Blur Glow: OFF", "高斯模糊辉光: 关"), 0.0f)) ModuleArrayList::al_glow_enable = !ModuleArrayList::al_glow_enable;
                if (ModuleArrayList::al_glow_enable) { CustomSliderFloat(tr("Glow Alpha", "辉光透明度"), &ModuleArrayList::al_glow_alpha, 0.0f, 1.0f, "%.2f"); CustomSliderFloat(tr("Glow Radius", "模糊半径"), &ModuleArrayList::al_glow_intensity, 0.0f, 10.0f, "%.1f"); }
                CustomSliderFloat(tr("Scale", "全局缩放比例"), &ModuleArrayList::al_scale, 0.5f, 2.0f, "%.2f"); 
                CustomSliderFloat(tr("Offset X", "水平偏移"), &ModuleArrayList::al_offsetX, -((float)Globals::surfaceWidth), (float)Globals::surfaceWidth, "%.0f"); 
                CustomSliderFloat(tr("Offset Y", "垂直偏移"), &ModuleArrayList::al_offsetY, 0.0f, (float)Globals::surfaceHigh, "%.0f");
                CustomSliderFloat(tr("Module Spacing", "模块间隔"), &ModuleArrayList::al_spacing, -5.0f, 10.0f, "%.1f"); 
                CustomSliderFloat(tr("Line Width", "线条宽度"), &ModuleArrayList::al_line_width, 1.0f, 10.0f, "%.1f"); 
                CustomSliderFloat(tr("Line Height Mult", "线条高度倍率"), &ModuleArrayList::al_line_height_mult, 0.1f, 2.0f, "%.2f");
                CustomSliderFloat(tr("Line Offset X", "线条X轴调节"), &ModuleArrayList::al_line_offsetX, -50.0f, 50.0f, "%.1f"); 
                ImGui::TextUnformatted(tr("Use Interface Shadow sliders", "请到 Interface 里调全局阴影"));
            }
            else if (currentCategory == "Player" && currentModule == "Scaffold") {
                CustomKeybind(tr("Keybind", "快捷键"), &key_scaffold); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                if (CustomButton(al_show_scaffold ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_scaffold = !al_show_scaffold;
                if (CustomButton(show_fb_scaffold ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_scaffold = !show_fb_scaffold;
                CustomSliderFloat(tr("Blocks Left", "剩余方块"), &Globals::blockCount, 0.0f, 64.0f, "%.0f");
            }
            else if (currentCategory == "Other" && currentModule == "Keyboard") { 
                CustomSliderFloat(tr("Keyboard Scale", "键盘全局缩放"), &ModuleKeyboard::vk_global_scale, 0.5f, 2.5f, "%.2f");
                CustomSliderFloat(tr("Button Size", "按钮基础大小"), &ModuleKeyboard::vk_btn_size, 20.0f, 80.0f, "%.1f");
                CustomSliderFloat(tr("Text Size", "文本大小"), &ModuleKeyboard::vk_text_size, 10.0f, 40.0f, "%.1f");
            }
            else if (currentCategory == "Other" && currentModule == "FloatConfig") { 
                if (CustomButton(tr("Reset All Positions", "重置所有悬浮窗位置"), 0.0f)) { ModuleFloatConfig::fb_reset_pos = true; }
                CustomSliderFloat(tr("Scale", "全局缩放"), &ModuleFloatConfig::fb_scale, 0.5f, 3.0f, "%.2f");
                CustomSliderFloat(tr("Font Size", "字体大小"), &ModuleFloatConfig::fb_font_size, 10.0f, 50.0f, "%.1f");
                CustomSliderFloat(tr("Rounding", "圆角大小"), &ModuleFloatConfig::fb_rounding, 0.0f, 50.0f, "%.1f");
                if (CustomButton(ModuleFloatConfig::fb_chroma ? tr("Text Chroma: ON", "流光文字: 开") : tr("Text Chroma: OFF", "流光文字: 关"), 0.0f)) ModuleFloatConfig::fb_chroma = !ModuleFloatConfig::fb_chroma;
                CustomSliderFloat(tr("Text Alpha (ON)", "开启时文字透明度"), &ModuleFloatConfig::fb_text_alpha_on, 0.0f, 1.0f, "%.2f");
                CustomSliderFloat(tr("Text Alpha (OFF)", "关闭时文字透明度"), &ModuleFloatConfig::fb_text_alpha_off, 0.0f, 1.0f, "%.2f");
                CustomSliderFloat(tr("Background Alpha", "背景透明度"), &ModuleFloatConfig::fb_bg_alpha, 0.0f, 1.0f, "%.2f");
                ImGui::TextUnformatted(tr("Use Interface Shadow sliders", "请到 Interface 里调全局阴影"));
            }
        }
        
        if (!isCat) { scroll_y = ImGui::GetScrollY(); scroll_max_y = ImGui::GetScrollMaxY(); visible_h = ImGui::GetWindowHeight(); }
        ImGui::EndChild();

        if (!isCat && scroll_max_y > 0.0f) {
            float thumb_h = MAX_F(visible_h * (visible_h / (visible_h + scroll_max_y)), 20.0f); float thumb_y = (scroll_y / scroll_max_y) * (visible_h - thumb_h);
            float bar_x = guiPos.x + currentGuiWidth - 8.0f; float bar_y = guiPos.y + 80.0f * scaled_UI + thumb_y;
            guiDraw->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + 3.0f, bar_y + thumb_h), IM_COL32(180, 180, 180, (int)(Globals::currentUIAlpha * 180.0f)), 1.5f);
        }

        static ImVec2 resizerInitialMouse; static float resizerInitialW, resizerInitialH;
        ImVec2 br = ImVec2(guiPos.x + currentGuiWidth, guiPos.y + currentGuiHeight); ImVec2 grip_min(br.x - 50, br.y - 50); ImVec2 grip_max = br;
        bool gripHovered = (mousePos.x >= grip_min.x && mousePos.x <= grip_max.x && mousePos.y >= grip_min.y && mousePos.y <= grip_max.y);
        if (gripHovered && ImGui::IsMouseClicked(0)) { isDraggingResizer = true; resizerInitialMouse = mousePos; resizerInitialW = targetGuiWidth; resizerInitialH = targetGuiHeight; }
        if (!ImGui::IsMouseDown(0)) isDraggingResizer = false;
        if (isDraggingResizer && currentState != STATE_CATEGORIES) {
            targetGuiWidth = resizerInitialW + (mousePos.x - resizerInitialMouse.x); targetGuiHeight = resizerInitialH + (mousePos.y - resizerInitialMouse.y);
            targetGuiWidth = MAX_F(targetGuiWidth, 350.0f); targetGuiHeight = MAX_F(targetGuiHeight, 400.0f); savedExpandedWidth = targetGuiWidth; savedExpandedHeight = targetGuiHeight;
        }

        if (currentState != STATE_CATEGORIES) {
            guiDraw->AddLine(ImVec2(br.x - 15, br.y - 5), ImVec2(br.x - 5, br.y - 15), IM_COL32(150, 150, 150, (int)(Globals::globalGuiAlpha * 180)), 3.0f);
            guiDraw->AddLine(ImVec2(br.x - 25, br.y - 5), ImVec2(br.x - 5, br.y - 25), IM_COL32(150, 150, 150, (int)(Globals::globalGuiAlpha * 180)), 3.0f);
            guiDraw->AddLine(ImVec2(br.x - 35, br.y - 5), ImVec2(br.x - 5, br.y - 35), IM_COL32(150, 150, 150, (int)(Globals::globalGuiAlpha * 180)), 3.0f);
        }
        ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(2);

        // =========================================================
        // 二次确认关闭弹窗渲染
        // =========================================================
        if (cg_confirm_anim > 0.001f) {
            float ca = cg_confirm_anim * Globals::globalGuiAlpha;
            float ca_alpha = CLAMP_F(ca, 0.0f, 1.0f); 
            ImVec2 pop_sz = ImVec2(300.0f * scaled_UI, 150.0f * scaled_UI);
            ImVec2 pop_pos = ImVec2(Globals::surfaceWidth / 2.0f - (pop_sz.x / 2.0f) * cg_confirm_anim, Globals::surfaceHigh / 2.0f - (pop_sz.y / 2.0f) * cg_confirm_anim);
            ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(Globals::surfaceWidth, Globals::surfaceHigh), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); 
            ImGui::Begin("CG_Confirm_Layer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
            ImDrawList* cDraw = ImGui::GetWindowDrawList();
            if (Globals::shadowSize > 0.0f && Globals::shadowAlpha > 0.0f) {
                cDraw->AddShadowRect(
                    pop_pos,
                    ImVec2(pop_pos.x + pop_sz.x * cg_confirm_anim, pop_pos.y + pop_sz.y * cg_confirm_anim),
                    IM_COL32(0, 0, 0, (int)(Globals::shadowAlpha * ca_alpha * 255.0f)),
                    Globals::shadowSize * cg_confirm_anim,
                    ImVec2(0.0f, 0.0f),
                    0,
                    Globals::clickguiRounding
                );
            }
            cDraw->AddRectFilled(pop_pos, ImVec2(pop_pos.x + pop_sz.x * cg_confirm_anim, pop_pos.y + pop_sz.y * cg_confirm_anim), IM_COL32(0,0,0, (int)(Globals::bgAlpha * ca_alpha * 255.0f)), Globals::clickguiRounding);
            const char* title = tr("Close Float Window?", "确认关闭悬浮窗吗?");
            ImVec2 t_sz = font->CalcTextSizeA(30.0f * scaled_UI * cg_confirm_anim, 10000.0f, 0.0f, title);
            cDraw->AddText(font, 30.0f * scaled_UI * cg_confirm_anim, ImVec2(pop_pos.x + (pop_sz.x * cg_confirm_anim - t_sz.x) * 0.5f, pop_pos.y + 20.0f * scaled_UI * cg_confirm_anim), IM_COL32(255, 255, 255, (int)(ca_alpha * 255.0f)), title);
            ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(2);
            ImGui::SetNextWindowPos(pop_pos, ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(pop_sz.x * cg_confirm_anim, pop_sz.y * cg_confirm_anim), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); 
            ImGui::Begin("CG_Confirm_Interact", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::SetCursorPos(ImVec2(20.0f * scaled_UI * cg_confirm_anim, 90.0f * scaled_UI * cg_confirm_anim));
            float btn_w = 110.0f * scaled_UI * cg_confirm_anim;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.2f,0.2f,ca_alpha)); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f,0.3f,0.3f,ca_alpha)); ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f,0.1f,0.1f,ca_alpha)); ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Globals::clickguiRounding);
            if (ImGui::Button(tr("Cancel", "取消"), ImVec2(btn_w, 40.0f * scaled_UI * cg_confirm_anim))) { show_cg_confirm = false; }
            ImGui::SameLine(0, 40.0f * scaled_UI * cg_confirm_anim);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f,0.2f,0.2f,ca_alpha)); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f,0.3f,0.3f,ca_alpha)); ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f,0.1f,0.1f,ca_alpha));
            if (ImGui::Button(tr("Confirm", "确认关闭"), ImVec2(btn_w, 40.0f * scaled_UI * cg_confirm_anim))) { show_fb_clickgui = false; show_cg_confirm = false; }
            ImGui::PopStyleColor(6); ImGui::PopStyleVar(); ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(2);
        }
    }
}