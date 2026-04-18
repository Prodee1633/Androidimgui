#include "Modules/ArrayList.h"
#include "Globals.h"
#include <algorithm>
#include <cmath>

namespace ModuleArrayList {
    float al_scale = 1.0f;
    float al_offsetX = 0.0f;
    float al_offsetY = 0.0f;
    
    float al_spacing = 0.0f;
    float al_line_width = 3.0f;
    float al_line_height_mult = 1.0f;
    float al_line_offsetX = 0.0f;
    float al_bg_overlap = 0.0f; 
    
    bool al_no_bg = false;
    bool al_no_line = false;
    
    bool al_glow_enable = true; 
    float al_glow_alpha = 0.5f;
    float al_glow_intensity = 2.0f;

    static float anim_al_offsetY = 0.0f;
    static float anim_al_offsetX = 0.0f;

    void Render() {
        // 核心修复：位置调节坐标恢复刷新
        anim_al_offsetX += (al_offsetX - anim_al_offsetX) * 10.0f * Globals::io->DeltaTime;
        anim_al_offsetY += (al_offsetY - anim_al_offsetY) * 10.0f * Globals::io->DeltaTime;

        const float al_anim_speed = 2.0f;
        for(int i = 0; i < 7; i++) { 
            bool is_visible = *Globals::al_mods[i].state && *Globals::al_mods[i].show_in_al;
            Globals::al_mods[i].anim_t += (is_visible ? 1.0f : -1.0f) * al_anim_speed * Globals::io->DeltaTime; 
            Globals::al_mods[i].anim_t = CLAMP_F(Globals::al_mods[i].anim_t, 0.0f, 1.0f); 
        }

        if (!Globals::mod_arraylist_enable) return;

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always); 
        ImGui::SetNextWindowSize(ImVec2(Globals::surfaceWidth, Globals::surfaceHigh), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::Begin("ArrayListHUD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
        ImDrawList* alList = ImGui::GetWindowDrawList();
        
        struct AlItemDraw { Globals::ArraylistMod* mod; float t; float y_min; float y_max; float x_min; float x_max; };
        std::vector<AlItemDraw> draw_items; 
        std::vector<Globals::ArraylistMod*> active_mods;
        for(int i = 0; i < 7; i++) { if (Globals::al_mods[i].anim_t > 0.001f) active_mods.push_back(&Globals::al_mods[i]); }
        
        float font_size = 25.0f * al_scale;
        std::sort(active_mods.begin(), active_mods.end(), [&](Globals::ArraylistMod* a, Globals::ArraylistMod* b) {
            float w_a = Globals::font->CalcTextSizeA(font_size, 10000.0f, 0.0f, a->name).x;
            float w_b = Globals::font->CalcTextSizeA(font_size, 10000.0f, 0.0f, b->name).x;
            return w_a > w_b; 
        });

        float y = anim_al_offsetY + 5.0f;
        for(size_t i = 0; i < active_mods.size(); i++) {
            auto* mod = active_mods[i]; 
            float e_t = 1.0f - std::pow(1.0f - mod->anim_t, 4.0f); 
            ImVec2 target_txt_sz = Globals::font->CalcTextSizeA(font_size, 10000.0f, 0.0f, mod->name);
            float target_w = target_txt_sz.x + 10.0f * al_scale + al_line_width * al_scale;
            float target_h = target_txt_sz.y + 10.0f * al_scale;
            mod->current_w += (target_w - mod->current_w) * 15.0f * Globals::io->DeltaTime;
            mod->current_h += (target_h - mod->current_h) * 15.0f * Globals::io->DeltaTime;
            float draw_w = mod->current_w; float draw_h = mod->current_h * e_t; 
            float target_x = Globals::surfaceWidth - draw_w + anim_al_offsetX; 
            float current_x = (Globals::surfaceWidth + 20.0f + anim_al_offsetX) + (target_x - (Globals::surfaceWidth + 20.0f + anim_al_offsetX)) * e_t;
            draw_items.push_back({mod, e_t, y, y + draw_h, current_x, current_x + draw_w}); 
            y += (draw_h + al_spacing * al_scale) * e_t;
        }
        
        for(size_t i = 0; i < draw_items.size(); i++) {
            auto& item = draw_items[i]; 
            ImVec2 min_pt(item.x_min, item.y_min), max_pt(item.x_max, item.y_max);
            float wave_top = Globals::waveTime * Globals::waveSpeed + i * 0.15f;
            ImVec4 c_top = Globals::LerpColorMulti(Globals::themeColors, wave_top, 1.0f);
            
            if (Globals::shadowSize > 0.001f && Globals::shadowAlpha > 0.0f) {
                const float shadow_pad = Globals::shadowSize * al_scale + 8.0f * al_scale;
                const float clip_y_min = (i > 0) ? ((draw_items[i - 1].y_max + item.y_min) * 0.5f) : (item.y_min - shadow_pad);
                const float clip_y_max = (i + 1 < draw_items.size()) ? ((item.y_max + draw_items[i + 1].y_min) * 0.5f) : (item.y_max + shadow_pad);
                ImU32 shadow_col_top = ImGui::ColorConvertFloat4ToU32(ImVec4(c_top.x, c_top.y, c_top.z, Globals::shadowAlpha * item.t));
                ImU32 shadow_col_bot = ImGui::ColorConvertFloat4ToU32(ImVec4(c_bot.x, c_bot.y, c_bot.z, Globals::shadowAlpha * item.t));

                auto draw_shadow_clip = [&](const ImVec2& clip_min, const ImVec2& clip_max) {
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                        return;
                    alList->PushClipRect(clip_min, clip_max, false);
                    alList->AddShadowRectMultiColor(
                        min_pt,
                        max_pt,
                        shadow_col_top,
                        shadow_col_top,
                        shadow_col_bot,
                        shadow_col_bot,
                        Globals::shadowSize * al_scale,
                        ImVec2(0.0f, 0.0f),
                        0,
                        0.0f
                    );
                    alList->PopClipRect();
                };

                draw_shadow_clip(ImVec2(min_pt.x - shadow_pad, clip_y_min), ImVec2(max_pt.x + shadow_pad, clip_y_max));

                if (i > 0 && item.x_min < draw_items[i - 1].x_min) {
                    draw_shadow_clip(
                        ImVec2(min_pt.x - shadow_pad, item.y_min - shadow_pad),
                        ImVec2(draw_items[i - 1].x_min, clip_y_min)
                    );
                }

                if (i + 1 < draw_items.size() && item.x_min < draw_items[i + 1].x_min) {
                    draw_shadow_clip(
                        ImVec2(min_pt.x - shadow_pad, clip_y_max),
                        ImVec2(draw_items[i + 1].x_min, item.y_max + shadow_pad)
                    );
                }
            }
            
            if (!al_no_bg) alList->AddRectFilled(min_pt, max_pt, IM_COL32(0, 0, 0, (int)(Globals::bgAlpha * 255.0f * item.t)));
            
            float line_w = al_line_width * al_scale, line_h = (max_pt.y - min_pt.y) * al_line_height_mult;
            float line_y_min = min_pt.y + ((max_pt.y - min_pt.y) - line_h) * 0.5f, line_y_max = min_pt.y + ((max_pt.y - min_pt.y) + line_h) * 0.5f;
            float wave_bot = Globals::waveTime * Globals::waveSpeed + (i + 1) * 0.15f;
            ImVec4 c_bot = Globals::LerpColorMulti(Globals::themeColors, wave_bot, 1.0f);
            ImU32 col_top = ImGui::ColorConvertFloat4ToU32(ImVec4(c_top.x, c_top.y, c_top.z, item.t)), col_bot = ImGui::ColorConvertFloat4ToU32(ImVec4(c_bot.x, c_bot.y, c_bot.z, item.t));
            
            float render_line_x = max_pt.x + al_line_offsetX * al_scale;
            if (!al_no_line) alList->AddRectFilledMultiColor(ImVec2(render_line_x - line_w, line_y_min), ImVec2(render_line_x, line_y_max), col_top, col_top, col_bot, col_bot);
            
            float render_font_size = MAX_F(0.1f, 25.0f * al_scale * item.t);
            ImVec2 render_txt_sz = Globals::font->CalcTextSizeA(render_font_size, 10000.0f, 0.0f, item.mod->name);
            float text_y = min_pt.y + ((max_pt.y - min_pt.y) - render_txt_sz.y) * 0.5f; 
            float final_text_x = min_pt.x + 5.0f * al_scale;
            
            if (al_glow_enable) {
                float glow_alpha = item.t * al_glow_alpha * 0.35f; 
                ImU32 text_glow_col = ImGui::ColorConvertFloat4ToU32(ImVec4(c_top.x, c_top.y, c_top.z, glow_alpha));
                float radius = al_glow_intensity * al_scale;
                
                int passes = 8;
                for(int j = 0; j < passes; j++) {
                    float ang = (j / (float)passes) * 6.28318f; 
                    alList->AddText(Globals::font, render_font_size, ImVec2(final_text_x + cos(ang) * radius, text_y + sin(ang) * radius), text_glow_col, item.mod->name);
                }
            }
            alList->AddText(Globals::font, render_font_size, ImVec2(final_text_x, text_y), col_top, item.mod->name);
        }
        ImGui::End(); ImGui::PopStyleColor(1); ImGui::PopStyleVar(2);
    }
}