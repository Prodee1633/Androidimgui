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
    
    // 基础配置变量
    static float targetWidth = 400.0f, targetHeight = 250.0f;
    static float targetPosX = 100.0f, targetPosY = 100.0f;
    static float targetAlpha = 0.8f, targetRounding = 20.0f;
    
    // 文本 Melt 变量
    static float textScale = 1.0f;
    static float textOffsetX = 20.0f, textOffsetY = 20.0f;
    static float textGlowIntensity = 5.0f; // 辉光范围
    static ImVec4 textGlowColor = ImVec4(1, 1, 1, 0.5f); // 辉光颜色

    // 动画平滑变量
    static float animWidth = 0, animHeight = 0;
    static float animPosX = 0, animPosY = 0;
    static float animAlpha = 0;
    static float animSpeed = 10.0f; // 动画速度

    while (true) {
        if (this->isDestroy) { ThreadIo = false; cond.notify_all(); return; }
        if (this->isChage) { glViewport(0, 0, this->surfaceWidth, this->surfaceHigh); this->isChage = false; }
        this->clearBuffers();
        if (!ActivityState) { usleep(16000); continue; }
        
        imguiMainWinStart();
        
        // 1. 鲁棒性检查，防止闪退
        if (input == nullptr || io == nullptr) { 
            ImGui::EndFrame(); 
            usleep(16000); 
            continue; 
        }
        input->initImguiIo(io); 
        input->setImguiContext(g);

        float dt = io->DeltaTime;

        // 2. 线性过渡动画计算 (Lerp)
        animWidth  += (targetWidth - animWidth)   * animSpeed * dt;
        animHeight += (targetHeight - animHeight) * animSpeed * dt;
        animPosX   += (targetPosX - animPosX)     * animSpeed * dt;
        animPosY   += (targetPosY - animPosY)     * animSpeed * dt;
        animAlpha  += (targetAlpha - animAlpha)   * animSpeed * dt;

        // 3. 调节菜单 (控制台)
        ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin("UI控制器");
        ImGui::Text("矩形设置");
        ImGui::SliderFloat("宽度", &targetWidth, 100, 1000);
        ImGui::SliderFloat("高度", &targetHeight, 100, 1000);
        ImGui::SliderFloat("位置X", &targetPosX, 0, surfaceWidth);
        ImGui::SliderFloat("位置Y", &targetPosY, 0, surfaceHigh);
        ImGui::SliderFloat("透明度", &targetAlpha, 0, 1.0f);
        ImGui::SliderFloat("圆角", &targetRounding, 0, 100);
        ImGui::SliderFloat("动画速度", &animSpeed, 1, 30);
        
        ImGui::Separator();
        ImGui::Text("文本 Melt 设置");
        ImGui::SliderFloat("文本大小", &textScale, 0.5f, 3.0f);
        ImGui::SliderFloat("文本偏移X", &textOffsetX, -100, 200);
        ImGui::SliderFloat("文本偏移Y", &textOffsetY, -100, 200);
        ImGui::SliderFloat("辉光强度", &textGlowIntensity, 0, 20);
        ImGui::ColorEdit4("辉光颜色", (float*)&textGlowColor);
        ImGui::End();

        // 4. 动画矩形绘制
        // 使用一个全屏覆盖层作为画布，防止窗口标题栏干扰，这也是解决“一摸就闪退”的有效方法
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(surfaceWidth, surfaceHigh));
        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // 矩形绝对坐标
        ImVec2 rectMin = ImVec2(animPosX, animPosY);
        ImVec2 rectMax = ImVec2(animPosX + animWidth, animPosY + animHeight);

        // A. 绘制背景阴影
        for (int i = 1; i <= 10; i++) {
            float shadowAlpha = (1.0f - (i / 10.0f)) * 0.2f * animAlpha;
            drawList->AddRect(
                ImVec2(rectMin.x - i * 2, rectMin.y - i * 2),
                ImVec2(rectMax.x + i * 2, rectMax.y + i * 2),
                IM_COL32(0, 0, 0, (int)(shadowAlpha * 255)),
                targetRounding + i * 2, 0, 2.0f
            );
        }

        // B. 绘制黑色主矩形
        drawList->AddRectFilled(rectMin, rectMax, IM_COL32(0, 0, 0, (int)(animAlpha * 255)), targetRounding);

        // C. 绘制文本 "Melt" (位置跟随矩形动画)
        ImVec2 textPos = ImVec2(rectMin.x + textOffsetX, rectMin.y + textOffsetY);
        float fontSize = 32.0f * textScale;
        
        // 绘制文本辉光 (通过多层叠加)
        if (textGlowIntensity > 0.1f) {
            for (int i = 1; i <= 3; i++) {
                float glowAlpha = (1.0f - (i / 3.0f)) * textGlowColor.w * animAlpha;
                ImU32 gCol = ImGui::ColorConvertFloat4ToU32(ImVec4(textGlowColor.x, textGlowColor.y, textGlowColor.z, glowAlpha));
                float offset = (i * textGlowIntensity) / 3.0f;
                // 四周偏移绘制模拟模糊
                drawList->AddText(imFont, fontSize, ImVec2(textPos.x + offset, textPos.y), gCol, "Melt");
                drawList->AddText(imFont, fontSize, ImVec2(textPos.x - offset, textPos.y), gCol, "Melt");
                drawList->AddText(imFont, fontSize, ImVec2(textPos.x, textPos.y + offset), gCol, "Melt");
                drawList->AddText(imFont, fontSize, ImVec2(textPos.x, textPos.y - offset), gCol, "Melt");
            }
        }
        
        // 绘制主文本
        drawList->AddText(imFont, fontSize, textPos, IM_COL32(255, 255, 255, (int)(animAlpha * 255)), "Melt");

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