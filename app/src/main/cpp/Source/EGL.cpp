//
// Created by admin on 2022/6/10.
//

#include "EGL.h"
//#include "KingImage.h"
//#include "ImGuiDraw.h"

// 语言枚举
enum Language {
    LANG_CN = 0,
    LANG_EN = 1
};

// 当前语言
static Language currentLang = LANG_CN;

// 多语言文本
const char* GetText(const char* cn, const char* en) {
    return (currentLang == LANG_CN) ? cn : en;
}

// 模块名称 - 中英文
const char* moduleNamesCN[] = {
    "自瞄", "透视", "无后坐", "加速", "飞天", "遁地", "无敌", "秒杀", "吸星", "传送"
};

const char* moduleNamesEN[] = {
    "Aim", "ESP", "NoRecoil", "Speed", "Fly", "Noclip", "God", "Kill", "Magnet", "Teleport"
};

const char* GetModuleName(int index) {
    return (currentLang == LANG_CN) ? moduleNamesCN[index] : moduleNamesEN[index];
}

// 设置项名称 - 中英文
const char* settingNamesCN[] = {
    "开关", "强度", "范围", "速度", "延迟", "模式", "透明度", "大小", "颜色", "距离"
};

const char* settingNamesEN[] = {
    "Toggle", "Power", "Range", "Speed", "Delay", "Mode", "Alpha", "Size", "Color", "Dist"
};

const char* GetSettingName(int index) {
    return (currentLang == LANG_CN) ? settingNamesCN[index] : settingNamesEN[index];
}

// 动画插值函数
float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// 辉光文本绘制 - 使用字体颜色作为辉光颜色，辉光效果0到3
void DrawGlowTextAnimated(ImDrawList* drawList, ImVec2 pos, const char* text, float glowIntensity, ImVec4 textColor) {
    ImU32 mainColor = ImGui::ColorConvertFloat4ToU32(textColor);
    float maxGlow = 3.0f;
    float currentGlow = glowIntensity * maxGlow;
    int layers = (int)(currentGlow * 2.0f);
    if (layers < 1) layers = 1;
    
    for (int i = layers; i >= 1; i--) {
        float alpha = (80.0f / i) * glowIntensity;
        ImU32 glowColor = IM_COL32(
            (int)(textColor.x * 255),
            (int)(textColor.y * 255),
            (int)(textColor.z * 255),
            (int)alpha
        );
        float offset = i * 0.8f;
        drawList->AddText(ImVec2(pos.x - offset, pos.y), glowColor, text);
        drawList->AddText(ImVec2(pos.x + offset, pos.y), glowColor, text);
        drawList->AddText(ImVec2(pos.x, pos.y - offset), glowColor, text);
        drawList->AddText(ImVec2(pos.x, pos.y + offset), glowColor, text);
    }
    drawList->AddText(pos, mainColor, text);
}

// 按键绑定按钮
void KeybindButton(const char* label, int* keybind, float width = 80.0f) {
    ImVec2 buttonSize = ImVec2(width, ImGui::GetFrameHeight() * 0.6f);
    const char* displayText = (*keybind == 0) ? "None" : "Key";
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    
    if (ImGui::Button(displayText, buttonSize)) {
        // 按键绑定逻辑
    }
    
    ImGui::PopStyleColor(3);
}

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
        LOGE("eglGetDisplay error=%u", glGetError());
        return -1;
    }
    LOGE("生成mEglDisplay");
    
    EGLint *version = new EGLint[2];
    if (!eglInitialize(mEglDisplay, &version[0], &version[1])) {
        LOGE("eglInitialize error=%u", glGetError());
        return -1;
    }
    LOGE("eglInitialize成功");
    
    const EGLint attribs[] = {EGL_BUFFER_SIZE, 32, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                              EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE};

    EGLint num_config;
    if (!eglGetConfigs(mEglDisplay, NULL, 1, &num_config)) {
        LOGE("eglGetConfigs  error =%u", glGetError());
        return -1;
    }
    LOGE("num_config=%d", num_config);
    
    if (!eglChooseConfig(mEglDisplay, attribs, &mEglConfig, 1, &num_config)) {
        LOGE("eglChooseConfig  error=%u", glGetError());
        return -1;
    }
    LOGE("eglChooseConfig成功");
    
    int attrib_list[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    mEglContext = eglCreateContext(mEglDisplay, mEglConfig, EGL_NO_CONTEXT, attrib_list);
    if (mEglContext == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext  error = %u", glGetError());
        return -1;
    }
    
    mEglSurface = eglCreateWindowSurface(mEglDisplay, mEglConfig, SurfaceWin, NULL);
    if (mEglSurface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface  error = %u", glGetError());
        return -1;
    }
    LOGE("eglCreateWindowSurface成功");
    
    if (!eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
        LOGE("eglMakeCurrent  error = %u", glGetError());
        return -1;
    }
    LOGE("eglMakeCurrent成功");
    return 1;
}

int EGL::initImgui() {
    if (RunInitImgui) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplAndroid_Init(this->SurfaceWin);
        ImGui_ImplOpenGL3_Init("#version 300 es");
        return 1;
    }
    RunInitImgui = true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    LOGE("CreateContext成功");
    io = &ImGui::GetIO();
    io->IniSavingRate = 10.0f;
    string SaveFile = this->SaveDir;
    SaveFile += "/save.ini";
    io->IniFilename = SaveFile.c_str();

    ImGui_ImplAndroid_Init(this->SurfaceWin);
    LOGE(" ImGui_ImplAndroid_Init成功");
    ImGui_ImplOpenGL3_Init("#version 300 es");
    LOGE(" ImGui_ImplOpenGL3_Init成功");
    
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    imFont = io->Fonts->AddFontFromMemoryTTF((void *) OPPOSans_H, OPPOSans_H_size, 32.0f, &font_cfg, io->Fonts->GetGlyphRangesChineseFull());
    io->MouseDoubleClickTime = 0.0001f;
    LOGE(" Font成功");
    
    g = ImGui::GetCurrentContext();
    style = &ImGui::GetStyle();
    style->ScaleAllSizes(4.0f);
    style->FramePadding = ImVec2(10.0f, 20.0f);

    string LoadFile = this->SaveDir;
    LoadFile += "/Style.dat";
    ImGuiStyle s;
    if (MyFile::ReadFile(&s, LoadFile.c_str()) == 1) {
        *style = s;
        LOGE("读取主题成功");
    }

    return 1;
}

void EGL::onSurfaceCreate(JNIEnv *env, jobject surface, int SurfaceWidth, int SurfaceHigh) {
    LOGE("onSurfaceCreate");
    this->SurfaceWin       = ANativeWindow_fromSurface(env, surface);
    this->surfaceWidth     = SurfaceWidth;
    this->surfaceHigh      = SurfaceHigh;
    this->surfaceWidthHalf = this->surfaceWidth / 2;
    this->surfaceHighHalf  = this->surfaceHigh / 2;
    SurfaceThread = new std::thread([this] { EglThread(); });
    SurfaceThread->detach();
    LOGE("onSurfaceCreate_end");
}

void EGL::onSurfaceChange(int SurfaceWidth, int SurfaceHigh) {
    this->surfaceWidth     = SurfaceWidth;
    this->surfaceHigh      = SurfaceHigh;
    this->surfaceWidthHalf = this->surfaceWidth / 2;
    this->surfaceHighHalf  = this->surfaceHigh / 2;
    this->isChage          = true;
    LOGE("onSurfaceChange");
}

void EGL::onSurfaceDestroy() {
    LOGE("onSurfaceDestroy");
    this->isDestroy = true;
    std::unique_lock<std::mutex> ulo(Threadlk);
    cond.wait(ulo, [this] { return !this->ThreadIo; });
    delete SurfaceThread;
    SurfaceThread = nullptr;
}

// 模块数据
static int moduleCount = 10;
static int selectedModule = 0;
static float moduleOpacity[10] = {1.0f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f};
static float moduleGlowAnim[10] = {0.0f};
static int moduleKeybinds[10] = {0};

// 设置数据
static float settingValues[10][10] = {{0}};
static int settingKeybinds[10][10] = {{0}};
static bool settingToggles[10][10] = {{false}};

// 导航栏动画
static float navIndicatorX = 0.0f;
static float navIndicatorWidth = 0.0f;

// Client Name
static char clientName[64] = "Fate";

// 全局圆角
static float globalRounding = 8.0f;

// 主题颜色透明度
static float themeAlpha = 1.0f;

// 字体颜色
static ImVec4 fontColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

void EGL::EglThread() {
    LOGE("imguidgsbdf成");
    this->initEgl();
    this->initImgui();
    ThreadIo = true;
    input->initImguiIo(io);
    input->setImguiContext(g);
    input->setwin(this->g_window);

    while (true) {
        if (this->isChage) {
            glViewport(0, 0, this->surfaceWidth, this->surfaceHigh);
            this->isChage = false;
        }
        if (this->isDestroy) {
            this->isDestroy = false;
            ThreadIo = false;
            cond.notify_all();
            return;
        }

        this->clearBuffers();
        if (!ActivityState) continue;
        
        imguiMainWinStart();
        
        ImGui::SetNextWindowBgAlpha(0.7);
        style->WindowTitleAlign = ImVec2(0.5, 0.5);
        
        if (input->Scrollio && !input->Activeio) {
            input->funScroll(g->WheelingWindow ? g->WheelingWindow : g->HoveredWindow);
        }

        // 主菜单窗口
        ImGui::Begin(clientName, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
        input->g_window = g_window = ImGui::GetCurrentWindow();
        
        ImVec2 winSize = ImGui::GetWindowSize();
        ImVec2 winPos = ImGui::GetWindowPos();
        
        // 设置滚动条宽度
        style->ScrollbarSize = 27.8f;
        
        // 左侧面板宽度
        float leftPanelWidth = 200.0f;
        float separatorWidth = 20.0f;
        float scrollbarWidth = 27.8f;
        
        // 右侧面板宽度（考虑滚动条位置）
        float rightPanelWidth = winSize.x - leftPanelWidth - separatorWidth - scrollbarWidth - 30.0f;
        
        // 应用全局圆角
        style->FrameRounding = globalRounding;
        style->WindowRounding = globalRounding;
        style->ScrollbarRounding = globalRounding;
        style->GrabRounding = globalRounding;
        style->PopupRounding = globalRounding;
        style->TabRounding = globalRounding;
        
        // 应用主题透明度
        ImVec4 themeColor = style->Colors[ImGuiCol_Button];
        themeColor.w = themeAlpha;
        style->Colors[ImGuiCol_Button] = themeColor;
        style->Colors[ImGuiCol_ButtonHovered] = ImVec4(themeColor.x + 0.1f, themeColor.y + 0.1f, themeColor.z + 0.1f, themeAlpha);
        style->Colors[ImGuiCol_ButtonActive] = ImVec4(themeColor.x + 0.2f, themeColor.y + 0.2f, themeColor.z + 0.2f, themeAlpha);
        style->Colors[ImGuiCol_SliderGrab] = themeColor;
        style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(themeColor.x + 0.2f, themeColor.y + 0.2f, themeColor.z + 0.2f, themeAlpha);
        
        // 应用字体颜色
        style->Colors[ImGuiCol_Text] = fontColor;
        
        // 导航栏 - 更慢的动画速度
        const float navAnimSpeed = 0.08f;
        
        ImGui::SetCursorPos(ImVec2(10, 50));
        ImGui::BeginGroup();
        
        // 计算导航指示器位置
        float navItemWidth = leftPanelWidth / 3.0f;
        float targetNavX = selectedModule < 3 ? selectedModule * navItemWidth : 0;
        navIndicatorX = Lerp(navIndicatorX, targetNavX, navAnimSpeed);
        navIndicatorWidth = Lerp(navIndicatorWidth, navItemWidth * 0.8f, navAnimSpeed);
        
        // 绘制导航指示器
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 navPos = ImGui::GetCursorScreenPos();
        drawList->AddRectFilled(
            ImVec2(navPos.x + navIndicatorX, navPos.y + 35),
            ImVec2(navPos.x + navIndicatorX + navIndicatorWidth, navPos.y + 38),
            ImGui::ColorConvertFloat4ToU32(ImVec4(themeColor.x, themeColor.y, themeColor.z, themeAlpha)),
            2.0f
        );
        
        // 模块分类按钮
        for (int i = 0; i < 3; i++) {
            if (i > 0) ImGui::SameLine();
            const char* navText = (i == 0) ? GetText("战斗", "Combat") : (i == 1) ? GetText("视觉", "Visual") : GetText("移动", "Move");
            if (ImGui::Button(navText, ImVec2(navItemWidth - 5, 35))) {
                selectedModule = i * 3;
            }
        }
        ImGui::EndGroup();
        
        // 分隔线
        ImGui::SetCursorPos(ImVec2(10, 95));
        ImGui::Separator();
        
        // 左侧模块列表 - 带滚动条
        ImGui::SetCursorPos(ImVec2(10, 105));
        ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, winSize.y - 150), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        
        // 更新模块动画 - 只有透明度和辉光，没有向右偏移
        for (int i = 0; i < moduleCount; i++) {
            float targetGlow = (i == selectedModule) ? 1.0f : 0.0f;
            float targetOpacity = (i == selectedModule) ? 1.0f : 0.6f;
            
            // 辉光动画
            if (moduleGlowAnim[i] < targetGlow) {
                moduleGlowAnim[i] += 0.04f;
                if (moduleGlowAnim[i] > targetGlow) moduleGlowAnim[i] = targetGlow;
            } else if (moduleGlowAnim[i] > targetGlow) {
                moduleGlowAnim[i] -= 0.04f;
                if (moduleGlowAnim[i] < targetGlow) moduleGlowAnim[i] = targetGlow;
            }
            
            // 透明度动画
            if (moduleOpacity[i] < targetOpacity) {
                moduleOpacity[i] += 0.04f;
                if (moduleOpacity[i] > targetOpacity) moduleOpacity[i] = targetOpacity;
            } else if (moduleOpacity[i] > targetOpacity) {
                moduleOpacity[i] -= 0.04f;
                if (moduleOpacity[i] < targetOpacity) moduleOpacity[i] = targetOpacity;
            }
        }
        
        // 绘制模块按钮
        for (int i = 0; i < moduleCount; i++) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, moduleOpacity[i]);
            
            const char* moduleName = GetModuleName(i);
            ImVec2 textPos = ImGui::GetCursorScreenPos();
            
            // 使用辉光效果绘制文本 - 辉光颜色使用字体颜色
            if (moduleGlowAnim[i] > 0.01f) {
                DrawGlowTextAnimated(drawList, textPos, moduleName, moduleGlowAnim[i], fontColor);
            } else {
                ImGui::TextColored(fontColor, "%s", moduleName);
            }
            
            // 点击选择模块
            ImVec2 textSize = ImGui::CalcTextSize(moduleName);
            ImGui::SetCursorScreenPos(textPos);
            if (ImGui::InvisibleButton(moduleName, ImVec2(leftPanelWidth - 20, textSize.y + 10))) {
                selectedModule = i;
            }
            
            ImGui::PopStyleVar();
            ImGui::Spacing();
        }
        
        ImGui::EndChild();
        
        // 分隔线
        ImGui::SetCursorPos(ImVec2(leftPanelWidth + 20, 105));
        ImGui::Separator();
        
        // 右侧设置面板 - 带滚动条
        ImGui::SetCursorPos(ImVec2(leftPanelWidth + 30, 105));
        ImGui::BeginChild("RightPanel", ImVec2(rightPanelWidth, winSize.y - 150), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        
        // 当前选中模块的设置
        ImGui::TextColored(fontColor, "%s %s", GetText("设置", "Settings"), GetModuleName(selectedModule));
        ImGui::Spacing();
        
        // 10个设置项
        for (int i = 0; i < 10; i++) {
            ImGui::PushID(i);
            
            // 设置名称
            ImGui::TextColored(fontColor, "%s %d", GetSettingName(i), i + 1);
            
            // 开关
            ImGui::SameLine(rightPanelWidth * 0.4f);
            ImGui::Checkbox("##toggle", &settingToggles[selectedModule][i]);
            
            // 滑块
            ImGui::SameLine(rightPanelWidth * 0.55f);
            ImGui::SetNextItemWidth(rightPanelWidth * 0.25f);
            ImGui::SliderFloat("##slider", &settingValues[selectedModule][i], 0.0f, 100.0f, "");
            
            // 按键绑定
            ImGui::SameLine(rightPanelWidth * 0.82f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 2));
            KeybindButton("##keybind", &settingKeybinds[selectedModule][i], rightPanelWidth * 0.15f);
            ImGui::PopStyleVar();
            
            ImGui::PopID();
            ImGui::Spacing();
        }
        
        ImGui::EndChild();
        
        // Interface设置按钮
        ImGui::SetCursorPos(ImVec2(winSize.x - 120, 10));
        if (ImGui::Button(GetText("设置", "Settings"), ImVec2(100, 35))) {
            ImGui::OpenPopup("InterfaceSettings");
        }
        
        // Interface设置弹窗
        if (ImGui::BeginPopupModal("InterfaceSettings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(fontColor, "%s", GetText("界面设置", "Interface Settings"));
            ImGui::Separator();
            
            // Client Name输入
            ImGui::TextColored(fontColor, "%s", GetText("客户端名称:", "Client Name:"));
            ImGui::InputText("##clientname", clientName, sizeof(clientName));
            
            // 语言切换
            ImGui::TextColored(fontColor, "%s", GetText("语言:", "Language:"));
            if (ImGui::Button(currentLang == LANG_CN ? "中文" : "English", ImVec2(100, 30))) {
                currentLang = (currentLang == LANG_CN) ? LANG_EN : LANG_CN;
            }
            
            // 全局圆角
            ImGui::TextColored(fontColor, "%s", GetText("全局圆角:", "Global Rounding:"));
            ImGui::SliderFloat("##rounding", &globalRounding, 0.0f, 20.0f, "%.1f");
            
            // 主题透明度
            ImGui::TextColored(fontColor, "%s", GetText("主题透明度:", "Theme Alpha:"));
            ImGui::SliderFloat("##alpha", &themeAlpha, 0.1f, 1.0f, "%.2f");
            
            // 字体颜色
            ImGui::TextColored(fontColor, "%s", GetText("字体颜色:", "Font Color:"));
            ImGui::ColorEdit3("##fontcolor", (float*)&fontColor);
            
            ImGui::Separator();
            if (ImGui::Button(GetText("关闭", "Close"), ImVec2(100, 30))) {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::End();
        
        imguiMainWinEnd();
        this->swapBuffers();
        input->fps = this->FPS;
    }
}

int EGL::swapBuffers() {
    if (eglSwapBuffers(mEglDisplay, mEglSurface)) {
        return 1;
    }
    LOGE("eglSwapBuffers  error = %u", glGetError());
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

void EGL::setSaveSettingsdir(string &dir) {
    this->SaveDir = dir;
    LOGE("保存路径=%s", SaveDir.c_str());
}

void EGL::setinput(ImguiAndroidInput *input_) {
    this->input = input_;
}
