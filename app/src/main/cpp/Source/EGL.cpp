#include "EGL.h"
#include <string> 
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <float.h>
#include <cmath>
#include <algorithm>

EGL::EGL() {
    mEglDisplay = EGL_NO_DISPLAY;
    mEglSurface = EGL_NO_SURFACE;
    mEglConfig  = nullptr;
    mEglContext = EGL_NO_CONTEXT;
}

static bool RunInitImgui;

int EGL::initEgl() {
    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mEglDisplay == EGL_NO_DISPLAY) {
        return -1;
    }
    EGLint *version = new EGLint[2];
    if (!eglInitialize(mEglDisplay, &version[0], &version[1])) {
        return -1;
    }
    const EGLint attribs[] = {EGL_BUFFER_SIZE, 32, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                              EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE};
    EGLint num_config;
    if (!eglGetConfigs(mEglDisplay, NULL, 1, &num_config)) {
        return -1;
    }
    if (!eglChooseConfig(mEglDisplay, attribs, &mEglConfig, 1, &num_config)) {
        return -1;
    }
    int attrib_list[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    mEglContext = eglCreateContext(mEglDisplay, mEglConfig, EGL_NO_CONTEXT, attrib_list);
    if (mEglContext == EGL_NO_CONTEXT) {
        return -1;
    }
    mEglSurface = eglCreateWindowSurface(mEglDisplay, mEglConfig, SurfaceWin, NULL);
    if (mEglSurface == EGL_NO_SURFACE) {
        return -1;
    }
    if (!eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
        return -1;
    }
    return 1;
}

int EGL::initImgui() {
    if (RunInitImgui){
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplAndroid_Init(this->SurfaceWin);
        ImGui_ImplOpenGL3_Init("#version 300 es");
        return 1;
    }
    RunInitImgui = true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    io->IniSavingRate = 10.0f;
    std::string SaveFile = this->SaveDir;
    SaveFile += "/save.ini";
    io->IniFilename = SaveFile.c_str();
    ImGui_ImplAndroid_Init(this->SurfaceWin);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    imFont = io->Fonts->AddFontFromMemoryTTF((void *) OPPOSans_H, OPPOSans_H_size, 32.0f, &font_cfg, io->Fonts->GetGlyphRangesChineseFull());
    io->MouseDoubleClickTime = 0.0001f;
    g = ImGui::GetCurrentContext();
    style =&ImGui::GetStyle();
    style->ScaleAllSizes(4.0f);
    style->FramePadding=ImVec2(10.0f,20.0f);
    std::string LoadFile = this->SaveDir;
    LoadFile += "/Style.dat";
    ImGuiStyle s;
    if (MyFile::ReadFile(&s,LoadFile.c_str())==1){
       *style=s;
    }
    return 1;
}

void EGL::onSurfaceCreate(JNIEnv *env, jobject surface, int SurfaceWidth, int SurfaceHigh) {
    this->SurfaceWin = ANativeWindow_fromSurface(env, surface);
    this->surfaceWidth = SurfaceWidth;
    this->surfaceHigh = SurfaceHigh;
    this->surfaceWidthHalf = this->surfaceWidth / 2;
    this->surfaceHighHalf = this->surfaceHigh / 2;
    SurfaceThread = new std::thread([this] { EglThread(); });
    SurfaceThread->detach();
}

void EGL::onSurfaceChange(int SurfaceWidth, int SurfaceHigh) {
    this->surfaceWidth = SurfaceWidth;
    this->surfaceHigh = SurfaceHigh;
    this->surfaceWidthHalf = this->surfaceWidth / 2;
    this->surfaceHighHalf = this->surfaceHigh / 2;
    this->isChage = true;
}

void EGL::onSurfaceDestroy() {
    this->isDestroy = true;
    std::unique_lock<std::mutex> ulo(Threadlk);
    cond.wait(ulo, [this] { return !this->ThreadIo; });
    delete SurfaceThread;
    SurfaceThread = nullptr;
}

void EGL::EglThread() {
    if (this->initEgl() != 1) return;
    if (this->initImgui() != 1) return;
    ThreadIo = true;
    if (input == nullptr || io == nullptr) { ThreadIo = false; return; }
    input->initImguiIo(io); input->setImguiContext(g);

    #define MAX_F(a, b) ((a) > (b) ? (a) : (b))
    #define MIN_F(a, b) ((a) < (b) ? (a) : (b))
    #define CLAMP_F(x, min, max) MIN_F(MAX_F(x, min), max)
    #define ABS_F(x) ((x) < 0.0f ? -(x) : (x))

    // 基础 UI_SCALE
    const float BASE_UI_SCALE = 1.3f;
    static float clickgui_scale = 1.0f;
    static float anim_clickgui_scale = 1.0f;

    const float iconSize = 30.0f, textSize = 25.0f, padCollapsedX = 15.0f, padBlockX = 10.0f;         
    const float scaffoldBgSize = 65.0f, cubeSize = 80.0f, progressBarOffsetY = -2.5f;
    const float shadowOffsetY = -0.059f, shadowSize = 20.0f, animSpeed = 25.0f, animBounciness = 0.55f;    
    const float normalHeight = 80.0f, expandedHeight = 100.0f, switchScale = 1.4f;      
    const float titleTextSize = 30.0f, statusTextSize = 15.0f, padNormalX = 25.0f;

    static float islandScale = 1.0f, islandOffsetX = 0.0f, islandOffsetY = 80.0f;  
    static float anim_islandScale = 1.0f; 

    static float bgAlpha = 0.5f, shadowAlpha = 0.3f, clickguiRounding = 15.0f; 
    static int clickguiLanguage = 0;       
    static char clientName[64] = "Fate"; 

    static float anim_islandOffsetX = 0.0f, anim_islandOffsetY = 80.0f;
    static float anim_th_offsetX = 0.0f, anim_th_offsetY = 150.0f;
    static float anim_al_offsetX = 0.0f, anim_al_offsetY = 0.0f;

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
    if (!themeInit) { for(int i = 0; i < presetCount; i++) themeNames[i] = presetData[i].name; themeInit = true; }
    static int currentThemeIndex = 0;
    
    auto HexToV4 = [](int hex) -> ImVec4 { return ImVec4(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f, 1.0f); };
    static std::vector<ImVec4> themeColors;
    if (themeColors.empty()) { themeColors.push_back(ImVec4(1,1,1,1)); themeColors.push_back(ImVec4(1,1,1,1)); }
    
    static bool enableWave = true; static float waveSpeed = 0.5f; static float blockCount = 12.0f; 

    struct Notification {
        bool active = false; char modName[64]; bool isEnable = false; float timer = 0.0f; 
        float switchAnimPos = 0.0f; float switchAnimVel = 0.0f; float heightMult = 0.0f; float heightVel = 0.0f; float linearAlpha = 0.0f;    
    };
    static Notification notifs[10]; 

    static float fpsTimer = 1.0f; static int currentFps = 0; static float waveTime = 0.0f; 
    static float currentCoreWidth = 0.0f, coreWidthVel = 0.0f, currentCoreHeight = 0.0f, coreHeightVel = 0.0f;
    static float currentRounding = 0.0f, activeMaxWidth = 0.0f, activeMaxWidthVel = 0.0f; 
    static float blockAnimMult = 0.0f, blockAnimVel = 0.0f, smoothProgress = 0.0f, progressVel = 0.0f;
    static float blockLinearAlpha = 0.0f, opalLinearAlpha = 1.0f;
    static float bpsTimer = 0.0f, lastBpsBlockCount = blockCount, currentBps = 0.0f, displayBps = 0.0f;

    static bool showClickGui = true; 
    static bool mod_island_enable = true, mod_interface_enable = true, mod_targethud_enable = false, mod_scaffold_enable = false, mod_arraylist_enable = true;
    static bool al_show_island = true, al_show_interface = true, al_show_targethud = true, al_show_scaffold = true, al_show_arraylist = true, al_show_clickgui = true;
    static int key_island = 0, key_interface = 0, key_targethud = 0, key_scaffold = 0, key_arraylist = 0, key_clickgui = 0;
    static int* active_bind_target = nullptr; 

    // === 全局悬浮窗配置 ===
    static float fb_scale = 1.0f;
    static float fb_font_size = 22.0f;
    static bool fb_chroma = true;
    static float fb_bg_alpha = 0.5f;
    static float fb_shadow_size = 15.0f;
    static float fb_shadow_alpha = 0.3f;
    static float fb_rounding = 15.0f;
    static float fb_text_alpha_on = 1.0f;
    static float fb_text_alpha_off = 0.6f;
    static bool fb_reset_pos = false;

    static bool show_fb_clickgui = false, show_fb_island = false, show_fb_interface = false, show_fb_targethud = false, show_fb_scaffold = false, show_fb_arraylist = false;

    struct ArraylistMod { 
        const char* name; bool* state; float anim_t; int* key; bool* show_in_al; 
        float current_w; float current_h;
        bool* show_fb; float target_fb_x; float target_fb_y; float current_fb_x; float current_fb_y;
        float fb_anim_w; float fb_anim_h; float fb_hold_time; bool fb_dragging;
    };
    static ArraylistMod al_mods[] = { 
        {"ClickGui", &showClickGui, 0.0f, &key_clickgui, &al_show_clickgui, 0.0f, 0.0f, &show_fb_clickgui, 100, 100, 100, 100, 0, 0, 0, false}, 
        {"Island", &mod_island_enable, 0.0f, &key_island, &al_show_island, 0.0f, 0.0f, &show_fb_island, 100, 200, 100, 200, 0, 0, 0, false}, 
        {"Interface", &mod_interface_enable, 0.0f, &key_interface, &al_show_interface, 0.0f, 0.0f, &show_fb_interface, 100, 300, 100, 300, 0, 0, 0, false}, 
        {"TargetHUD", &mod_targethud_enable, 0.0f, &key_targethud, &al_show_targethud, 0.0f, 0.0f, &show_fb_targethud, 100, 400, 100, 400, 0, 0, 0, false}, 
        {"Scaffold", &mod_scaffold_enable, 0.0f, &key_scaffold, &al_show_scaffold, 0.0f, 0.0f, &show_fb_scaffold, 100, 500, 100, 500, 0, 0, 0, false}, 
        {"ArrayList", &mod_arraylist_enable, 0.0f, &key_arraylist, &al_show_arraylist, 0.0f, 0.0f, &show_fb_arraylist, 100, 600, 100, 600, 0, 0, 0, false} 
    };
    
    // ArrayList 参数
    static float al_scale = 1.0f, al_offsetX = 0.0f, al_offsetY = 0.0f;
    const float al_anim_speed = 2.0f; 
    const float al_bg_height_offset = -5.5f; 
    static float al_spacing = 0.0f, al_line_width = 3.0f, al_line_height_mult = 1.0f, al_line_offsetX = 0.0f; 
    static float al_bg_overlap = 0.5f; 
    static bool al_shadow_chroma = false; static float al_shadow_size = 15.0f, al_chroma_shadow_alpha = 0.5f;
    static bool al_no_bg = false, al_no_shadow = false, al_no_line = false;
    static bool al_glow_enable = true; static float al_glow_alpha = 0.5f, al_glow_intensity = 2.0f;

    static float globalGuiAlpha = 1.0f; 
    enum GuiState { STATE_CATEGORIES, STATE_MODULES, STATE_SETTINGS };
    static GuiState currentState = STATE_CATEGORIES, nextState = STATE_CATEGORIES;
    static float viewAlpha = 1.0f; static bool isTransitioning = false;      
    static std::string currentCategory = "", currentModule = "";    

    static int th_mode = 0; 
    static float th_scale = 1.0f, th_offsetX = 0.0f, th_offsetY = 150.0f, self_health = 20.0f; static int target_health = 20; static float win_anim_t = 1.0f; 
    static float anim_th_scale = 1.0f; 
    static float anim_th_width = 0.0f, anim_th_height = 0.0f; 

    // TargetHUD Rhythm 锁定参数
    static float rh_name_scale = 1.5f;
    static float rh_hp_scale = 0.85f;
    static float rh_avatar_scale = 1.0f; 
    static float rh_win_scale = 0.85f;

    const float TH_APPEAR_T1 = 0.15f, TH_APPEAR_T2 = 0.10f, TH_DISAPPEAR_T1 = 0.15f, TH_DISAPPEAR_T2 = 0.10f;  
    enum ThAnimState { TH_HIDDEN, TH_APPEAR_1, TH_APPEAR_2, TH_VISIBLE, TH_DISAPPEAR_1, TH_DISAPPEAR_2 };
    static ThAnimState th_state = TH_HIDDEN; static float th_anim_timer = 0.0f, th_current_scale_mult = 0.0f; static bool last_targethud_enable = false;
    static float th_alpha_anim = 0.0f; 
    static float anim_target_health = 20.0f, hit_timer = 999.0f; static bool is_hit = false;

    struct TParticle { float angle, target_dist, size, curr_x, curr_y, life, max_life, linger_time, fade_time; };
    static std::vector<TParticle> th_particles;

    static ImVec2 guiBasePos = ImVec2(50, 100);
    static float currentGuiWidth = 250.0f, currentGuiHeight = 400.0f, targetGuiWidth = 250.0f, targetGuiHeight = 400.0f;
    static float savedExpandedWidth = 450.0f, savedExpandedHeight = 550.0f;
    static bool isDraggingWindow = false, isDraggingResizer = false; float currentUIAlpha = 1.0f;

    struct ModItem { const char* label; bool* state; std::string modName; int* key; ModItem(const char* l, bool* s, const char* n, int* k) { label = l; state = s; modName = std::string(n); key = k; } };

    auto UpdateSpring = [](float& current, float& velocity, float target, float speed, float bounciness, float dt) {
        if (dt > 0.033f) dt = 0.033f; float k = speed * speed, d = 2.0f * (1.0f - bounciness * 0.85f) * speed; 
        float force = k * (target - current) - d * velocity; velocity += force * dt; current += velocity * dt;
    };
    auto LerpColorMulti = [](const std::vector<ImVec4>& cols, float t, float alphaMult) -> ImVec4 {
        if (cols.empty()) return ImVec4(1.0f, 1.0f, 1.0f, alphaMult); if (cols.size() == 1) return ImVec4(cols[0].x, cols[0].y, cols[0].z, alphaMult);
        float floor_t = (float)((int)t); if (t < 0.0f && t != floor_t) floor_t -= 1.0f; t = t - floor_t; float scaledT = t * (float)cols.size(); 
        int idx1 = (int)scaledT; int idx2 = (idx1 + 1) % (int)cols.size(); float frac = scaledT - (float)idx1; frac = frac * frac * (3.0f - 2.0f * frac);
        ImVec4 a = cols[idx1]; ImVec4 b = cols[idx2]; return ImVec4(a.x + (b.x - a.x) * frac, a.y + (b.y - a.y) * frac, a.z + (b.z - a.z) * frac, alphaMult);
    };
    auto LerpColorSolid = [](const ImVec4& a, const ImVec4& b, float t) -> ImVec4 { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f); };
    auto LerpColor = [](const ImVec4& a, const ImVec4& b, float t, float alpha) -> ImVec4 { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, alpha); };
    
    auto tr = [&](const char* en, const char* cn) { return clickguiLanguage == 0 ? en : cn; };
    auto getCatCn = [&](const std::string& en) -> std::string {
        if (en == "Combat") return "战斗"; if (en == "Player") return "玩家"; if (en == "Misc") return "杂项"; if (en == "World") return "世界"; if (en == "Visual") return "视觉"; if (en == "Other") return "其他"; return en;
    };
    auto getModCn = [&](const std::string& en) -> std::string {
        if (en == "ClickGui") return "菜单显示"; if (en == "Island") return "灵动岛"; if (en == "Interface") return "菜单"; if (en == "Targethud") return "目标信息"; if (en == "Scaffold") return "脚手架"; if(en == "ArrayList") return "列表"; if (en == "FloatConfig") return "悬浮窗配置"; return en;
    };

    auto TriggerNotification = [&](const char* modNameStr, bool enable) {
        if (mod_scaffold_enable) return; bool found = false;
        for (int i = 0; i < 10; ++i) { if (notifs[i].active && std::string(notifs[i].modName) == modNameStr) { notifs[i].isEnable = enable; notifs[i].timer = 2.0f; found = true; break; } }
        if (!found) { for (int i = 0; i < 10; ++i) { if (!notifs[i].active) { notifs[i].active = true; snprintf(notifs[i].modName, sizeof(notifs[i].modName), "%s", modNameStr); notifs[i].isEnable = enable; notifs[i].timer = 2.0f; notifs[i].switchAnimPos = enable ? 0.0f : 1.0f; notifs[i].switchAnimVel = 0.0f; notifs[i].heightMult = 0.0f; notifs[i].heightVel = 0.0f; notifs[i].linearAlpha = 0.0f; break; } } }
    };

    while (true) {
        if (this->isDestroy) { ThreadIo = false; cond.notify_all(); return; }
        if (this->isChage) { glViewport(0, 0, this->surfaceWidth, this->surfaceHigh); this->isChage = false; }
        this->clearBuffers(); if (!ActivityState) { usleep(16000); continue; }
        
        imguiMainWinStart(); ImFont* font = ImGui::GetFont();

        fpsTimer += io->DeltaTime; if (fpsTimer >= 1.0f) { currentFps = (int)(io->Framerate); fpsTimer -= 1.0f; }
        waveTime += io->DeltaTime; bpsTimer += io->DeltaTime;
        if (bpsTimer >= 0.5f) { float diff = lastBpsBlockCount - blockCount; if (diff < 0) diff = 0; currentBps = diff / bpsTimer; lastBpsBlockCount = blockCount; bpsTimer = 0.0f; }
        displayBps += (currentBps - displayBps) * 5.0f * io->DeltaTime;

        anim_islandOffsetX += (islandOffsetX - anim_islandOffsetX) * 10.0f * io->DeltaTime;
        anim_islandOffsetY += (islandOffsetY - anim_islandOffsetY) * 10.0f * io->DeltaTime;
        anim_islandScale += (islandScale - anim_islandScale) * 10.0f * io->DeltaTime; 
        
        anim_th_offsetX += (th_offsetX - anim_th_offsetX) * 10.0f * io->DeltaTime;
        anim_th_offsetY += (th_offsetY - anim_th_offsetY) * 10.0f * io->DeltaTime;
        anim_th_scale += (th_scale - anim_th_scale) * 10.0f * io->DeltaTime; 
        
        anim_al_offsetX += (al_offsetX - anim_al_offsetX) * 10.0f * io->DeltaTime;
        anim_al_offsetY += (al_offsetY - anim_al_offsetY) * 10.0f * io->DeltaTime;
        
        anim_clickgui_scale += (clickgui_scale - anim_clickgui_scale) * 10.0f * io->DeltaTime; 
        float scaled_UI = BASE_UI_SCALE * anim_clickgui_scale; 

        if (active_bind_target != nullptr) {
            for (int i = 0; i < 650; i++) {
                ImGuiKey key = (ImGuiKey)i;
                if (ImGui::IsKeyPressed(key)) {
                    if (key == ImGuiKey_Escape) *active_bind_target = 0; 
                    else *active_bind_target = i;
                    active_bind_target = nullptr; break;
                }
            }
        }

        themeColors[0] = HexToV4(presetData[currentThemeIndex].c1); themeColors[1] = HexToV4(presetData[currentThemeIndex].c2);
        if (showClickGui) { globalGuiAlpha += io->DeltaTime * 5.0f; if (globalGuiAlpha > 1.0f) globalGuiAlpha = 1.0f; }
        else { globalGuiAlpha -= io->DeltaTime * 5.0f; if (globalGuiAlpha < 0.0f) globalGuiAlpha = 0.0f; }

        for(int i = 0; i < 6; i++) { 
            bool is_visible = *al_mods[i].state && *al_mods[i].show_in_al;
            al_mods[i].anim_t += (is_visible ? 1.0f : -1.0f) * al_anim_speed * io->DeltaTime; 
            al_mods[i].anim_t = CLAMP_F(al_mods[i].anim_t, 0.0f, 1.0f); 
        }

        if (mod_targethud_enable) { th_alpha_anim += io->DeltaTime / 0.3f; }
        else { th_alpha_anim -= io->DeltaTime / 0.3f; }
        th_alpha_anim = CLAMP_F(th_alpha_anim, 0.0f, 1.0f);

        if (mod_targethud_enable && !last_targethud_enable) { if (th_state == TH_HIDDEN || th_state == TH_DISAPPEAR_1 || th_state == TH_DISAPPEAR_2) { th_state = TH_APPEAR_1; th_anim_timer = 0.0f; } } 
        else if (!mod_targethud_enable && last_targethud_enable) { if (th_state == TH_VISIBLE || th_state == TH_APPEAR_1 || th_state == TH_APPEAR_2) { th_state = TH_DISAPPEAR_1; th_anim_timer = 0.0f; } }
        last_targethud_enable = mod_targethud_enable;

        switch (th_state) {
            case TH_APPEAR_1: th_anim_timer += io->DeltaTime; if (th_anim_timer >= TH_APPEAR_T1) { th_state = TH_APPEAR_2; th_anim_timer -= TH_APPEAR_T1; th_current_scale_mult = 1.1f; } else { th_current_scale_mult = (th_anim_timer / MAX_F(0.001f, TH_APPEAR_T1)) * 1.1f; } break;
            case TH_APPEAR_2: th_anim_timer += io->DeltaTime; if (th_anim_timer >= TH_APPEAR_T2) { th_state = TH_VISIBLE; th_anim_timer = 0.0f; th_current_scale_mult = 1.0f; } else { th_current_scale_mult = 1.1f - 0.1f * (th_anim_timer / MAX_F(0.001f, TH_APPEAR_T2)); } break;
            case TH_VISIBLE: th_current_scale_mult = 1.0f; break;
            case TH_DISAPPEAR_1: th_anim_timer += io->DeltaTime; if (th_anim_timer >= TH_DISAPPEAR_T1) { th_state = TH_DISAPPEAR_2; th_anim_timer -= TH_DISAPPEAR_T1; th_current_scale_mult = 1.1f; } else { th_current_scale_mult = 1.0f + 0.1f * (th_anim_timer / MAX_F(0.001f, TH_DISAPPEAR_T1)); } break;
            case TH_DISAPPEAR_2: th_anim_timer += io->DeltaTime; if (th_anim_timer >= TH_DISAPPEAR_T2) { th_state = TH_HIDDEN; th_anim_timer = 0.0f; th_current_scale_mult = 0.0f; } else { th_current_scale_mult = 1.1f * (1.0f - (th_anim_timer / MAX_F(0.001f, TH_DISAPPEAR_T2))); } break;
            case TH_HIDDEN: th_current_scale_mult = 0.0f; break;
        }

        anim_target_health += ((float)target_health - anim_target_health) * 10.0f * io->DeltaTime; if (is_hit) hit_timer += io->DeltaTime;

        if (mod_scaffold_enable) { for (int i = 0; i < 10; ++i) notifs[i].timer = 0.0f; }
        UpdateSpring(blockAnimMult, blockAnimVel, mod_scaffold_enable ? 1.0f : 0.0f, animSpeed, animBounciness, io->DeltaTime);
        if (mod_scaffold_enable) blockLinearAlpha += io->DeltaTime * 10.0f; else blockLinearAlpha -= io->DeltaTime * 10.0f;
        blockLinearAlpha = CLAMP_F(blockLinearAlpha, 0.0f, 1.0f);

        float overallExpandMult = blockAnimMult; bool hasAnyActiveTimer = false;
        for (int i = 0; i < 10; ++i) {
            if (!notifs[i].active) continue; notifs[i].timer -= io->DeltaTime;
            UpdateSpring(notifs[i].switchAnimPos, notifs[i].switchAnimVel, notifs[i].isEnable ? 1.0f : 0.0f, animSpeed, animBounciness, io->DeltaTime);
            UpdateSpring(notifs[i].heightMult, notifs[i].heightVel, (notifs[i].timer > 0.0f && !mod_scaffold_enable) ? 1.0f : 0.0f, animSpeed, animBounciness, io->DeltaTime);
            if (notifs[i].timer > 0.0f && !mod_scaffold_enable) { notifs[i].linearAlpha += io->DeltaTime * 10.0f; hasAnyActiveTimer = true; } else { notifs[i].linearAlpha -= io->DeltaTime * 10.0f; }
            notifs[i].linearAlpha = CLAMP_F(notifs[i].linearAlpha, 0.0f, 1.0f);
            if (notifs[i].heightMult > overallExpandMult) overallExpandMult = notifs[i].heightMult;
            if (notifs[i].timer <= 0.0f && notifs[i].heightMult < 0.005f && ABS_F(notifs[i].heightVel) < 0.01f && notifs[i].linearAlpha <= 0.001f) { notifs[i].active = false; notifs[i].heightMult = 0.0f; notifs[i].linearAlpha = 0.0f; continue; }
        }

        bool shouldShowOpal = (!mod_scaffold_enable && !hasAnyActiveTimer);
        if (shouldShowOpal) opalLinearAlpha += io->DeltaTime * 10.0f; else opalLinearAlpha -= io->DeltaTime * 10.0f;
        opalLinearAlpha = CLAMP_F(opalLinearAlpha, 0.0f, 1.0f);

        float s_iconSize = iconSize * anim_islandScale, s_textSize = textSize * anim_islandScale, s_padCollapsedX = padCollapsedX * anim_islandScale, s_padNormalX = padNormalX * anim_islandScale, s_padBlockX = padBlockX * anim_islandScale;
        float s_scaffoldBgSize = scaffoldBgSize * anim_islandScale, s_normalHeight = normalHeight * anim_islandScale, s_expandedHeight = expandedHeight * anim_islandScale, s_spacing = 15.0f * anim_islandScale, s_paddingY = 14.0f * anim_islandScale, s_switchScale = switchScale * anim_islandScale;

        float collapsedW = s_padCollapsedX * 2.0f + s_iconSize + s_spacing + font->CalcTextSizeA(s_textSize, 10000.0f, 0.0f, clientName).x + font->CalcTextSizeA(s_textSize, 10000.0f, 0.0f, " | Prodee163 | 120fps | 0ms").x;
        float expandedReqW = 0.0f; 
        if (mod_scaffold_enable || blockAnimMult > 0.001f || ABS_F(blockAnimVel) > 0.001f || blockLinearAlpha > 0.001f) {
            expandedReqW = MAX_F(expandedReqW, s_padBlockX * 2.0f + MAX_F(s_scaffoldBgSize + s_spacing + font->CalcTextSizeA(titleTextSize * anim_islandScale, 10000.0f, 0.0f, "Scaffold Toggle").x, 300.0f * anim_islandScale));
        }
        for (int i = 0; i < 10; ++i) {
            if (notifs[i].active && (notifs[i].heightMult > 0.001f || notifs[i].linearAlpha > 0.001f || ABS_F(notifs[i].heightVel) > 0.001f)) {
                const char* nTitleBuf = "Module Toggle"; char st1Buf[64]; snprintf(st1Buf, sizeof(st1Buf), "%s has been ", notifs[i].modName); const char* st2Str = notifs[i].isEnable ? "Enabled" : "Disabled";
                float txtW = MAX_F(font->CalcTextSizeA(titleTextSize * anim_islandScale, 10000.0f, 0.0f, nTitleBuf).x, font->CalcTextSizeA(statusTextSize * anim_islandScale, 10000.0f, 0.0f, st1Buf).x + font->CalcTextSizeA(statusTextSize * anim_islandScale, 10000.0f, 0.0f, st2Str).x);
                expandedReqW = MAX_F(expandedReqW, s_padNormalX * 2.0f + (50.0f * s_switchScale) + s_spacing + txtW);
            }
        }
        float currentTargetMaxWidth = (expandedReqW > 0.0f) ? expandedReqW : collapsedW;
        if (activeMaxWidth == 0.0f) activeMaxWidth = currentTargetMaxWidth;
        UpdateSpring(activeMaxWidth, activeMaxWidthVel, currentTargetMaxWidth, animSpeed, animBounciness, io->DeltaTime);

        // GUI 工具函数 (位置过渡更加平滑)
        auto AnimatedHoverText = [&](const char* label, float scale) -> bool {
            float currentTextSize = ImGui::GetFontSize() * scale * scaled_UI; 
            ImVec2 label_size = font->CalcTextSizeA(currentTextSize, 10000.0f, 0.0f, label); 
            ImVec2 size(ImGui::GetContentRegionAvail().x, label_size.y + 6.0f * scaled_UI); 
            ImVec2 target_pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(label, size); bool hovered = ImGui::IsItemHovered(); bool pressed = hovered && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 5.0f);
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float offset = storage->GetFloat(id, 0.0f); float targetOffset = hovered ? 15.0f * scaled_UI : 0.0f; offset += (targetOffset - offset) * 15.0f * io->DeltaTime; storage->SetFloat(id, offset);
            float cur_x = storage->GetFloat(id + 1, target_pos.x), cur_y = storage->GetFloat(id + 2, target_pos.y);
            if (ABS_F(cur_x - target_pos.x) > 500.0f || ABS_F(cur_y - target_pos.y) > 500.0f) { cur_x = target_pos.x; cur_y = target_pos.y; }
            cur_x += (target_pos.x - cur_x) * 20.0f * io->DeltaTime; cur_y += (target_pos.y - cur_y) * 20.0f * io->DeltaTime; storage->SetFloat(id + 1, cur_x); storage->SetFloat(id + 2, cur_y);
            ImGui::GetWindowDrawList()->AddText(font, currentTextSize, ImVec2(cur_x + offset, cur_y + 3.0f * scaled_UI), IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)), label); return pressed;
        };

        auto CustomButton = [&](const char* label, float overrideWidth) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float width = overrideWidth > 0.0f ? overrideWidth : (label_size.x + 30.0f * scaled_UI); ImVec2 size(width, label_size.y + 16.0f * scaled_UI); ImVec2 target_pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(label, size); bool hovered = ImGui::IsItemHovered(); bool pressed = hovered && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 5.0f);
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float hover_t = storage->GetFloat(id, 0.0f); hover_t += ((hovered ? 1.0f : 0.0f) - hover_t) * 15.0f * io->DeltaTime; storage->SetFloat(id, hover_t);
            float cur_x = storage->GetFloat(id + 1, target_pos.x), cur_y = storage->GetFloat(id + 2, target_pos.y);
            float cur_w = storage->GetFloat(id + 10, size.x), cur_h = storage->GetFloat(id + 11, size.y);
            if (ABS_F(cur_x - target_pos.x) > 500.0f || ABS_F(cur_y - target_pos.y) > 500.0f) { cur_x = target_pos.x; cur_y = target_pos.y; } if (ABS_F(cur_w - size.x) > 500.0f || ABS_F(cur_h - size.y) > 500.0f) { cur_w = size.x; cur_h = size.y; }
            cur_x += (target_pos.x - cur_x) * 20.0f * io->DeltaTime; cur_y += (target_pos.y - cur_y) * 20.0f * io->DeltaTime; cur_w += (size.x - cur_w) * 20.0f * io->DeltaTime; cur_h += (size.y - cur_h) * 20.0f * io->DeltaTime;
            storage->SetFloat(id + 1, cur_x); storage->SetFloat(id + 2, cur_y); storage->SetFloat(id + 10, cur_w); storage->SetFloat(id + 11, cur_h);
            ImVec2 draw_pos(cur_x, cur_y), draw_size(cur_w, cur_h); ImVec4 base_bg = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            ImVec4 bg_color = ImVec4(base_bg.x + hover_t * 0.25f, base_bg.y + hover_t * 0.25f, base_bg.z + hover_t * 0.25f, currentUIAlpha);
            ImGui::GetWindowDrawList()->AddRectFilled(draw_pos, ImVec2(draw_pos.x + draw_size.x, draw_pos.y + draw_size.y), ImGui::ColorConvertFloat4ToU32(bg_color), clickguiRounding);
            ImVec2 text_pos(draw_pos.x + (draw_size.x - label_size.x) * 0.5f, draw_pos.y + (draw_size.y - label_size.y) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, text_pos, IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)), label); return pressed;
        };

        auto CustomKeybind = [&](const char* label, int* key_ref) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float w = ImGui::GetContentRegionAvail().x; ImVec2 pos = ImGui::GetCursorScreenPos(); ImVec2 size(w, label_size.y + 16.0f * scaled_UI);
            ImGui::InvisibleButton(label, size);
            bool hovered = ImGui::IsItemHovered(); bool pressed = hovered && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 5.0f);
            if (pressed) { if (active_bind_target == key_ref) active_bind_target = nullptr; else active_bind_target = key_ref; }
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float hover_t = storage->GetFloat(id, 0.0f); hover_t += ((hovered ? 1.0f : 0.0f) - hover_t) * 15.0f * io->DeltaTime; storage->SetFloat(id, hover_t);
            float cur_x = storage->GetFloat(id + 11, pos.x), cur_y = storage->GetFloat(id + 12, pos.y); float cur_w = storage->GetFloat(id + 10, w);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; } if (ABS_F(cur_w - w) > 500.0f) cur_w = w;
            cur_x += (pos.x - cur_x) * 20.0f * io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * io->DeltaTime; cur_w += (w - cur_w) * 20.0f * io->DeltaTime;
            storage->SetFloat(id + 11, cur_x); storage->SetFloat(id + 12, cur_y); storage->SetFloat(id + 10, cur_w);
            ImVec2 draw_pos(cur_x, cur_y); ImVec4 base_bg = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
            ImVec4 bg_color = ImVec4(base_bg.x + hover_t * 0.15f, base_bg.y + hover_t * 0.15f, base_bg.z + hover_t * 0.15f, currentUIAlpha);
            ImGui::GetWindowDrawList()->AddRectFilled(draw_pos, ImVec2(draw_pos.x + cur_w, draw_pos.y + size.y), ImGui::ColorConvertFloat4ToU32(bg_color), clickguiRounding);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + 10.0f * scaled_UI, draw_pos.y + 8.0f * scaled_UI), IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)), label);
            std::string bind_str;
            if (active_bind_target == key_ref) bind_str = "Listening...";
            else bind_str = (*key_ref == 0) ? "None" : ("Key " + std::to_string(*key_ref)); 
            ImVec2 val_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, bind_str.c_str());
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + cur_w - val_size.x - 10.0f * scaled_UI, draw_pos.y + 8.0f * scaled_UI), IM_COL32(200, 200, 200, (int)(currentUIAlpha * 255)), bind_str.c_str());
            return pressed;
        };

        auto ModuleButton = [&](const char* label, bool* v, bool* settingsClicked, const char* modNameLog) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            ImVec2 size(label_size.x + 45.0f * scaled_UI, label_size.y + 20.0f * scaled_UI); ImVec2 target_pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(label, size); bool hovered = ImGui::IsItemHovered(); bool pressed = hovered && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 5.0f);
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float hover_t = storage->GetFloat(id, 0.0f); hover_t += ((hovered ? 1.0f : 0.0f) - hover_t) * 15.0f * io->DeltaTime; storage->SetFloat(id, hover_t);
            float cur_x = storage->GetFloat(id + 1, target_pos.x), cur_y = storage->GetFloat(id + 2, target_pos.y);
            float cur_w = storage->GetFloat(id + 10, size.x), cur_h = storage->GetFloat(id + 11, size.y);
            if (ABS_F(cur_x - target_pos.x) > 500.0f || ABS_F(cur_y - target_pos.y) > 500.0f) { cur_x = target_pos.x; cur_y = target_pos.y; } if (ABS_F(cur_w - size.x) > 500.0f || ABS_F(cur_h - size.y) > 500.0f) { cur_w = size.x; cur_h = size.y; }
            cur_x += (target_pos.x - cur_x) * 20.0f * io->DeltaTime; cur_y += (target_pos.y - cur_y) * 20.0f * io->DeltaTime; cur_w += (size.x - cur_w) * 20.0f * io->DeltaTime; cur_h += (size.y - cur_h) * 20.0f * io->DeltaTime;
            storage->SetFloat(id + 1, cur_x); storage->SetFloat(id + 2, cur_y); storage->SetFloat(id + 10, cur_w); storage->SetFloat(id + 11, cur_h);
            ImVec2 draw_pos(cur_x, cur_y), draw_size(cur_w, cur_h);
            
            ImVec4 activeColor = ImVec4(themeColors[0].x, themeColors[0].y, themeColors[0].z, 1.0f);
            ImVec4 base_bg = *v ? activeColor : ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            ImVec4 bg_color = ImVec4(base_bg.x + hover_t * 0.25f, base_bg.y + hover_t * 0.25f, base_bg.z + hover_t * 0.25f, currentUIAlpha);
            
            ImGui::GetWindowDrawList()->AddRectFilled(draw_pos, ImVec2(draw_pos.x + draw_size.x, draw_pos.y + draw_size.y), ImGui::ColorConvertFloat4ToU32(bg_color), clickguiRounding);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + 15.0f * scaled_UI, draw_pos.y + (draw_size.y - label_size.y) * 0.5f), IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)), label);
            ImVec2 arrow_min(target_pos.x + size.x - 22.0f * scaled_UI, target_pos.y), arrow_max(target_pos.x + size.x, target_pos.y + size.y); bool arrow_hovered = (io->MousePos.x >= arrow_min.x && io->MousePos.x <= arrow_max.x && io->MousePos.y >= arrow_min.y && io->MousePos.y <= arrow_max.y);
            if (pressed) { if (arrow_hovered) *settingsClicked = true; else { *v = !*v; if (std::string(modNameLog) != "Scaffold" && std::string(modNameLog) != "ClickGui") TriggerNotification(modNameLog, *v); } }
            float cx = draw_pos.x + draw_size.x - 13.0f * scaled_UI, cy = draw_pos.y + draw_size.y * 0.5f;
            ImVec2 p1(cx - 3.0f * scaled_UI, cy - 4.0f * scaled_UI), p2(cx + 3.0f * scaled_UI, cy), p3(cx - 3.0f * scaled_UI, cy + 4.0f * scaled_UI);
            ImU32 arrowCol = arrow_hovered ? IM_COL32(200, 200, 255, (int)(currentUIAlpha * 255)) : IM_COL32(150, 150, 150, (int)(currentUIAlpha * 255));
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
            float text_anim_t = storage->GetFloat(id + 21, 1.0f); if (text_anim_t < 1.0f) { text_anim_t += 10.0f * io->DeltaTime; if (text_anim_t > 1.0f) text_anim_t = 1.0f; } storage->SetFloat(id + 21, text_anim_t);
            int fade_out_idx = storage->GetInt(id + 22, *current_item);
            float hover_t = storage->GetFloat(id, 0.0f); hover_t += ((hovered ? 1.0f : 0.0f) - hover_t) * 15.0f * io->DeltaTime; storage->SetFloat(id, hover_t);
            float cur_x = storage->GetFloat(id + 1, pos.x), cur_y = storage->GetFloat(id + 2, pos.y);
            float cur_w = storage->GetFloat(id + 10, size.x), cur_h = storage->GetFloat(id + 11, size.y);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; } if (ABS_F(cur_w - size.x) > 500.0f || ABS_F(cur_h - size.y) > 500.0f) { cur_w = size.x; cur_h = size.y; }
            cur_x += (pos.x - cur_x) * 20.0f * io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * io->DeltaTime; cur_w += (size.x - cur_w) * 20.0f * io->DeltaTime; cur_h += (size.y - cur_h) * 20.0f * io->DeltaTime;
            storage->SetFloat(id + 1, cur_x); storage->SetFloat(id + 2, cur_y); storage->SetFloat(id + 10, cur_w); storage->SetFloat(id + 11, cur_h);
            ImVec4 base_bg = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); ImVec4 bg_color = ImVec4(base_bg.x + hover_t * 0.25f, base_bg.y + hover_t * 0.25f, base_bg.z + hover_t * 0.25f, currentUIAlpha);
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(cur_x, cur_y), ImVec2(cur_x + cur_w, cur_y + cur_h), ImGui::ColorConvertFloat4ToU32(bg_color), clickguiRounding);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(cur_x + 15.0f * scaled_UI, cur_y + (cur_h - label_size.y) * 0.5f), IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)), label);
            ImGui::GetWindowDrawList()->PushClipRect(ImVec2(cur_x, cur_y), ImVec2(cur_x + cur_w, cur_y + cur_h), true);
            if (text_anim_t < 1.0f) {
                ImVec2 fade_val_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, items[fade_out_idx]);
                float out_y = cur_y + (cur_h - fade_val_size.y) * 0.5f + (text_anim_t * 15.0f * scaled_UI);
                ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(cur_x + cur_w - fade_val_size.x - 15.0f * scaled_UI, out_y), IM_COL32(200, 200, 200, (int)(currentUIAlpha * 255.0f * (1.0f - text_anim_t))), items[fade_out_idx]);
            }
            float in_y = cur_y + (cur_h - val_size.y) * 0.5f - ((1.0f - text_anim_t) * 15.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(cur_x + cur_w - val_size.x - 15.0f * scaled_UI, in_y), IM_COL32(200, 200, 200, (int)(currentUIAlpha * 255.0f * text_anim_t)), items[*current_item]);
            ImGui::GetWindowDrawList()->PopClipRect(); return pressed;
        };

        auto CustomSliderFloat = [&](const char* label, float* v, float v_min, float v_max, const char* fmt = "%.2f") -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float w = ImGui::GetContentRegionAvail().x; ImVec2 pos = ImGui::GetCursorScreenPos(); ImVec2 size(w, label_size.y + 16.0f * scaled_UI); 
            ImGui::InvisibleButton(label, size); bool active = ImGui::IsItemActive(); ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float cur_x = storage->GetFloat(id + 11, pos.x), cur_y = storage->GetFloat(id + 12, pos.y); float cur_w = storage->GetFloat(id + 10, w);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; } if (ABS_F(cur_w - w) > 500.0f) cur_w = w;
            cur_x += (pos.x - cur_x) * 20.0f * io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * io->DeltaTime; cur_w += (w - cur_w) * 20.0f * io->DeltaTime;
            storage->SetFloat(id + 11, cur_x); storage->SetFloat(id + 12, cur_y); storage->SetFloat(id + 10, cur_w);
            ImVec2 draw_pos(cur_x, cur_y), track_min(draw_pos.x, draw_pos.y + label_size.y + 6.0f * scaled_UI), track_max(draw_pos.x + cur_w, draw_pos.y + label_size.y + 10.0f * scaled_UI);
            if (active) { float mouse_x = io->MousePos.x; float new_t = CLAMP_F((mouse_x - track_min.x) / cur_w, 0.0f, 1.0f); *v = v_min + new_t * (v_max - v_min); }
            float target_t = (*v - v_min) / (v_max - v_min); float current_t = storage->GetFloat(id, target_t); current_t += (target_t - current_t) * 15.0f * io->DeltaTime; storage->SetFloat(id, current_t);
            char val_buf[32]; snprintf(val_buf, sizeof(val_buf), fmt, *v); ImVec2 val_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, val_buf);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, draw_pos, IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)), label);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + cur_w - val_size.x, draw_pos.y), IM_COL32(200, 200, 200, (int)(currentUIAlpha * 255)), val_buf);
            ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(themeColors[0].x, themeColors[0].y, themeColors[0].z, currentUIAlpha));
            ImGui::GetWindowDrawList()->AddRectFilled(track_min, track_max, IM_COL32(50, 50, 50, (int)(currentUIAlpha * 255)), 2.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddRectFilled(track_min, ImVec2(track_min.x + current_t * cur_w, track_max.y), fillCol, 2.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(track_min.x + current_t * cur_w, track_min.y + 2.0f * scaled_UI), 6.0f * scaled_UI, IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)));
            return active;
        };
        
        auto CustomSliderInt = [&](const char* label, int* v, int v_min, int v_max) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI; ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float w = ImGui::GetContentRegionAvail().x; ImVec2 pos = ImGui::GetCursorScreenPos(); ImVec2 size(w, label_size.y + 16.0f * scaled_UI); 
            ImGui::InvisibleButton(label, size); bool active = ImGui::IsItemActive(); ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float cur_x = storage->GetFloat(id + 11, pos.x), cur_y = storage->GetFloat(id + 12, pos.y); float cur_w = storage->GetFloat(id + 10, w);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; } if (ABS_F(cur_w - w) > 500.0f) cur_w = w;
            cur_x += (pos.x - cur_x) * 20.0f * io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * io->DeltaTime; cur_w += (w - cur_w) * 20.0f * io->DeltaTime;
            storage->SetFloat(id + 11, cur_x); storage->SetFloat(id + 12, cur_y); storage->SetFloat(id + 10, cur_w);
            ImVec2 draw_pos(cur_x, cur_y), track_min(draw_pos.x, draw_pos.y + label_size.y + 6.0f * scaled_UI), track_max(draw_pos.x + cur_w, draw_pos.y + label_size.y + 10.0f * scaled_UI);
            if (active) { float mouse_x = io->MousePos.x; float new_t = CLAMP_F((mouse_x - track_min.x) / cur_w, 0.0f, 1.0f); *v = (int)CLAMP_F(v_min + new_t * (v_max - v_min) + 0.5f, (float)v_min, (float)v_max); }
            float target_t = (float)(*v - v_min) / (v_max - v_min); float current_t = storage->GetFloat(id, target_t); current_t += (target_t - current_t) * 15.0f * io->DeltaTime; storage->SetFloat(id, current_t);
            char val_buf[32]; snprintf(val_buf, sizeof(val_buf), "%d", *v); ImVec2 val_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, val_buf);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, draw_pos, IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)), label);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, ImVec2(draw_pos.x + cur_w - val_size.x, draw_pos.y), IM_COL32(200, 200, 200, (int)(currentUIAlpha * 255)), val_buf);
            ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(themeColors[0].x, themeColors[0].y, themeColors[0].z, currentUIAlpha));
            ImGui::GetWindowDrawList()->AddRectFilled(track_min, track_max, IM_COL32(50, 50, 50, (int)(currentUIAlpha * 255)), 2.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddRectFilled(track_min, ImVec2(track_min.x + current_t * cur_w, track_max.y), fillCol, 2.0f * scaled_UI);
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(track_min.x + current_t * cur_w, track_min.y + 2.0f * scaled_UI), 6.0f * scaled_UI, IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)));
            return active;
        };

        auto CustomInputText = [&](const char* label, char* buf, size_t buf_size) -> bool {
            float text_sz = ImGui::GetFontSize() * scaled_UI;
            ImVec2 label_size = font->CalcTextSizeA(text_sz, 10000.0f, 0.0f, label);
            float w = ImGui::GetContentRegionAvail().x; ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size(w, label_size.y + 35.0f * scaled_UI); 
            
            ImGui::InvisibleButton(label, size);
            ImGuiID id = ImGui::GetID(label); ImGuiStorage* storage = ImGui::GetStateStorage();
            float cur_x = storage->GetFloat(id + 11, pos.x), cur_y = storage->GetFloat(id + 12, pos.y);
            float cur_w = storage->GetFloat(id + 10, w);
            if (ABS_F(cur_x - pos.x) > 500.0f || ABS_F(cur_y - pos.y) > 500.0f) { cur_x = pos.x; cur_y = pos.y; }
            if (ABS_F(cur_w - w) > 500.0f) cur_w = w;
            cur_x += (pos.x - cur_x) * 20.0f * io->DeltaTime; cur_y += (pos.y - cur_y) * 20.0f * io->DeltaTime; cur_w += (w - cur_w) * 20.0f * io->DeltaTime;
            storage->SetFloat(id + 11, cur_x); storage->SetFloat(id + 12, cur_y); storage->SetFloat(id + 10, cur_w);

            ImVec2 draw_pos(cur_x, cur_y);
            ImGui::GetWindowDrawList()->AddText(font, text_sz, draw_pos, IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255)), label);
            
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x, draw_pos.y + label_size.y + 5.0f * scaled_UI));
            ImGui::PushItemWidth(cur_w);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, clickguiRounding);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.1f, currentUIAlpha));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,currentUIAlpha));
            
            char hidden_label[128]; snprintf(hidden_label, sizeof(hidden_label), "##%s", label);
            bool ret = ImGui::InputText(hidden_label, buf, buf_size);
            
            ImGui::PopStyleColor(2); ImGui::PopStyleVar(); ImGui::PopItemWidth();
            return ret;
        };

        if (globalGuiAlpha > 0.001f) {
            if (isTransitioning) { viewAlpha -= io->DeltaTime * 10.0f; if (viewAlpha <= 0.0f) { viewAlpha = 0.0f; currentState = nextState; isTransitioning = false; } } 
            else { if (viewAlpha < 1.0f) { viewAlpha += io->DeltaTime * 10.0f; if (viewAlpha > 1.0f) viewAlpha = 1.0f; } }

            if (!isDraggingResizer) {
                if (currentState == STATE_CATEGORIES) { 
                    float item_h = font->CalcTextSizeA(ImGui::GetFontSize() * 0.75f * scaled_UI, 10000.0f, 0.0f, "A").y + 6.0f * scaled_UI;
                    targetGuiWidth = 250.0f * scaled_UI; 
                    targetGuiHeight = 110.0f * scaled_UI + (item_h * 6.0f) + 60.0f * scaled_UI; 
                } 
                else { targetGuiWidth = savedExpandedWidth; targetGuiHeight = savedExpandedHeight; }
            }
            if (isDraggingResizer) { currentGuiWidth = targetGuiWidth; currentGuiHeight = targetGuiHeight; } 
            else { currentGuiWidth += (targetGuiWidth - currentGuiWidth) * 15.0f * io->DeltaTime; currentGuiHeight += (targetGuiHeight - currentGuiHeight) * 15.0f * io->DeltaTime; }

            ImGui::SetNextWindowPos(guiBasePos, ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(currentGuiWidth, currentGuiHeight), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); 
            
            ImGui::Begin("Fate_ClickGui", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            input->g_window = g_window = ImGui::GetCurrentWindow(); 
            ImDrawList* guiDraw = ImGui::GetWindowDrawList(); ImVec2 guiPos = ImGui::GetWindowPos();

            guiDraw->PushClipRectFullScreen();
            if (shadowSize > 0.0f && shadowAlpha > 0.0f) {
                int steps = 30; float strokeThickness = (shadowSize / steps) * 1.5f;
                for (int i = 1; i <= steps; i++) {
                    float t = (float)i / steps; float alpha = shadowAlpha * globalGuiAlpha * (1.0f - t) * (1.0f - t); float expand = t * shadowSize;
                    guiDraw->AddRect(ImVec2(guiPos.x - expand, guiPos.y - expand), ImVec2(guiPos.x + currentGuiWidth + expand, guiPos.y + currentGuiHeight + expand), 
                        IM_COL32(0, 0, 0, (int)(alpha * 255.0f)), clickguiRounding + expand, 0, strokeThickness);
                }
            }
            guiDraw->PopClipRect();
            guiDraw->AddRectFilled(guiPos, ImVec2(guiPos.x + currentGuiWidth, guiPos.y + currentGuiHeight), IM_COL32(0, 0, 0, (int)(bgAlpha * globalGuiAlpha * 255.0f)), clickguiRounding);

            static ImVec2 dragOffset; ImVec2 titleBar_min = guiPos, titleBar_max = ImVec2(guiPos.x + currentGuiWidth, guiPos.y + 70.0f * scaled_UI);
            ImVec2 mousePos = io->MousePos; bool title_hovered = (mousePos.x >= titleBar_min.x && mousePos.x <= titleBar_max.x && mousePos.y >= titleBar_min.y && mousePos.y <= titleBar_max.y);
            if (title_hovered && ImGui::IsMouseClicked(0)) { isDraggingWindow = true; dragOffset = ImVec2(mousePos.x - guiBasePos.x, mousePos.y - guiBasePos.y); }
            if (!ImGui::IsMouseDown(0)) isDraggingWindow = false;
            if (isDraggingWindow) { guiBasePos.x = mousePos.x - dragOffset.x; guiBasePos.y = mousePos.y - dragOffset.y; }

            currentUIAlpha = 1.0f * globalGuiAlpha;

            if (currentState == STATE_CATEGORIES) {
                // 修改点：Title 大小受到 scaled_UI 影响
                float scaledTitleSize = ImGui::GetFontSize() * 1.75f * scaled_UI; 
                ImVec2 fateSize = font->CalcTextSizeA(scaledTitleSize, 10000.0f, 0.0f, clientName);
                ImU32 titleCol = IM_COL32(255, 255, 255, (int)(currentUIAlpha * 255.0f));
                guiDraw->AddText(font, scaledTitleSize, ImVec2(guiPos.x + (currentGuiWidth - fateSize.x) * 0.5f, guiPos.y + 15.0f * scaled_UI), titleCol, clientName);
            } else {
                float dirTextSize = ImGui::GetFontSize() * 1.2f * scaled_UI; std::string dirStr;
                if (currentState == STATE_MODULES) { dirStr = clickguiLanguage == 0 ? currentCategory : getCatCn(currentCategory); } 
                else if (currentState == STATE_SETTINGS) { dirStr = std::string(clickguiLanguage == 0 ? currentCategory : getCatCn(currentCategory)) + " / " + (clickguiLanguage == 0 ? currentModule : getModCn(currentModule)); }
                guiDraw->AddText(font, dirTextSize, ImVec2(guiPos.x + 20.0f * scaled_UI, guiPos.y + 25.0f * scaled_UI), IM_COL32(230, 230, 230, (int)(currentUIAlpha * 255.0f)), dirStr.c_str());
                const char* backLabel = tr("< Back", "< 返回"); float backBtnW = font->CalcTextSizeA(ImGui::GetFontSize() * scaled_UI, 10000.0f, 0.0f, backLabel).x + 30.0f * scaled_UI;
                ImGui::SetCursorPos(ImVec2(currentGuiWidth - backBtnW - 10.0f * scaled_UI, 15.0f * scaled_UI)); 
                if (CustomButton(backLabel, 0.0f)) { if (currentState == STATE_MODULES) nextState = STATE_CATEGORIES; else if (currentState == STATE_SETTINGS) nextState = STATE_MODULES; isTransitioning = true; }
            }
            guiDraw->AddLine(ImVec2(guiPos.x + 20.0f * scaled_UI, guiPos.y + 70.0f * scaled_UI), ImVec2(guiPos.x + currentGuiWidth - 20.0f * scaled_UI, guiPos.y + 70.0f * scaled_UI), IM_COL32(100, 100, 100, (int)(bgAlpha * globalGuiAlpha * 150.0f)), 1.0f);

            currentUIAlpha = globalGuiAlpha * viewAlpha; ImGui::SetCursorPos(ImVec2(20.0f * scaled_UI, 80.0f * scaled_UI));
            bool isCat = (currentState == STATE_CATEGORIES); float scroll_y = 0.0f, scroll_max_y = 0.0f, visible_h = 0.0f;

            ImGui::BeginChild("GuiScrollRegion", ImVec2(currentGuiWidth - 40.0f * scaled_UI, currentGuiHeight - 100.0f * scaled_UI), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
            
            if (!isCat && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0, 3.0f)) { 
                ImGui::SetScrollY(ImGui::GetScrollY() - io->MouseDelta.y); 
            }

            if (currentState == STATE_CATEGORIES) {
                const char* catsEN[] = {"Combat", "Player", "Misc", "World", "Visual", "Other"};
                const char* catsCN[] = {"战斗", "玩家", "杂项", "世界", "视觉", "其他"};
                for (int i = 0; i < 6; ++i) {
                    ImGui::PushID(i);
                    if (AnimatedHoverText(clickguiLanguage == 0 ? catsEN[i] : catsCN[i], 0.75f)) { currentCategory = catsEN[i]; nextState = STATE_MODULES; isTransitioning = true; }
                    ImGui::PopID();
                }
            } 
            else if (currentState == STATE_MODULES) {
                float rightBound = guiPos.x + currentGuiWidth - 20.0f * scaled_UI; 
                if (currentCategory == "Visual") {
                    ModItem mods[] = { 
                        ModItem(tr("ClickGui", "菜单显示"), &showClickGui, "ClickGui", &key_clickgui), 
                        ModItem(tr("Island", "灵动岛"), &mod_island_enable, "Island", &key_island), 
                        ModItem(tr("Interface", "界面"), &mod_interface_enable, "Interface", &key_interface), 
                        ModItem(tr("Target HUD", "目标信息"), &mod_targethud_enable, "Targethud", &key_targethud), 
                        ModItem(tr("ArrayList", "列表"), &mod_arraylist_enable, "ArrayList", &key_arraylist) 
                    };
                    for (int i = 0; i < 5; ++i) {
                        float next_w = font->CalcTextSizeA(ImGui::GetFontSize() * scaled_UI, 10000.0f, 0.0f, mods[i].label).x + 45.0f * scaled_UI;
                        if (i > 0) { float last_x2 = ImGui::GetItemRectMax().x; if (last_x2 + 15.0f * scaled_UI + next_w < rightBound) ImGui::SameLine(0.0f, 15.0f * scaled_UI); else ImGui::Spacing(); }
                        bool settingsClicked = false;
                        if (ModuleButton(mods[i].label, mods[i].state, &settingsClicked, mods[i].modName.c_str())) { if (settingsClicked) { currentModule = mods[i].modName; nextState = STATE_SETTINGS; isTransitioning = true; } }
                    }
                } 
                else if (currentCategory == "Player") {
                    ModItem mods[] = { ModItem(tr("Scaffold", "脚手架"), &mod_scaffold_enable, "Scaffold", &key_scaffold) };
                    for (int i = 0; i < 1; ++i) {
                        bool settingsClicked = false;
                        if (ModuleButton(mods[i].label, mods[i].state, &settingsClicked, mods[i].modName.c_str())) { if (settingsClicked) { currentModule = mods[i].modName; nextState = STATE_SETTINGS; isTransitioning = true; } }
                    }
                }
                else if (currentCategory == "Other") {
                    // 新增：悬浮窗配置模块入口
                    static bool dummy_state = true;
                    ModItem mods[] = { ModItem(tr("Float Config", "悬浮窗配置"), &dummy_state, "FloatConfig", &key_island /*dummy*/) };
                    for (int i = 0; i < 1; ++i) {
                        bool settingsClicked = false;
                        if (ModuleButton(mods[i].label, mods[i].state, &settingsClicked, mods[i].modName.c_str())) { if (settingsClicked) { currentModule = mods[i].modName; nextState = STATE_SETTINGS; isTransitioning = true; } }
                    }
                } else { ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, currentUIAlpha), "%s", tr("No modules yet.", "此分类暂无模块")); }
            }
            else if (currentState == STATE_SETTINGS) {
                if (currentCategory == "Visual" && currentModule == "ClickGui") {
                    CustomKeybind(tr("Keybind", "快捷键"), &key_clickgui); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(al_show_clickgui ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_clickgui = !al_show_clickgui;
                    if (CustomButton(show_fb_clickgui ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_clickgui = !show_fb_clickgui;
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Menu Scale", "菜单整体缩放"), &clickgui_scale, 0.5f, 2.5f, "%.2f"); 
                }
                else if (currentCategory == "Visual" && currentModule == "Island") {
                    CustomKeybind(tr("Keybind", "快捷键"), &key_island); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(al_show_island ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_island = !al_show_island;
                    if (CustomButton(show_fb_island ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_island = !show_fb_island;
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Island Scale", "全局缩放"), &islandScale, 0.3f, 3.0f, "%.2f"); CustomSliderFloat(tr("Offset X", "X轴偏移"), &islandOffsetX, -((float)this->surfaceWidth), (float)this->surfaceWidth, "%.0f"); CustomSliderFloat(tr("Offset Y", "Y轴偏移"), &islandOffsetY, 0.0f, (float)this->surfaceHigh, "%.0f");
                } 
                else if (currentCategory == "Visual" && currentModule == "Interface") {
                    CustomKeybind(tr("Keybind", "快捷键"), &key_interface); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(al_show_interface ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_interface = !al_show_interface;
                    if (CustomButton(show_fb_interface ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_interface = !show_fb_interface;
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomInputText(tr("Client Name", "客户端名称"), clientName, IM_ARRAYSIZE(clientName));
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Bg Alpha", "背景透明度"), &bgAlpha, 0.0f, 1.0f, "%.2f"); CustomSliderFloat(tr("Shadow Alpha", "阴影透明度"), &shadowAlpha, 0.0f, 1.0f, "%.2f"); CustomSliderFloat(tr("Overall Rounding", "全局圆角大小"), &clickguiRounding, 0.0f, 40.0f, "%.2f");
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    const char* langOptsEN[] = {"English", "Chinese"}; const char* langOptsCN[] = {"英文", "中文"};
                    CustomCombo(tr("Language", "界面语言"), &clickguiLanguage, clickguiLanguage == 0 ? langOptsEN : langOptsCN, 2);
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(enableWave ? tr("Disable Wave", "关闭彩虹波动") : tr("Enable Wave", "启用彩虹波动"), 0.0f)) { enableWave = !enableWave; }
                    CustomSliderFloat(tr("Wave Speed", "波动速度"), &waveSpeed, 0.1f, 10.0f, "%.2f"); CustomCombo(tr("Theme", "主题预设"), &currentThemeIndex, themeNames, presetCount);
                }
                else if (currentCategory == "Visual" && currentModule == "Targethud") {
                    CustomKeybind(tr("Keybind", "快捷键"), &key_targethud); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(al_show_targethud ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_targethud = !al_show_targethud;
                    if (CustomButton(show_fb_targethud ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_targethud = !show_fb_targethud;
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(tr("Simulate Hit", "触发受击动画"), 0.0f)) {
                        hit_timer = 0.0f; is_hit = true; int range = (int)(15.0f - 10.0f + 1.0f); int p_count = (int)10.0f + (rand() % MAX_F(1, range));
                        for(int i=0; i<p_count; i++) {
                            TParticle p; p.angle = (rand() % 360) * 3.14159f / 180.0f; p.target_dist = 50.0f + (rand() % 100 / 100.0f) * (80.0f - 50.0f);
                            p.size = 2.0f + (rand() % 100 / 100.0f) * (3.0f - 2.0f); p.life = 0.0f; p.max_life = p.target_dist / MAX_F(1.0f, 100.0f);
                            p.linger_time = 0.4f; p.fade_time = 0.3f; th_particles.push_back(p);
                        }
                    }
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    const char* modes[] = {"Fate", "Rhythm"}; CustomCombo(tr("TH Mode", "样式"), &th_mode, modes, 2);
                    
                    // Rhythm 模式去除了缩放设置，以固定默认值为准

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Self Health", "自身血量"), &self_health, 0.0f, 20.0f, "%.0f"); CustomSliderInt(tr("Target Health", "目标血量"), &target_health, 0, 20);
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Scale", "全局缩放比例"), &th_scale, 0.5f, 3.0f, "%.2f"); CustomSliderFloat(tr("Offset X", "全局X轴偏移"), &th_offsetX, -((float)this->surfaceWidth), (float)this->surfaceWidth, "%.0f"); CustomSliderFloat(tr("Offset Y", "全局Y轴偏移"), &th_offsetY, -((float)this->surfaceHigh), (float)this->surfaceHigh, "%.0f");
                }
                else if (currentCategory == "Visual" && currentModule == "ArrayList") {
                    CustomKeybind(tr("Keybind", "快捷键"), &key_arraylist); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(al_show_arraylist ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_arraylist = !al_show_arraylist;
                    if (CustomButton(show_fb_arraylist ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_arraylist = !show_fb_arraylist;
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    
                    float rightBound = guiPos.x + currentGuiWidth - 20.0f * scaled_UI;
                    float w1 = font->CalcTextSizeA(ImGui::GetFontSize() * scaled_UI, 10000.0f, 0.0f, al_no_bg ? "No Background: ON" : "No Background: OFF").x + 30.0f * scaled_UI;
                    (void)w1;
                    if (CustomButton(al_no_bg ? "No Background: ON" : "No Background: OFF", 0.0f)) al_no_bg = !al_no_bg;
                    
                    float w2 = font->CalcTextSizeA(ImGui::GetFontSize() * scaled_UI, 10000.0f, 0.0f, al_no_shadow ? "No Shadow: ON" : "No Shadow: OFF").x + 30.0f * scaled_UI;
                    if (ImGui::GetItemRectMax().x + 15.0f * scaled_UI + w2 < rightBound) ImGui::SameLine(0.0f, 15.0f * scaled_UI); else ImGui::Spacing();
                    if (CustomButton(al_no_shadow ? "No Shadow: ON" : "No Shadow: OFF", 0.0f)) al_no_shadow = !al_no_shadow;
                    
                    float w3 = font->CalcTextSizeA(ImGui::GetFontSize() * scaled_UI, 10000.0f, 0.0f, al_no_line ? "No Line: ON" : "No Line: OFF").x + 30.0f * scaled_UI;
                    if (ImGui::GetItemRectMax().x + 15.0f * scaled_UI + w3 < rightBound) ImGui::SameLine(0.0f, 15.0f * scaled_UI); else ImGui::Spacing();
                    if (CustomButton(al_no_line ? "No Line: ON" : "No Line: OFF", 0.0f)) al_no_line = !al_no_line;

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(al_glow_enable ? tr("Text Blur Glow: ON", "高斯模糊辉光: 开") : tr("Text Blur Glow: OFF", "高斯模糊辉光: 关"), 0.0f)) al_glow_enable = !al_glow_enable;
                    if (al_glow_enable) {
                        CustomSliderFloat(tr("Glow Alpha", "辉光透明度"), &al_glow_alpha, 0.0f, 1.0f, "%.2f");
                        CustomSliderFloat(tr("Glow Radius", "模糊半径"), &al_glow_intensity, 0.0f, 10.0f, "%.1f");
                    }
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Scale", "全局缩放比例"), &al_scale, 0.5f, 2.0f, "%.2f"); CustomSliderFloat(tr("Offset X", "水平偏移"), &al_offsetX, -((float)this->surfaceWidth), (float)this->surfaceWidth, "%.0f"); CustomSliderFloat(tr("Offset Y", "垂直偏移"), &al_offsetY, 0.0f, (float)this->surfaceHigh, "%.0f");
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Module Spacing", "模块间隔"), &al_spacing, -5.0f, 10.0f, "%.1f"); CustomSliderFloat(tr("Line Width", "线条宽度"), &al_line_width, 1.0f, 10.0f, "%.1f"); CustomSliderFloat(tr("Line Height Mult", "线条高度倍率"), &al_line_height_mult, 0.1f, 2.0f, "%.2f");
                    CustomSliderFloat(tr("Line Offset X", "线条X轴调节"), &al_line_offsetX, -50.0f, 50.0f, "%.1f"); 
                    CustomSliderFloat(tr("Bg Overlap Fix", "背景黑线重叠修复"), &al_bg_overlap, -5.0f, 5.0f, "%.2f");
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(al_shadow_chroma ? tr("Shadow Chroma: ON", "阴影波浪变色: 开") : tr("Shadow Chroma: OFF", "阴影波浪变色: 关"), 0.0f)) { al_shadow_chroma = !al_shadow_chroma; }
                    CustomSliderFloat(tr("Bottom Shadow Expand", "底部阴影扩展量"), &al_shadow_size, -50.0f, 40.0f, "%.1f"); 
                    if (al_shadow_chroma) { CustomSliderFloat(tr("Chroma Shadow Alpha", "变色阴影透明度"), &al_chroma_shadow_alpha, 0.0f, 1.0f, "%.2f"); }
                }
                else if (currentCategory == "Player" && currentModule == "Scaffold") {
                    CustomKeybind(tr("Keybind", "快捷键"), &key_scaffold); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(al_show_scaffold ? tr("Show in ArrayList: ON", "在列表显示: 开") : tr("Show in ArrayList: OFF", "在列表显示: 关"), 0.0f)) al_show_scaffold = !al_show_scaffold;
                    if (CustomButton(show_fb_scaffold ? tr("Float Window: ON", "快捷悬浮窗: 开") : tr("Float Window: OFF", "快捷悬浮窗: 关"), 0.0f)) show_fb_scaffold = !show_fb_scaffold;
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Blocks Left", "剩余方块"), &blockCount, 0.0f, 64.0f, "%.0f");
                }
                else if (currentCategory == "Other" && currentModule == "FloatConfig") { // 悬浮窗配置菜单
                    if (CustomButton(tr("Reset All Positions", "重置所有悬浮窗位置"), 0.0f)) { fb_reset_pos = true; }
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Scale", "全局缩放"), &fb_scale, 0.5f, 3.0f, "%.2f");
                    CustomSliderFloat(tr("Font Size", "字体大小"), &fb_font_size, 10.0f, 50.0f, "%.1f");
                    CustomSliderFloat(tr("Rounding", "圆角大小"), &fb_rounding, 0.0f, 50.0f, "%.1f");
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (CustomButton(fb_chroma ? tr("Text Chroma: ON", "流光文字: 开") : tr("Text Chroma: OFF", "流光文字: 关"), 0.0f)) fb_chroma = !fb_chroma;
                    CustomSliderFloat(tr("Text Alpha (ON)", "开启时文字透明度"), &fb_text_alpha_on, 0.0f, 1.0f, "%.2f");
                    CustomSliderFloat(tr("Text Alpha (OFF)", "关闭时文字透明度"), &fb_text_alpha_off, 0.0f, 1.0f, "%.2f");
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    CustomSliderFloat(tr("Background Alpha", "背景透明度"), &fb_bg_alpha, 0.0f, 1.0f, "%.2f");
                    CustomSliderFloat(tr("Shadow Size", "阴影范围"), &fb_shadow_size, 0.0f, 50.0f, "%.1f");
                    CustomSliderFloat(tr("Shadow Alpha", "阴影透明度"), &fb_shadow_alpha, 0.0f, 1.0f, "%.2f");
                }
            }
            
            if (!isCat) { scroll_y = ImGui::GetScrollY(); scroll_max_y = ImGui::GetScrollMaxY(); visible_h = ImGui::GetWindowHeight(); }
            ImGui::EndChild();

            if (!isCat && scroll_max_y > 0.0f) {
                float thumb_h = MAX_F(visible_h * (visible_h / (visible_h + scroll_max_y)), 20.0f); float thumb_y = (scroll_y / scroll_max_y) * (visible_h - thumb_h);
                float bar_x = guiPos.x + currentGuiWidth - 8.0f; float bar_y = guiPos.y + 80.0f * scaled_UI + thumb_y;
                guiDraw->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + 3.0f, bar_y + thumb_h), IM_COL32(180, 180, 180, (int)(currentUIAlpha * 180.0f)), 1.5f);
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
                guiDraw->AddLine(ImVec2(br.x - 15, br.y - 5), ImVec2(br.x - 5, br.y - 15), IM_COL32(150, 150, 150, (int)(globalGuiAlpha * 180)), 3.0f);
                guiDraw->AddLine(ImVec2(br.x - 25, br.y - 5), ImVec2(br.x - 5, br.y - 25), IM_COL32(150, 150, 150, (int)(globalGuiAlpha * 180)), 3.0f);
                guiDraw->AddLine(ImVec2(br.x - 35, br.y - 5), ImVec2(br.x - 5, br.y - 35), IM_COL32(150, 150, 150, (int)(globalGuiAlpha * 180)), 3.0f);
            }
            ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(2);
        }

        // ==========================================
        // 3. 完美中心堆叠 灵动岛渲染 
        // ==========================================
        if (mod_island_enable) { 
            ImVec2 pB = font->CalcTextSizeA(s_textSize, 10000.0f, 0.0f, clientName);
            float collapsedH = MAX_F(s_iconSize, pB.y) + s_paddingY * 2.0f; float totalContentH = 0.0f;
            if (mod_scaffold_enable || blockAnimMult > 0.001f || ABS_F(blockAnimVel) > 0.001f || blockLinearAlpha > 0.001f) totalContentH += s_expandedHeight * blockAnimMult;
            for (int i = 0; i < 10; ++i) { if (notifs[i].active && (notifs[i].heightMult > 0.001f || notifs[i].linearAlpha > 0.001f || ABS_F(notifs[i].heightVel) > 0.001f)) { if (totalContentH > 0.0f) totalContentH += s_spacing * notifs[i].heightMult; totalContentH += s_normalHeight * notifs[i].heightMult; } }
            
            float targetCoreWidth = collapsedW * (1.0f - overallExpandMult) + activeMaxWidth * overallExpandMult; float targetCoreHeight = collapsedH * (1.0f - overallExpandMult) + totalContentH;
            if (currentCoreWidth == 0.0f) { currentCoreWidth = targetCoreWidth; currentCoreHeight = targetCoreHeight; }
            UpdateSpring(currentCoreWidth, coreWidthVel, targetCoreWidth, animSpeed, animBounciness, io->DeltaTime); UpdateSpring(currentCoreHeight, coreHeightVel, targetCoreHeight, animSpeed, animBounciness, io->DeltaTime);

            float targetRounding = (totalContentH > 0.0f) ? (24.0f * anim_islandScale) : (collapsedH * 0.5f);
            if (currentRounding == 0.0f) currentRounding = targetRounding;
            else { float rDiff = targetRounding - currentRounding; float rStep = 150.0f * io->DeltaTime; if (ABS_F(rDiff) <= rStep) currentRounding = targetRounding; else currentRounding += (rDiff > 0 ? rStep : -rStep); }

            float targetCoreX = (this->surfaceWidth - currentCoreWidth) * 0.5f + anim_islandOffsetX, targetCoreY = anim_islandOffsetY;                                   
            float s_safePaddingX = (shadowSize + 15.0f) * anim_islandScale, s_safePaddingY = (shadowSize + ABS_F(shadowOffsetY) + 15.0f) * anim_islandScale; 

            ImGui::SetNextWindowPos(ImVec2(targetCoreX - s_safePaddingX, targetCoreY - s_safePaddingY), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(currentCoreWidth + s_safePaddingX * 2.0f, currentCoreHeight + s_safePaddingY * 2.0f), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); 

            ImGui::Begin("DynamicIsland", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
            ImDrawList* drawList = ImGui::GetWindowDrawList(); float coreX = ImGui::GetCursorScreenPos().x + s_safePaddingX, coreY = ImGui::GetCursorScreenPos().y + s_safePaddingY;

            drawList->PushClipRectFullScreen();
            if (shadowSize > 0.0f && shadowAlpha > 0.0f) {
                int steps = 40; float strk = ((shadowSize * anim_islandScale) / steps) * 1.8f; 
                for (int i = 1; i <= steps; i++) {
                    float t = (float)i / steps; float alpha = shadowAlpha * (1.0f - t) * (1.0f - t) * (1.0f - t); float expand = t * shadowSize * anim_islandScale;
                    drawList->AddRect(ImVec2(coreX - expand, coreY - expand + (shadowOffsetY * anim_islandScale)), ImVec2(coreX + currentCoreWidth + expand, coreY + currentCoreHeight + expand + (shadowOffsetY * anim_islandScale)), IM_COL32(0, 0, 0, (int)(alpha * 255.0f)), currentRounding + expand, 0, strk);
                }
            }
            drawList->PopClipRect();
            drawList->AddRectFilled(ImVec2(coreX, coreY), ImVec2(coreX + currentCoreWidth, coreY + currentCoreHeight), IM_COL32(0, 0, 0, (int)(bgAlpha * 255.0f)), currentRounding);
            drawList->PushClipRect(ImVec2(coreX, coreY), ImVec2(coreX + currentCoreWidth, coreY + currentCoreHeight), true);

            if (opalLinearAlpha > 0.001f) {
                float aCY = coreY + collapsedH * 0.5f, aCX = coreX + (currentCoreWidth - collapsedW) * 0.5f + s_padCollapsedX; 
                float wave1 = waveTime * waveSpeed, wave3 = waveTime * waveSpeed + 0.2f;
                ImU32 iCol1 = ImGui::ColorConvertFloat4ToU32(LerpColorMulti(themeColors, enableWave ? wave1 : 0.0f, opalLinearAlpha));
                ImU32 iCol3 = ImGui::ColorConvertFloat4ToU32(LerpColorMulti(themeColors, enableWave ? wave3 : 0.0f, opalLinearAlpha));
                
                drawList->AddCircleFilled(ImVec2(aCX + s_iconSize * 0.5f, aCY), s_iconSize*0.5f, iCol1);
                drawList->AddCircleFilled(ImVec2(aCX + s_iconSize * 0.5f, aCY), s_iconSize*0.22f, IM_COL32(0,0,0,(int)(bgAlpha*255.0f*opalLinearAlpha)));
                drawList->AddCircleFilled(ImVec2(aCX + s_iconSize * 0.5f, aCY), s_iconSize*0.1f, iCol3);

                float curX = aCX + s_iconSize + s_spacing, tY = aCY - pB.y * 0.5f;
                int c_len = strlen(clientName);
                for (int i = 0; i < c_len; i++) {
                    float char_wave = waveTime * waveSpeed + 0.3f + (i * 0.1f);
                    ImU32 cCol = ImGui::ColorConvertFloat4ToU32(LerpColorMulti(themeColors, enableWave ? char_wave : 0.0f, opalLinearAlpha));
                    char b[2] = { clientName[i], '\0' };
                    drawList->AddText(font, s_textSize, ImVec2(curX, tY), cCol, b); curX += font->CalcTextSizeA(s_textSize, 10000.0f, 0.0f, b).x;
                }
                char sBuf[128]; snprintf(sBuf, sizeof(sBuf), " | Prodee163 | %dfps | 0ms", currentFps);
                drawList->AddText(font, s_textSize, ImVec2(curX, tY), IM_COL32(255, 255, 255, (int)(255.0f*opalLinearAlpha)), sBuf);
            }

            float currentDrawY = coreY + (currentCoreHeight - totalContentH) * 0.5f;
            if (blockAnimMult > 0.001f || blockLinearAlpha > 0.001f) {
                float alpha = blockLinearAlpha; 
                if (alpha > 0.001f) {
                    float upY = currentDrawY + (40.0f * anim_islandScale), cX = coreX + s_padBlockX; 
                    drawList->AddRectFilled(ImVec2(cX, upY - s_scaffoldBgSize * 0.5f), ImVec2(cX + s_scaffoldBgSize, upY + s_scaffoldBgSize * 0.5f), ImGui::ColorConvertFloat4ToU32(LerpColorSolid(ImVec4(0,0,0,1), ImVec4(60.f/255.f, 60.f/255.f, 60.f/255.f, 1.0f), alpha)), 10.0f * anim_islandScale);
                    
                    const char* bTitle = "Scaffold Toggle"; char bStatus[128]; snprintf(bStatus, sizeof(bStatus), "%d blocks left - %.1f blocks/m", (int)blockCount, displayBps);
                    ImVec2 tB = font->CalcTextSizeA(titleTextSize * anim_islandScale, 10000.0f, 0.0f, bTitle);
float textsH = tB.y + (5.0f * anim_islandScale) + font->CalcTextSizeA(statusTextSize * anim_islandScale, 10000.0f, 0.0f, bStatus).y;

                    ImVec2 ic(cX + s_scaffoldBgSize * 0.5f, upY); float cs = (cubeSize * anim_islandScale) * 0.25f; 
                    ImVec2 p0(ic.x, ic.y + cs), p1(ic.x + cs*0.866f, ic.y + cs*0.5f), p2(ic.x + cs*0.866f, ic.y - cs*0.5f), p3(ic.x, ic.y - cs), p4(ic.x - cs*0.866f, ic.y - cs*0.5f), p5(ic.x - cs*0.866f, ic.y + cs*0.5f); 
                    
                    ImVec4 mt = ImVec4(themeColors[0].x, themeColors[0].y, themeColors[0].z, 1.0f);
                    ImU32 cB = ImGui::ColorConvertFloat4ToU32(LerpColorSolid(ImVec4(0,0,0,1), ImVec4(mt.x*0.8f, mt.y*0.8f, mt.z*0.8f, 1), alpha)), cR = ImGui::ColorConvertFloat4ToU32(LerpColorSolid(ImVec4(0,0,0,1), ImVec4(mt.x*0.5f, mt.y*0.5f, mt.z*0.5f, 1), alpha)), cL = ImGui::ColorConvertFloat4ToU32(LerpColorSolid(ImVec4(0,0,0,1), ImVec4(mt.x*0.65f, mt.y*0.65f, mt.z*0.65f, 1), alpha)), cW = ImGui::ColorConvertFloat4ToU32(LerpColorSolid(ImVec4(0,0,0,1), mt, alpha));
                    drawList->AddQuadFilled(ic, p5, p0, p1, cB); drawList->AddQuadFilled(ic, p1, p2, p3, cR); drawList->AddQuadFilled(ic, p3, p4, p5, cL); 
                    float wT = 1.5f * anim_islandScale; drawList->AddLine(p0, p1, cW, wT); drawList->AddLine(p1, p2, cW, wT); drawList->AddLine(p2, p3, cW, wT); drawList->AddLine(p3, p4, cW, wT); drawList->AddLine(p4, p5, cW, wT); drawList->AddLine(p5, p0, cW, wT); drawList->AddLine(ic, p0, cW, wT); drawList->AddLine(ic, p2, cW, wT); drawList->AddLine(ic, p4, cW, wT);

                    float tX = cX + s_scaffoldBgSize + s_spacing, tY1 = upY - textsH * 0.5f, tY2 = tY1 + tB.y + (5.0f * anim_islandScale);
                    drawList->AddText(font, titleTextSize * anim_islandScale, ImVec2(tX, tY1), IM_COL32(255, 255, 255, (int)(255.0f * alpha)), bTitle); drawList->AddText(font, statusTextSize * anim_islandScale, ImVec2(tX, tY2), IM_COL32(180, 180, 180, (int)(255.0f * alpha)), bStatus);

                    float targetProg = CLAMP_F((float)blockCount / 64.0f, 0.0f, 1.0f); UpdateSpring(smoothProgress, progressVel, targetProg, animSpeed, animBounciness, io->DeltaTime);
                    float pbW = currentCoreWidth - s_padBlockX * 2.0f, pbH = 16.0f * anim_islandScale, pbY = currentDrawY + (79.0f + progressBarOffsetY) * anim_islandScale, pbR = pbH * 0.5f; 
                    drawList->AddRectFilled(ImVec2(cX, pbY), ImVec2(cX + pbW, pbY + pbH), ImGui::ColorConvertFloat4ToU32(LerpColorSolid(ImVec4(0,0,0,1), ImVec4(40.f/255.f, 40.f/255.f, 40.f/255.f, 1.0f), alpha)), pbR);
                    if (smoothProgress > 0.01f) drawList->AddRectFilled(ImVec2(cX, pbY), ImVec2(cX + MAX_F(pbR*2.0f, pbW * smoothProgress), pbY + pbH), cW, pbR);
                }
                currentDrawY += s_expandedHeight * blockAnimMult;
            }

            for (int i = 0; i < 10; ++i) {
                if (!notifs[i].active || (notifs[i].heightMult < 0.001f && notifs[i].linearAlpha < 0.001f && ABS_F(notifs[i].heightVel) < 0.001f)) continue;
                if (currentDrawY > coreY + (currentCoreHeight - totalContentH) * 0.5f + 0.001f) currentDrawY += s_spacing * notifs[i].heightMult;
                float alpha = notifs[i].linearAlpha;
                if (alpha > 0.001f) {
                    float cY = currentDrawY + (40.0f * anim_islandScale), swW = 50.0f * s_switchScale, swH = 28.0f * s_switchScale;
                    const char* nTitleBuf = "Module Toggle"; char st1Buf[64]; snprintf(st1Buf, sizeof(st1Buf), "%s has been ", notifs[i].modName); const char* st2Str = notifs[i].isEnable ? "Enabled" : "Disabled";
                    ImVec2 tB = font->CalcTextSizeA(titleTextSize * anim_islandScale, 10000.0f, 0.0f, nTitleBuf), st1B = font->CalcTextSizeA(statusTextSize * anim_islandScale, 10000.0f, 0.0f, st1Buf), st2B = font->CalcTextSizeA(statusTextSize * anim_islandScale, 10000.0f, 0.0f, st2Str);
                    float txtH = tB.y + (5.0f * anim_islandScale) + MAX_F(st1B.y, st2B.y), cX = coreX + s_padNormalX;

                    ImVec4 actC = ImVec4(themeColors[0].x, themeColors[0].y, themeColors[0].z, 1.0f);
                    ImU32 trkU32 = ImGui::ColorConvertFloat4ToU32(LerpColorSolid(ImVec4(0,0,0,1), LerpColor(ImVec4(0.2f,0.2f,0.2f,1.0f), actC, notifs[i].switchAnimPos, 1.0f), alpha));
                    drawList->AddRectFilled(ImVec2(cX, cY - swH * 0.5f), ImVec2(cX + swW, cY + swH * 0.5f), trkU32, swH * 0.5f);
                    float tX = (cX + swH * 0.5f) + (swW - swH) * notifs[i].switchAnimPos;
                    ImU32 thmbU32 = ImGui::ColorConvertFloat4ToU32(LerpColorSolid(ImVec4(0,0,0,1), LerpColor(ImVec4(0.6f,0.6f,0.6f,1.0f), ImVec4(1.0f,1.0f,1.0f,1.0f), notifs[i].switchAnimPos, 1.0f), alpha));
                    drawList->AddCircleFilled(ImVec2(tX, cY), swH * 0.5f - (4.0f * s_switchScale), thmbU32);

                    float txtX = cX + swW + s_spacing, txtY1 = cY - txtH * 0.5f, txtY2 = txtY1 + tB.y + (5.0f * anim_islandScale);
                    drawList->AddText(font, titleTextSize * anim_islandScale, ImVec2(txtX, txtY1), IM_COL32(255, 255, 255, (int)(255.0f * alpha)), nTitleBuf);
                    drawList->AddText(font, statusTextSize * anim_islandScale, ImVec2(txtX, txtY2), IM_COL32(150, 150, 150, (int)(255.0f * alpha)), st1Buf);
                    drawList->AddText(font, statusTextSize * anim_islandScale, ImVec2(txtX + st1B.x, txtY2), notifs[i].isEnable ? IM_COL32(50, 220, 50, (int)(255.0f * alpha)) : IM_COL32(255, 50, 50, (int)(255.0f * alpha)), st2Str);
                }
                currentDrawY += s_normalHeight * notifs[i].heightMult;
            }
            drawList->PopClipRect(); ImGui::End(); ImGui::PopStyleColor(1); ImGui::PopStyleVar(2);
        }
        
        // ==========================================
        // 4. Target HUD 独立渲染层
        // ==========================================
        if (th_state != TH_HIDDEN || th_alpha_anim > 0.0f) { 
            float real_scale = anim_th_scale * th_current_scale_mult;
            if (real_scale > 0.01f) {
                const float TH_ALPHA = th_alpha_anim; 
                float th_height = 100.0f; 
                float th_rounding = 25.0f, th_avatar_rounding = 20.0f;
                float th_winlose_alpha = 0.6f;
                
                float base_name_size = 25.0f;
                float base_winlose_size = (th_mode == 0) ? 25.0f : 30.0f; 
                float base_hptext_size = (th_mode == 0) ? 15.0f : 30.0f;  
                float base_avatar_size = 90.0f;  
                
                float th_name_size = base_name_size * ((th_mode == 1) ? rh_name_scale : 1.0f);
                float th_winlose_size = base_winlose_size * ((th_mode == 1) ? rh_win_scale : 1.0f);
                float th_hptext_size = base_hptext_size * ((th_mode == 1) ? rh_hp_scale : 1.0f);
                float th_avatar_size = base_avatar_size * ((th_mode == 1) ? rh_avatar_scale : 1.0f);

                const char* playerName = "Target Entity"; ImVec2 unscaled_name_sz = font->CalcTextSizeA(th_name_size, 10000.0f, 0.0f, playerName);
                
                const char* hpPrefixDisplay = "Health: "; 
                ImVec2 prefix_sz_unscaled = font->CalcTextSizeA(th_hptext_size, 10000.0f, 0.0f, hpPrefixDisplay);
                float char_w_unscaled = font->CalcTextSizeA(th_hptext_size, 10000.0f, 0.0f, "8").x;
                float hp_val = MAX_F(0.0f, anim_target_health);
                int hp_max_p = (hp_val >= 99.9f) ? 3 : ((hp_val >= 9.9f) ? 2 : 1);
                float hp_unscaled_width = prefix_sz_unscaled.x + hp_max_p * char_w_unscaled;
                float wl_unscaled_w = font->CalcTextSizeA(th_winlose_size, 10000.0f, 0.0f, "Win").x;

                float th_avatar_x, th_avatar_y, th_name_x, th_name_y, th_hptext_x, th_hptext_y, th_winlose_x, th_winlose_y, th_target_width;

                if (th_mode == 0) {
                    th_avatar_x = 5.0f; th_avatar_y = 5.0f; 
                    th_name_x = th_avatar_size + 15.0f; th_name_y = 15.0f;
                    th_hptext_x = th_avatar_size + 15.0f; th_hptext_y = 45.0f; 
                    th_winlose_x = th_name_x + unscaled_name_sz.x + 15.0f; th_winlose_y = 15.0f; 
                    th_target_width = th_avatar_size + MAX_F(unscaled_name_sz.x, hp_unscaled_width + 40.0f) + 120.0f;
                } else {
                    float left_content_w = MAX_F(unscaled_name_sz.x, hp_unscaled_width + 10.0f + wl_unscaled_w);
                    th_target_width = 10.0f + left_content_w + 20.0f + th_avatar_size + 10.0f; 
                    
                    th_name_x = 10.0f; 
                    th_name_y = 15.0f;
                    th_hptext_x = 10.0f; 
                    th_hptext_y = 50.0f;
                    th_winlose_x = th_hptext_x + hp_unscaled_width + 10.0f; 
                    th_winlose_y = 50.0f;
                    th_avatar_x = th_target_width - th_avatar_size - 5.0f; 
                    th_avatar_y = 5.0f;
                }
                
                if (anim_th_width == 0.0f) { anim_th_width = th_target_width; anim_th_height = th_height; }
                anim_th_width += (th_target_width - anim_th_width) * 15.0f * io->DeltaTime;
                anim_th_height += (th_height - anim_th_height) * 15.0f * io->DeltaTime;

                float th_hpbar_x = th_avatar_x + th_avatar_size + 5.0f, th_hpbar_y = 75.0f, th_hpbar_width = anim_th_width - th_hpbar_x - 5.0f; 
                float th_w = anim_th_width * real_scale, th_h = anim_th_height * real_scale;
                
                float center_x = this->surfaceWidth * 0.5f + anim_th_offsetX, center_y = this->surfaceHigh * 0.5f + anim_th_offsetY;
                float th_x = center_x - th_w * 0.5f, th_y = center_y - th_h * 0.5f, th_safe_pad = 150.0f * real_scale; 
                
                ImGui::SetNextWindowPos(ImVec2(th_x - th_safe_pad, th_y - th_safe_pad), ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(th_w + th_safe_pad * 2.0f, th_h + th_safe_pad * 2.0f), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
                
                ImGui::Begin("TargetHUD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
                ImDrawList* dList = ImGui::GetWindowDrawList();
                
                if (shadowSize > 0.0f && shadowAlpha > 0.0f) {
                    int steps = 30; float strokeThickness = (shadowSize / steps) * 1.5f;
                    for (int i = 1; i <= steps; i++) {
                        float t = (float)i / steps; float alpha = shadowAlpha * TH_ALPHA * (1.0f - t) * (1.0f - t); float expand = t * shadowSize * real_scale;
                        dList->AddRect(ImVec2(th_x - expand, th_y - expand), ImVec2(th_x + th_w + expand, th_y + th_h + expand), IM_COL32(0, 0, 0, (int)(alpha * 255.0f)), th_rounding * real_scale + expand, 0, strokeThickness);
                    }
                }
                dList->AddRectFilled(ImVec2(th_x, th_y), ImVec2(th_x + th_w, th_y + th_h), IM_COL32(0, 0, 0, (int)(bgAlpha * TH_ALPHA * 255.0f)), th_rounding * real_scale);
                
                float current_avatar_size = th_avatar_size, current_red_alpha = 0.0f;
                if (hit_timer < 0.3f) {
                    float p = hit_timer / 0.3f; if (p < 0.5f) current_avatar_size = th_avatar_size * (1.0f - (1.0f - 0.8f) * (p * 2.0f)); else current_avatar_size = th_avatar_size * (1.0f - (1.0f - 0.8f) * ((1.0f - p) * 2.0f));
                    current_red_alpha = 0.4f * (1.0f - hit_timer / 0.3f);
                }

                float center_offset = (th_avatar_size - current_avatar_size) * 0.5f;
                ImVec2 av_min = ImVec2(th_x + th_avatar_x * real_scale + center_offset * real_scale, th_y + th_avatar_y * real_scale + center_offset * real_scale);
                ImVec2 av_max = ImVec2(av_min.x + current_avatar_size * real_scale, av_min.y + current_avatar_size * real_scale);
                dList->AddRectFilled(av_min, av_max, IM_COL32(60, 60, 60, (int)(TH_ALPHA * 255.0f)), th_avatar_rounding * real_scale);
                if (current_red_alpha > 0.001f) { dList->AddRectFilled(av_min, av_max, IM_COL32(255, 0, 0, (int)(current_red_alpha * TH_ALPHA * 255.0f)), th_avatar_rounding * real_scale); }

                ImVec2 av_center = ImVec2((av_min.x + av_max.x) * 0.5f, (av_min.y + av_max.y) * 0.5f);
                for (auto it = th_particles.begin(); it != th_particles.end(); ) {
                    if (it->life >= it->max_life + it->linger_time + it->fade_time) { it = th_particles.erase(it); } else {
                        it->life += io->DeltaTime; float p_alpha = 0.8f; 
                        if (it->life < it->max_life) {
                            float move_p = it->life / it->max_life; float ease = 1.0f - std::pow(1.0f - move_p, 3.0f);
                            it->curr_x = av_center.x + std::cos(it->angle) * it->target_dist * ease * real_scale; it->curr_y = av_center.y + std::sin(it->angle) * it->target_dist * ease * real_scale;
                        } else if (it->life > it->max_life + it->linger_time) { float fade_p = (it->life - it->max_life - it->linger_time) / it->fade_time; p_alpha = 0.8f * (1.0f - fade_p); }
                        p_alpha = MAX_F(0.0f, p_alpha);
                        if (p_alpha > 0.001f) {
                            ImU32 particle_col = ImGui::ColorConvertFloat4ToU32(LerpColorMulti(themeColors, waveTime * waveSpeed + it->angle, p_alpha * TH_ALPHA));
                            for (int g = 1; g <= 2; ++g) { 
                                float g_alpha = p_alpha * (1.0f - (float)g / 3.0f) * 0.4f; 
                                ImU32 shadow_col = ImGui::ColorConvertFloat4ToU32(LerpColorMulti(themeColors, waveTime * waveSpeed + it->angle, g_alpha * TH_ALPHA));
                                dList->AddCircleFilled(ImVec2(it->curr_x, it->curr_y), (it->size + g * 1.5f) * real_scale, shadow_col); 
                            }
                            dList->AddCircleFilled(ImVec2(it->curr_x, it->curr_y), it->size * real_scale, particle_col);
                        }
                        ++it;
                    }
                }
                
                dList->AddText(font, th_name_size * real_scale, ImVec2(th_x + th_name_x * real_scale, th_y + th_name_y * real_scale), IM_COL32(255, 255, 255, (int)(TH_ALPHA * 255.0f)), playerName);
                
                bool is_win = (self_health >= (float)target_health); win_anim_t += ((is_win ? 1.0f : 0.0f) - win_anim_t) * 10.0f * io->DeltaTime;
                float win_alpha = win_anim_t * th_winlose_alpha, lose_alpha = (1.0f - win_anim_t) * th_winlose_alpha;
                float win_y = th_y + th_winlose_y * real_scale - (1.0f - win_anim_t) * 10.0f * real_scale, lose_y = th_y + th_winlose_y * real_scale + win_anim_t * 10.0f * real_scale;
                if (win_alpha > 0.01f) { ImU32 win_col = IM_COL32(50, 255, 50, (int)(win_alpha * TH_ALPHA * 255.0f)); dList->AddText(font, th_winlose_size * real_scale, ImVec2(th_x + th_winlose_x * real_scale, win_y), win_col, "Win"); }
                if (lose_alpha > 0.01f) { ImU32 lose_col = IM_COL32(255, 50, 50, (int)(lose_alpha * TH_ALPHA * 255.0f)); dList->AddText(font, th_winlose_size * real_scale, ImVec2(th_x + th_winlose_x * real_scale, lose_y), lose_col, "Lose"); }
                
                float hpTextScaleSize = th_hptext_size * real_scale; ImVec2 prefix_sz = font->CalcTextSizeA(hpTextScaleSize, 10000.0f, 0.0f, hpPrefixDisplay);
                float hx = th_x + th_hptext_x * real_scale, hy = th_y + th_hptext_y * real_scale;
                dList->AddText(font, hpTextScaleSize, ImVec2(hx, hy), IM_COL32(255, 255, 255, (int)(TH_ALPHA * 255.0f)), hpPrefixDisplay);
                
                float dx = hx + prefix_sz.x; dList->PushClipRect(ImVec2(dx, hy), ImVec2(dx + 100.0f * real_scale, hy + prefix_sz.y), true);
                float val = MAX_F(0.0f, anim_target_health); float snap_val = std::floor(val + 0.5f); if (ABS_F(val - snap_val) < 0.01f) { val = snap_val; }
                int max_p = (val >= 99.9f) ? 3 : ((val >= 9.9f) ? 2 : 1); float char_w = font->CalcTextSizeA(hpTextScaleSize, 10000.0f, 0.0f, "8").x;
                for (int p = max_p - 1; p >= 0; --p) {
                    float divisor = std::pow(10.0f, (float)p); int d_curr = (int)(val / divisor) % 10; if (d_curr < 0) d_curr += 10;
                    int d_next = (d_curr + 1) % 10; float lower_val = std::fmod(val, divisor), roll_threshold = divisor - 1.0f, frac = 0.0f;
                    if (lower_val > roll_threshold) { frac = lower_val - roll_threshold; }
                    float y_curr = hy - frac * prefix_sz.y, y_next = hy + (1.0f - frac) * prefix_sz.y;
                    char c_curr[2] = {(char)('0' + d_curr), '\0'}, c_next[2] = {(char)('0' + d_next), '\0'};
                    dList->AddText(font, hpTextScaleSize, ImVec2(dx, y_curr), IM_COL32(255,255,255, (int)(TH_ALPHA*255.0f)), c_curr);
                    dList->AddText(font, hpTextScaleSize, ImVec2(dx, y_next), IM_COL32(255,255,255, (int)(TH_ALPHA*255.0f)), c_next); dx += char_w;
                }
                dList->PopClipRect();
                
                if (th_mode == 0) {
                    float hp_w = th_hpbar_width * real_scale, hp_h = 18.0f * real_scale;
                    float hp_bx = th_x + th_hpbar_x * real_scale, hp_by = th_y + th_hpbar_y * real_scale, hp_r = hp_h * 0.5f; 
                    dList->AddRectFilled(ImVec2(hp_bx, hp_by), ImVec2(hp_bx + hp_w, hp_by + hp_h), IM_COL32(40, 40, 40, (int)(TH_ALPHA * 255.0f)), hp_r);
                    float hp_ratio = CLAMP_F(anim_target_health / 20.0f, 0.0f, 1.0f); float fill_w = hp_w * hp_ratio;
                    if (fill_w > 0.001f) {
                        dList->PushClipRect(ImVec2(hp_bx, hp_by), ImVec2(hp_bx + fill_w, hp_by + hp_h), true);
                        float wave_left = waveTime * waveSpeed, wave_right = waveTime * waveSpeed + 0.5f; 
                        ImU32 col_left = ImGui::ColorConvertFloat4ToU32(enableWave ? LerpColorMulti(themeColors, wave_left, TH_ALPHA) : ImVec4(themeColors[0].x, themeColors[0].y, themeColors[0].z, TH_ALPHA));
                        ImU32 col_right = ImGui::ColorConvertFloat4ToU32(enableWave ? LerpColorMulti(themeColors, wave_right, TH_ALPHA) : ImVec4(themeColors[1].x, themeColors[1].y, themeColors[1].z, TH_ALPHA));
                        dList->AddRectFilledMultiColor(ImVec2(hp_bx, hp_by), ImVec2(hp_bx + hp_w, hp_by + hp_h), col_left, col_right, col_right, col_left);
                        dList->PopClipRect();
                    }
                }
                ImGui::End(); ImGui::PopStyleColor(1); ImGui::PopStyleVar(2);
            }
        }

        // ==========================================
        // 5. ArrayList 右上角瀑布流渲染层 
        // ==========================================
        if (mod_arraylist_enable) {
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(this->surfaceWidth, this->surfaceHigh), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            ImGui::Begin("ArrayListHUD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
            ImDrawList* alList = ImGui::GetWindowDrawList();

            struct AlItemDraw {
                ArraylistMod* mod;
                std::string suffix;
                float t;
                float y_min;
                float y_max;
                float x_min;
                float x_max;
                float rect_w;
                float rect_h;
                ImVec2 txt_sz;
                ImVec2 suf_sz;
            };

            std::vector<AlItemDraw> draw_items;
            std::vector<ArraylistMod*> active_mods;
            for (int i = 0; i < 6; i++) {
                if (al_mods[i].anim_t > 0.001f) active_mods.push_back(&al_mods[i]);
            }

            float font_size = 25.0f * al_scale;
            std::sort(active_mods.begin(), active_mods.end(), [&](ArraylistMod* a, ArraylistMod* b) {
                std::string s_a = "", s_b = "";
                if (std::string(a->name) == "TargetHUD") s_a = (th_mode == 0) ? " Fate" : " Rhythm";
                if (std::string(b->name) == "TargetHUD") s_b = (th_mode == 0) ? " Fate" : " Rhythm";
                float w_a = font->CalcTextSizeA(font_size, 10000.0f, 0.0f, a->name).x + (s_a.empty() ? 0.0f : font->CalcTextSizeA(font_size, 10000.0f, 0.0f, s_a.c_str()).x);
                float w_b = font->CalcTextSizeA(font_size, 10000.0f, 0.0f, b->name).x + (s_b.empty() ? 0.0f : font->CalcTextSizeA(font_size, 10000.0f, 0.0f, s_b.c_str()).x);
                return w_a > w_b;
            });

            float y = anim_al_offsetY + 5.0f;
            for (size_t i = 0; i < active_mods.size(); i++) {
                auto* mod = active_mods[i];
                float e_t = 1.0f - std::pow(1.0f - mod->anim_t, 4.0f);

                std::string suffix = "";
                if (std::string(mod->name) == "TargetHUD") suffix = (th_mode == 0) ? " Fate" : " Rhythm";

                ImVec2 target_txt_sz = font->CalcTextSizeA(font_size, 10000.0f, 0.0f, mod->name);
                ImVec2 target_suf_sz = suffix.empty() ? ImVec2(0,0) : font->CalcTextSizeA(font_size, 10000.0f, 0.0f, suffix.c_str());

                // 缩小左右留白
                float target_w = target_txt_sz.x + target_suf_sz.x + 10.0f * al_scale + al_line_width * al_scale;
                float target_h = MAX_F(target_txt_sz.y, target_suf_sz.y) + 10.0f * al_scale + al_bg_height_offset * al_scale;

                if (mod->current_w == 0.0f) { mod->current_w = target_w; mod->current_h = target_h; }
                mod->current_w += (target_w - mod->current_w) * 15.0f * io->DeltaTime;
                mod->current_h += (target_h - mod->current_h) * 15.0f * io->DeltaTime;

                float draw_w = mod->current_w;
                float draw_h = mod->current_h * e_t;

                float target_x = this->surfaceWidth - draw_w + anim_al_offsetX;
                float start_x = this->surfaceWidth + 20.0f + anim_al_offsetX;
                float current_x = start_x + (target_x - start_x) * e_t;

                draw_items.push_back({mod, suffix, e_t, y, y + draw_h, current_x, current_x + draw_w, draw_w, draw_h, target_txt_sz, target_suf_sz});
                y += (draw_h + al_spacing * al_scale) * e_t;
            }

            // === 新版整块大 Shadow + 背景遮罩 ===
            const bool has_items = !draw_items.empty();
            const bool use_bg_mask = (!al_no_bg && bgAlpha > 0.001f);

            if (has_items) {
                float block_min_x = FLT_MAX, block_min_y = FLT_MAX;
                float block_max_x = -FLT_MAX, block_max_y = -FLT_MAX;

                for (size_t i = 0; i < draw_items.size(); ++i) {
                    const auto& it = draw_items[i];
                    block_min_x = MIN_F(block_min_x, it.x_min);
                    block_min_y = MIN_F(block_min_y, it.y_min);
                    block_max_x = MAX_F(block_max_x, it.x_max);
                    block_max_y = MAX_F(block_max_y, it.y_max + al_bg_overlap);
                }

                float shadow_rect_min_x = block_min_x;
                float shadow_rect_min_y = block_min_y;
                float shadow_rect_max_x = block_max_x;
                float shadow_rect_max_y = block_max_y + al_shadow_size * al_scale;
                if (shadow_rect_max_y < shadow_rect_min_y) shadow_rect_max_y = shadow_rect_min_y;

                float current_shadow_alpha = al_shadow_chroma ? al_chroma_shadow_alpha : shadowAlpha;
                if (!al_no_shadow && current_shadow_alpha > 0.001f) {
                    const float BIG_BLUR = 200.0f;
                    const int steps = 64;
                    const float strokeThickness = (BIG_BLUR / (float)steps) * 1.25f;

                    ImVec4 shV4 = al_shadow_chroma ? LerpColorMulti(themeColors, waveTime * waveSpeed, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
                    int sr = (int)(shV4.x * 255.0f);
                    int sg = (int)(shV4.y * 255.0f);
                    int sb = (int)(shV4.z * 255.0f);

                    for (int s = 1; s <= steps; ++s) {
                        float t = (float)s / (float)steps;
                        float alpha = current_shadow_alpha * std::pow(1.0f - t, 2.7f);
                        float expand = t * BIG_BLUR;

                        alList->AddRect(
                            ImVec2(shadow_rect_min_x - expand, shadow_rect_min_y - expand),
                            ImVec2(shadow_rect_max_x + expand, shadow_rect_max_y + expand),
                            IM_COL32(sr, sg, sb, (int)(alpha * 255.0f)),
                            0.0f, 0, strokeThickness
                        );
                    }
                }

                // 用背景遮罩盖住内部 shadow，避免阴影污染内部
                if (use_bg_mask) {
                    for (size_t i = 0; i < draw_items.size(); ++i) {
                        const auto& it = draw_items[i];
                        alList->AddRectFilled(
                            ImVec2(it.x_min, it.y_min),
                            ImVec2(it.x_max, it.y_max + al_bg_overlap),
                            IM_COL32(0, 0, 0, (int)(bgAlpha * 255.0f * it.t))
                        );
                    }
                }
            }

            for (size_t i = 0; i < draw_items.size(); i++) {
                auto& item = draw_items[i];
                ImVec2 min_pt(item.x_min, item.y_min), max_pt(item.x_max, item.y_max);
                float wave_top = waveTime * waveSpeed + i * 0.15f, wave_bot = waveTime * waveSpeed + (i + 1) * 0.15f;
                ImVec4 c_top = LerpColorMulti(themeColors, wave_top, 1.0f), c_bot = LerpColorMulti(themeColors, wave_bot, 1.0f);

                // 有遮罩时不重复画背景，避免叠黑
                if (!al_no_bg && !use_bg_mask) {
                    alList->AddRectFilled(min_pt, ImVec2(max_pt.x, max_pt.y + al_bg_overlap), IM_COL32(0, 0, 0, (int)(bgAlpha * 255.0f * item.t)));
                }

                // line 用回以前模式（无圆角）
                float line_w = al_line_width * al_scale, line_h = item.rect_h * al_line_height_mult;
                float line_y_min = min_pt.y + (item.rect_h - line_h) * 0.5f, line_y_max = min_pt.y + (item.rect_h + line_h) * 0.5f;
                ImU32 col_top = ImGui::ColorConvertFloat4ToU32(ImVec4(c_top.x, c_top.y, c_top.z, item.t)), col_bot = ImGui::ColorConvertFloat4ToU32(ImVec4(c_bot.x, c_bot.y, c_bot.z, item.t));

                float render_line_x = max_pt.x + al_line_offsetX * al_scale;
                if (!al_no_line) {
                    alList->AddRectFilledMultiColor(ImVec2(render_line_x - line_w, line_y_min), ImVec2(render_line_x, line_y_max + al_bg_overlap), col_top, col_top, col_bot, col_bot);
                }

                float render_font_size = MAX_F(0.1f, 25.0f * al_scale * item.t);
                ImVec2 render_txt_sz = font->CalcTextSizeA(render_font_size, 10000.0f, 0.0f, item.mod->name);

                float text_y = min_pt.y + (item.rect_h - render_txt_sz.y) * 0.5f; 
                float final_text_x = min_pt.x + 5.0f * al_scale;
                
                if (al_glow_enable) {
                    float glow_alpha = item.t * al_glow_alpha * 0.35f; 
                    ImU32 text_glow_col = ImGui::ColorConvertFloat4ToU32(ImVec4(c_top.x, c_top.y, c_top.z, glow_alpha));
                    float radius = al_glow_intensity * al_scale;
                    
                    int passes = 8;
                    for(int j = 0; j < passes; j++) {
                        float ang = (j / (float)passes) * 6.28318f; 
                        alList->AddText(font, render_font_size, ImVec2(final_text_x + cos(ang) * radius, text_y + sin(ang) * radius), text_glow_col, item.mod->name);
                    }
                    for(int j = 0; j < passes; j++) {
                        float ang = (j / (float)passes) * 6.28318f;
                        alList->AddText(font, render_font_size, ImVec2(final_text_x + cos(ang) * radius * 0.5f, text_y + sin(ang) * radius * 0.5f), text_glow_col, item.mod->name);
                    }
                }
                alList->AddText(font, render_font_size, ImVec2(final_text_x, text_y), col_top, item.mod->name);
                
                if (!item.suffix.empty()) {
                    float suf_x = final_text_x + render_txt_sz.x;
                    ImVec4 suf_color = ImVec4(150.f/255.f, 150.f/255.f, 150.f/255.f, 0.7f * item.t);
                    ImU32 suf_col32 = ImGui::ColorConvertFloat4ToU32(suf_color);
                    if (al_glow_enable) {
                        float glow_alpha_suf = item.t * al_glow_alpha * 0.35f;
                        ImU32 suf_glow_col = ImGui::ColorConvertFloat4ToU32(ImVec4(150.f/255.f, 150.f/255.f, 150.f/255.f, glow_alpha_suf));
                        float radius = al_glow_intensity * al_scale;
                        
                        int passes = 8;
                        for(int j = 0; j < passes; j++) {
                            float ang = (j / (float)passes) * 6.28318f;
                            alList->AddText(font, render_font_size, ImVec2(suf_x + cos(ang) * radius, text_y + sin(ang) * radius), suf_glow_col, item.suffix.c_str());
                        }
                        for(int j = 0; j < passes; j++) {
                            float ang = (j / (float)passes) * 6.28318f;
                            alList->AddText(font, render_font_size, ImVec2(suf_x + cos(ang) * radius * 0.5f, text_y + sin(ang) * radius * 0.5f), suf_glow_col, item.suffix.c_str());
                        }
                    }
                    alList->AddText(font, render_font_size, ImVec2(suf_x, text_y), suf_col32, item.suffix.c_str());
                }
            }

            ImGui::End(); ImGui::PopStyleColor(1); ImGui::PopStyleVar(2);
        }

        // ==========================================
        // 6. 快捷悬浮窗 独立渲染图层 (动态触摸穿透与拦截)
        // ==========================================
        static bool fb_touch_started_on_button = false;
        static bool fb_was_down = false;

        bool hovering_any_fb = false;
        for (int i = 0; i < 6; i++) {
            if (*al_mods[i].show_fb) {
                ImVec2 p_min(al_mods[i].current_fb_x, al_mods[i].current_fb_y);
                ImVec2 p_max(p_min.x + al_mods[i].fb_anim_w, p_min.y + al_mods[i].fb_anim_h);
                if (io->MousePos.x >= p_min.x && io->MousePos.x <= p_max.x && io->MousePos.y >= p_min.y && io->MousePos.y <= p_max.y) {
                    hovering_any_fb = true;
                    break;
                }
            }
        }

        bool is_down = io->MouseDown[0];
        bool just_pressed = is_down && !fb_was_down;
        bool just_released = !is_down && fb_was_down;

        // 记录按下第一瞬间是否在悬浮窗内
        if (just_pressed) {
            fb_touch_started_on_button = hovering_any_fb;
        } else if (!is_down && !just_released) {
            fb_touch_started_on_button = false;
        }
        fb_was_down = is_down;

        // 核心动态拦截逻辑：只有触摸起点在悬浮窗上，或者当前鼠标悬停（PC模拟器支持），才阻断输入
        bool should_intercept = false;
        if (is_down) {
            should_intercept = fb_touch_started_on_button;
        } else if (just_released) {
            should_intercept = fb_touch_started_on_button; // 保持拦截状态以完成点击判定
        } else {
            should_intercept = hovering_any_fb;
        }

        ImGuiWindowFlags fb_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
        if (!should_intercept) {
            // 如果不该拦截，瞬间赋予完全穿透属性，把所有操作让给游戏！
            fb_flags |= ImGuiWindowFlags_NoInputs; 
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(this->surfaceWidth, this->surfaceHigh), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        
        ImGui::Begin("FloatButtonsLayer", nullptr, fb_flags);
        ImDrawList* fbList = ImGui::GetWindowDrawList();
        
        // 首次或手动重置位置打散布局
        if (fb_reset_pos) {
            float start_y = this->surfaceHigh * 0.2f;
            for (int i = 0; i < 6; i++) {
                al_mods[i].target_fb_x = this->surfaceWidth * 0.8f;
                al_mods[i].target_fb_y = start_y + i * 80.0f * fb_scale;
            }
            fb_reset_pos = false;
        }

        // 倒序检测触摸优先级 (召唤顺序越后，图层越靠前)
        int dragging_idx = -1;
        for (int i = 5; i >= 0; i--) { 
            if (*al_mods[i].show_fb && al_mods[i].fb_dragging) { dragging_idx = i; break; }
        }

        // 正序渲染保证底层在下
        for (int i = 0; i < 6; i++) {
            auto& fbm = al_mods[i];
            if (!*fbm.show_fb) continue;
            
            std::string d_name = tr(fbm.name, getModCn(fbm.name).c_str());
            float f_sz = fb_font_size * fb_scale;
            ImVec2 t_sz = font->CalcTextSizeA(f_sz, 10000.0f, 0.0f, d_name.c_str());
            
            float tgt_w = t_sz.x + 30.0f * fb_scale;
            float tgt_h = t_sz.y + 20.0f * fb_scale;
            
            fbm.fb_anim_w += (tgt_w - fbm.fb_anim_w) * 15.0f * io->DeltaTime;
            fbm.fb_anim_h += (tgt_h - fbm.fb_anim_h) * 15.0f * io->DeltaTime;
            
            fbm.current_fb_x += (fbm.target_fb_x - fbm.current_fb_x) * 20.0f * io->DeltaTime;
            fbm.current_fb_y += (fbm.target_fb_y - fbm.current_fb_y) * 20.0f * io->DeltaTime;
            
            float cx = fbm.current_fb_x, cy = fbm.current_fb_y;
            float cw = fbm.fb_anim_w, ch = fbm.fb_anim_h;
            
            ImVec2 p_min(cx, cy), p_max(cx + cw, cy + ch);
            bool hovered = (io->MousePos.x >= p_min.x && io->MousePos.x <= p_max.x && io->MousePos.y >= p_min.y && io->MousePos.y <= p_max.y);
            
            // 只有当触摸起点在按钮上时，才允许交互（防止外部滑动经过时误触）
            if (hovered && is_down && fb_touch_started_on_button && (dragging_idx == -1 || dragging_idx == i)) {
                fbm.fb_hold_time += io->DeltaTime;
                if (fbm.fb_hold_time > 1.0f) { fbm.fb_dragging = true; dragging_idx = i; }
            } else {
                if (hovered && just_released && !fbm.fb_dragging && fb_touch_started_on_button && (dragging_idx == -1 || dragging_idx == i)) {
                    *fbm.state = !*fbm.state;
                    if (std::string(fbm.name) != "Scaffold" && std::string(fbm.name) != "ClickGui") {
                        TriggerNotification(fbm.name, *fbm.state);
                    }
                }
                if (!is_down) { fbm.fb_hold_time = 0.0f; fbm.fb_dragging = false; }
            }
            
            if (fbm.fb_dragging) {
                fbm.target_fb_x += io->MouseDelta.x;
                fbm.target_fb_y += io->MouseDelta.y;
            }
            
            // Draw Shadow
            if (fb_shadow_size > 0.0f && fb_shadow_alpha > 0.0f) {
                int steps = 20; float strk = (fb_shadow_size / steps) * 1.5f;
                for (int s = 1; s <= steps; s++) {
                    float t = (float)s / steps; float alpha = fb_shadow_alpha * (1.0f - t) * (1.0f - t);
                    float expand = t * fb_shadow_size * fb_scale;
                    fbList->AddRect(ImVec2(cx - expand, cy - expand), ImVec2(cx + cw + expand, cy + ch + expand), IM_COL32(0,0,0, (int)(alpha*255.0f)), (fb_rounding * fb_scale) + expand, 0, strk);
                }
            }
            
            // Draw Bg
            fbList->AddRectFilled(p_min, p_max, IM_COL32(0,0,0, (int)(fb_bg_alpha*255.0f)), fb_rounding * fb_scale);
            
            // Draw Text
            float t_alpha = *fbm.state ? fb_text_alpha_on : fb_text_alpha_off;
            ImU32 txt_col;
            if (*fbm.state && fb_chroma) {
                txt_col = ImGui::ColorConvertFloat4ToU32(LerpColorMulti(themeColors, waveTime * waveSpeed + i * 0.2f, t_alpha));
            } else {
                txt_col = *fbm.state ? ImGui::ColorConvertFloat4ToU32(ImVec4(themeColors[0].x, themeColors[0].y, themeColors[0].z, t_alpha)) : IM_COL32(180, 180, 180, (int)(t_alpha * 255.0f));
            }
            fbList->AddText(font, f_sz, ImVec2(cx + 15.0f * fb_scale, cy + 10.0f * fb_scale), txt_col, d_name.c_str());
            
            // Progress bar for Hold-to-Drag
            if (fbm.fb_hold_time > 0.0f && !fbm.fb_dragging) {
                float prog = CLAMP_F(fbm.fb_hold_time / 1.0f, 0.0f, 1.0f);
                fbList->AddRectFilled(ImVec2(cx, p_max.y - 4.0f * fb_scale), ImVec2(cx + cw * prog, p_max.y), IM_COL32(255, 255, 255, 180), fb_rounding * fb_scale);
            }
        }
        ImGui::End(); ImGui::PopStyleColor(1); ImGui::PopStyleVar(2);

        imguiMainWinEnd();
        if (mEglDisplay != EGL_NO_DISPLAY && mEglSurface != EGL_NO_SURFACE) { this->swapBuffers(); }
        if (input != nullptr) { input->fps = currentFps; } usleep(16000); 
    }
}

int EGL::swapBuffers() {
    if (eglSwapBuffers(mEglDisplay, mEglSurface)) {
        return 1;
    }
    return 0;
}

void EGL::clearBuffers() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void EGL::imguiMainWinStart() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
}

void EGL::imguiMainWinEnd() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EGL::setSaveSettingsdir(std::string &dir) {
    this->SaveDir = dir;
}

void EGL::setinput(ImguiAndroidInput *input_) {
    this->input = input_;
}