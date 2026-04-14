#include "EGL.h"
#include <string> 
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <float.h>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------
// 基础 EGL 结构保持不变
// ---------------------------------------------------------

EGL::EGL() {
    mEglDisplay = EGL_NO_DISPLAY;
    mEglSurface = EGL_NO_SURFACE;
    mEglConfig  = nullptr;
    mEglContext = EGL_NO_CONTEXT;
}

static bool RunInitImgui;

int EGL::initEgl() {
    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mEglDisplay == EGL_NO_DISPLAY) return -1;
    EGLint *version = new EGLint[2];
    if (!eglInitialize(mEglDisplay, &version[0], &version[1])) return -1;
    const EGLint attribs[] = {EGL_BUFFER_SIZE, 32, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE};
    EGLint num_config;
    if (!eglGetConfigs(mEglDisplay, NULL, 1, &num_config)) return -1;
    if (!eglChooseConfig(mEglDisplay, attribs, &mEglConfig, 1, &num_config)) return -1;
    int attrib_list[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    mEglContext = eglCreateContext(mEglDisplay, mEglConfig, EGL_NO_CONTEXT, attrib_list);
    if (mEglContext == EGL_NO_CONTEXT) return -1;
    mEglSurface = eglCreateWindowSurface(mEglDisplay, mEglConfig, SurfaceWin, NULL);
    if (mEglSurface == EGL_NO_SURFACE) return -1;
    if (!eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) return -1;
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
    g = ImGui::GetCurrentContext();
    style =&ImGui::GetStyle();
    style->ScaleAllSizes(4.0f);
    return 1;
}

void EGL::onSurfaceCreate(JNIEnv *env, jobject surface, int SurfaceWidth, int SurfaceHigh) {
    this->SurfaceWin = ANativeWindow_fromSurface(env, surface);
    this->surfaceWidth = SurfaceWidth;
    this->surfaceHigh = SurfaceHigh;
    SurfaceThread = new std::thread([this] { EglThread(); });
    SurfaceThread->detach();
}

void EGL::onSurfaceChange(int SurfaceWidth, int SurfaceHigh) {
    this->surfaceWidth = SurfaceWidth;
    this->surfaceHigh = SurfaceHigh;
    this->isChage = true;
}

void EGL::onSurfaceDestroy() {
    this->isDestroy = true;
    std::unique_lock<std::mutex> ulo(Threadlk);
    cond.wait(ulo, [this] { return !this->ThreadIo; });
    delete SurfaceThread;
    SurfaceThread = nullptr;
}

// ---------------------------------------------------------
// 核心重写：EglThread
// ---------------------------------------------------------

void EGL::EglThread() {
    if (this->initEgl() != 1) return;
    if (this->initImgui() != 1) return;
    ThreadIo = true;

    // 1. 初始化输入系统 (只做一次)
    if (input == nullptr || io == nullptr) { ThreadIo = false; return; }
    input->initImguiIo(io);
    input->setImguiContext(g);

    // --- 固定参数 ---
    const float winW = 1000.0f;
    const float winH = 750.0f;

    // --- UI 调节变量 ---
    static float sidebarWidth = 220.0f;
    static float itemSpacing = 15.0f;
    static float iconSize = 24.0f;
    static float textSize = 22.0f;
    static float animSpeed = 12.0f;
    static float rectRounding = 15.0f;
    static float rectAlpha = 0.85f;

    // --- Melt 文本变量 ---
    static float meltOffsetX = 20.0f;
    static float meltOffsetY = -45.0f; // 放在矩形左上方外侧

    // --- 侧边栏状态 ---
    static int selectedTab = 0;
    const char* tabs[] = { "Search", "Player", "Render", "Visual", "Client", "Themes", "Language", "Config" };
    // 为每个按钮准备动画位移和透明度 (0.0 - 1.0)
    static float itemAnims[8] = { 0.0f }; 

    while (true) {
        if (this->isDestroy) { ThreadIo = false; cond.notify_all(); return; }
        if (this->isChage) { glViewport(0, 0, this->surfaceWidth, this->surfaceHigh); this->isChage = false; }
        this->clearBuffers();
        if (!ActivityState) { usleep(16000); continue; }

        imguiMainWinStart();
        float dt = io->DeltaTime;

        // 2. 调节控制台 (放在侧边，不遮挡主 UI)
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings Controller");
        ImGui::SliderFloat("Sidebar Width", &sidebarWidth, 150, 400);
        ImGui::SliderFloat("Item Spacing", &itemSpacing, 5, 50);
        ImGui::SliderFloat("Icon Size", &iconSize, 10, 50);
        ImGui::SliderFloat("Text Size", &textSize, 10, 50);
        ImGui::SliderFloat("Background Alpha", &rectAlpha, 0.1f, 1.0f);
        ImGui::SliderFloat("Rounding", &rectRounding, 0, 50);
        ImGui::End();

        // 3. 绘制主 UI 
        // 计算居中位置
        float centerX = (surfaceWidth - winW) / 2.0f;
        float centerY = (surfaceHigh - winH) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2(centerX, centerY));
        ImGui::SetNextWindowSize(ImVec2(winW, winH));
        
        // 核心修正：必须保证这个主窗口能接收输入，且内部元素渲染正确
        ImGui::Begin("MainPanel", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
        
        // 【关键修复】同步窗口指针，解决点击闪退
        this->g_window = ImGui::GetCurrentWindow();
        input->g_window = this->g_window;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p = ImVec2(centerX, centerY);

        // A. 绘制大背景 (带阴影)
        for(int i=1; i<=15; i++) {
            drawList->AddRect(ImVec2(p.x-i, p.y-i), ImVec2(p.x+winW+i, p.y+winH+i), IM_COL32(0,0,0, (int)(180*(1.0f-i/15.0f)*rectAlpha)), rectRounding+i);
        }
        drawList->AddRectFilled(p, ImVec2(p.x+winW, p.y+winH), IM_COL32(15, 15, 15, (int)(255*rectAlpha)), rectRounding);

        // B. 绘制左侧边栏背景
        drawList->AddRectFilled(p, ImVec2(p.x + sidebarWidth, p.y + winH), IM_COL32(25, 25, 25, (int)(255*rectAlpha)), rectRounding, ImDrawFlags_RoundCornersLeft);

        // C. 绘制左上方 "Melt" 文本 (带辉光)
        ImVec2 meltPos = ImVec2(p.x + meltOffsetX, p.y + meltOffsetY);
        for(int i=1; i<=3; i++) {
            drawList->AddText(imFont, 40.0f, ImVec2(meltPos.x, meltPos.y), IM_COL32(255, 255, 255, (int)(50/i)), "Melt");
        }
        drawList->AddText(imFont, 40.0f, meltPos, IM_COL32(255, 255, 255, 255), "Melt");

        // D. 垂直列表渲染
        float startY = p.y + 60.0f;
        for (int i = 0; i < 8; i++) {
            ImVec2 itemPos = ImVec2(p.x + 30.0f, startY + i * (textSize + itemSpacing));
            
            // 检测交互
            ImRect itemRect(itemPos.x, itemPos.y, p.x + sidebarWidth - 20.0f, itemPos.y + textSize + 10.0f);
            bool isHovered = itemRect.Contains(io->MousePos);
            bool isSelected = (selectedTab == i);

            if (isHovered && ImGui::IsMouseClicked(0)) selectedTab = i;

            // 动画目标：选中或悬停时目标为 1.0 (偏移 10px)，否则为 0.0
            float targetAnim = (isHovered || isSelected) ? 1.0f : 0.0f;
            itemAnims[i] += (targetAnim - itemAnims[i]) * animSpeed * dt;

            // 计算属性
            float currentOffset = itemAnims[i] * 10.0f;
            float currentAlpha = 0.7f + (itemAnims[i] * 0.3f); // 从 0.7 到 1.0
            ImU32 col = IM_COL32(255, 255, 255, (int)(255 * currentAlpha));

            // 绘制 Icon (根据你的想法绘制简单的几何图形作为 Demo)
            ImVec2 iconPos = ImVec2(itemPos.x + currentOffset, itemPos.y + (textSize - iconSize)/2.0f);
            drawList->AddRectFilled(iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize), col, 5.0f); // 模拟 Icon
            
            // 绘制文本
            drawList->AddText(imFont, textSize, ImVec2(iconPos.x + iconSize + 15.0f, itemPos.y), col, tabs[i]);

            // 如果是选中状态，画一个左侧提示条
            if (isSelected) {
                drawList->AddRectFilled(ImVec2(p.x, itemPos.y), ImVec2(p.x + 5, itemPos.y + textSize), col, 2.0f);
            }
        }

        ImGui::End();
        imguiMainWinEnd();

        if (mEglDisplay != EGL_NO_DISPLAY && mEglSurface != EGL_NO_SURFACE) { this->swapBuffers(); }
        usleep(16000);
    }
}

// ---------------------------------------------------------
// 其余辅助函数保持不变
// ---------------------------------------------------------

int EGL::swapBuffers() {
    return eglSwapBuffers(mEglDisplay, mEglSurface) ? 1 : 0;
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

void EGL::setSaveSettingsdir(std::string &dir) { this->SaveDir = dir; }
void EGL::setinput(ImguiAndroidInput *input_) { this->input = input_; }