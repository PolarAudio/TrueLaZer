// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#define _WIN32_WINNT 0x0601 // Windows 7
#include "imgui.h"
#include <stdio.h> // Ensure stdio is included for printf
#include <iostream>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <stdio.h> // Ensure stdio is included for printf
#include <vector>
#include <string>
#include <filesystem>
#include "Showbridge.h"
#include "ClipDeck.h"
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <windows.h> // For GetModuleFileNameA

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

namespace fs = std::filesystem;

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Helper function to get the executable's directory
std::string GetExecutablePath()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("/\\"); // Fix here
    return std::string(buffer).substr(0, pos);
}

void GenerateSampleILDAFrame(frame_buffer& fb)
{
    // Clear existing points
    fb.count = 0;

    const int ILDA_MAX_COORD = 32767;
    const int NUM_POINTS_TO_GENERATE = 383; // Target a number of points to get closer to 4612 bytes

    // Generate a simple line back and forth
    for (int i = 0; i < NUM_POINTS_TO_GENERATE; ++i) {
        fb.points[fb.count].x = static_cast<int>((i % 2 == 0 ? -0.5f : 0.5f) * ILDA_MAX_COORD);
        fb.points[fb.count].y = static_cast<int>((i % 2 == 0 ? -0.5f : 0.5f) * ILDA_MAX_COORD);
        fb.points[fb.count].blanking = 1;
        fb.points[fb.count].r = 255;
        fb.points[fb.count].g = 255;
        fb.points[fb.count].b = 255;
        fb.count++;
    }

    fb.status = 0;
    fb.delay = 0;
}

void RenderILDAFrame(const frame_buffer& fb)
{
    if (fb.count == 0) return;

    // Disable texturing to prevent font atlas from being applied to our lines
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_TEXTURE_2D);

    glPushMatrix();
    glScalef(1.0f, -1.0f, 1.0f); // Flip Y-axis for ILDA coordinates (Y-up)

    glBegin(GL_POINTS);
    for (int i = 0; i < fb.count; ++i)
    {
        if (fb.points[i].blanking == 1) // Only draw light points
        {
            glColor3ub(fb.points[i].r, fb.points[i].g, fb.points[i].b);
            glVertex2f(fb.points[i].x, fb.points[i].y);
        }
    }
    glEnd();

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < fb.count; ++i)
    {
        if (fb.points[i].blanking == 1) // Only draw light points
        {
            glColor3ub(fb.points[i].r, fb.points[i].g, fb.points[i].b);
            glVertex2f(fb.points[i].x, fb.points[i].y);
        }
        else
        {
            // If it's a blanking point, end the current line strip and start a new one
            glEnd();
            glBegin(GL_LINE_STRIP);
        }
    }
    glEnd();

    glPopMatrix();

    // Re-enable texturing
    glPopAttrib();
}

void loadClips(ClipDeck& clipDeck, const std::string& directory) {
    // Clear existing clips before loading new ones
    clipDeck.clear();
    std::cout << "Loading clips from: " << directory << std::endl;

    try {
        if (!fs::exists(directory) || !fs::is_directory(directory)) {
            std::cerr << "Error: Directory not found or not a directory: " << directory << std::endl;
            std::cerr << "Current working directory is: " << fs::current_path() << std::endl;
            return;
        }

        int row = 0;
        int col = 0;
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file() && (entry.path().extension() == ".ild" || entry.path().extension() == ".ILD")) {
                std::string path = entry.path().string();
                std::cout << "Found ILDA file: " << path << std::endl;
                std::string filename = entry.path().filename().string();
                size_t lastdot = filename.find_last_of(".");
                std::string name = (lastdot == std::string::npos) ? filename : filename.substr(0, lastdot);
                clipDeck.setClip(row, col, new Clip(name, path));
                col++;
                if (col >= clipDeck.getCols()) {
                    col = 0;
                    row++;
                    if (row >= clipDeck.getRows()) {
                        break;
                    }
                }
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        std::cerr << "Path: " << directory << std::endl;
        std::cerr << "Current working directory is: " << fs::current_path() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "An unknown error occurred while loading clips.";
    }

    std::cout << "Finished loading clips.";
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    printf("Initializing GLFW...\n");
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }
    printf("GLFW window created successfully.\n");

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    printf("Creating GLFW window...\n");
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "TrueLazer", nullptr, nullptr);
    if (window == nullptr)
    {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    printf("GLFW window created successfully.\n");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
    io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.
#endif

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // TrueLazer variables
    Showbridge showbridge;
    // Initialize Showbridge (WSAStartup)
    if (!showbridge.initialize()) { // This is where WSAStartup is called
        fprintf(stderr, "Failed to initialize Showbridge (Winsock). Error: %d\n", WSAGetLastError());
        return 1;
    }
    bool device_selected = false;
    frame_buffer g_ilda_frame = {0}; // Global ILDA frame for rendering
    std::vector<std::string> g_local_interfaces = showbridge.getNetworkInterfaces(); // Store local interface IPs for diagnostics
    std::string g_selected_local_interface_ip; // Store the selected local interface IP
    if (!g_local_interfaces.empty()) {
        g_selected_local_interface_ip = g_local_interfaces[0]; // Select the first interface by default
    }

    ClipDeck clipDeck(4, 8);
    std::cout << "Loading clips..." << std::endl;
    std::string ilda_files_path = GetExecutablePath() + "\\..\\ILDA-FILE-FORMAT-FILES"; // Corrected path
    printf("Executable Path: %s\n", GetExecutablePath().c_str());
    printf("ILDA Files Path: %s\n", ilda_files_path.c_str());
    loadClips(clipDeck, ilda_files_path);
    std::cout << "Clips loaded.";


    // Main loop
    std::cout << "Entering main loop..." << std::endl;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        // TrueLazer Main Window (full-screen, invisible, contains dockspace)
                ImGuiWindowFlags host_window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("TrueLazerMainWindow", nullptr, host_window_flags);
        ImGui::PopStyleVar(3);

        // DockSpace
        ImGuiID dockspace_id = ImGui::GetID("TrueLazerDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        // SDK Control Window
        ImGui::Begin("SDK Control");
        std::lock_guard<std::mutex> lock(showbridge.dacMutex); // Protect access to discoveredDACs

        // --- Network Diagnostics ---
        // Removed: Refresh Network Interfaces button and related logic
        // as Showbridge handles discovery internally and doesn't expose network interfaces directly.
        // If specific interface selection is needed, it would be a more complex feature.

        // Network Interface Selection
        if (!g_local_interfaces.empty())
        {
            const char* current_interface_ip = g_selected_local_interface_ip.c_str();
            if (ImGui::BeginCombo("Network Interface", current_interface_ip))
            {
                for (const auto& ip : g_local_interfaces)
                {
                    bool is_selected = (current_interface_ip == ip);
                    if (ImGui::Selectable(ip.c_str(), is_selected))
                    {
                        g_selected_local_interface_ip = ip;
                    }
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::Text("No network interfaces found.");
        }

        if (ImGui::Button("Discover DACs"))
        {
            printf("Attempting to start discovery on interface: %s\n", g_selected_local_interface_ip.c_str());
            showbridge.startDiscovery(g_selected_local_interface_ip);
        }
        ImGui::Separator();

        ImGui::Text("Discovered Devices:");
        if (!showbridge.discoveredDACs.empty())
        {
            ImGui::BeginChild("##discovered_ips", ImVec2(0, 100), ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            for (int i = 0; i < showbridge.discoveredDACs.size(); ++i)
            {
                ImGui::PushID(i);
                char device_label[64];
                snprintf(device_label, sizeof(device_label), "%s (Ch: %d)",
                         showbridge.discoveredDACs[i].ip_address.c_str(),
                         showbridge.discoveredDACs[i].dac_information.channel);
                if (ImGui::Selectable(device_label, showbridge.selectedDeviceIndex == i))
                {
                    showbridge.selectDAC(i);
                    device_selected = true;
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        } else {
            ImGui::Text("No devices found. Click 'Discover DACs' to scan.");
        }

        ImGui::Text("Device Selected: %s", (showbridge.selectedDeviceIndex != -1) ? "Yes" : "No");
        if (showbridge.selectedDeviceIndex != -1)
        {
            ImGui::Text("Selected IP: %s", showbridge.discoveredDACs[showbridge.selectedDeviceIndex].ip_address.c_str());
            ImGui::Separator();
            if (ImGui::Button("Send Sample ILDA Frame"))
            {
                frame_buffer sample_frame;
                GenerateSampleILDAFrame(sample_frame);
                if (showbridge.sendFrame(sample_frame))
                {
                    printf("Sample ILDA frame sent successfully!\n");
                }
                else
                {
                    fprintf(stderr, "Failed to send sample ILDA frame.\n");
                }
            }
        }
        ImGui::End(); // End SDK Control Window

        // ILDA Clip Deck Window
        ImGui::Begin("ILDA Clip Deck");
        if (ImGui::BeginTable("clip_grid", clipDeck.getCols()))
        {
            for (int row = 0; row < clipDeck.getRows(); row++)
            {
                ImGui::TableNextRow();
                for (int col = 0; col < clipDeck.getCols(); col++)
                {
                    ImGui::TableNextColumn();
                    Clip* clip = clipDeck.getClip(row, col);
                    if (clip) {
                        if (ImGui::Button(clip->getName().c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 50)))
                        {
                            frame_buffer frame = clip->getFrame();
                            showbridge.sendFrame(frame);
                        }
                    }
                    else {
                        char button_label[32];
                        ImFormatString(button_label, IM_ARRAYSIZE(button_label), "Clip %d,%d", row, col);
                        ImGui::Button(button_label, ImVec2(ImGui::GetContentRegionAvail().x, 50));
                    }
                }
            }
            ImGui::EndTable();
        }
        ImGui::End();

        // Layers and Columns Window
        ImGui::Begin("Layers and Columns");
        ImGui::Text("Layers:");
        ImGui::Indent();
        ImGui::Text("Layer 1: Active");
        ImGui::Text("Layer 2: Inactive");
        ImGui::Text("Layer 3: Inactive");
        ImGui::Unindent();
        ImGui::Separator();
        ImGui::Text("Columns:");
        ImGui::Indent();
        ImGui::Text("Column A");
        ImGui::Text("Column B");
        ImGui::Text("Column C");
        ImGui::Unindent();
        ImGui::End();

        // Media Browser Window
        ImGui::Begin("Media Browser");
        ImGui::Text("Files:");
        ImGui::Separator();
        ImGui::Selectable("ilda_pattern_01.ilda");
        ImGui::Selectable("ilda_pattern_02.ilda");
        ImGui::Selectable("ilda_animation_loop.ilda");
        ImGui::Selectable("text_effect.ilda");
        ImGui::Selectable("audio_reactive_visual.ilda");
        ImGui::End();

        // Extensive Effects Library Window
        ImGui::Begin("Extensive Effects Library");
        ImGui::Text("Available Effects:");
        ImGui::Separator();
        ImGui::Selectable("Rotation");
        ImGui::Selectable("Scale");
        ImGui::Selectable("Transform");
        ImGui::Selectable("Color Shift");
        ImGui::End();

        // Generative Content (Sources) Window
        ImGui::Begin("Generative Content (Sources)");
        ImGui::Text("Available Sources:");
        ImGui::Separator();
        ImGui::Selectable("Simple Colors");
        ImGui::Selectable("Dots");
        ImGui::Selectable("Lines");
        ImGui::Selectable("Circles");
        ImGui::Selectable("Text Generator");
        ImGui::End();

        // Selected Clip Preview Window
        ImGui::Begin("Selected Clip Preview");
        ImGui::Text("Preview of the currently selected ILDA clip.");
        // Placeholder for actual preview rendering
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        if (canvas_size.x > 0 && canvas_size.y > 0)
        {
            ImGui::Image((void*)(intptr_t)1, canvas_size, ImVec2(0, 1), ImVec2(1, 0)); // Dummy image for OpenGL context
            ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, NULL);
            ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
                frame_buffer* fb = (frame_buffer*)cmd->UserCallbackData;
                RenderILDAFrame(*fb);
            }, &g_ilda_frame);
        }
        ImGui::End();

        // World/Global Preview Window
        ImGui::Begin("World/Global Preview");
        ImGui::Text("Preview of all active projectors.");
        // Placeholder for actual preview rendering
        ImVec2 canvas_size_global = ImGui::GetContentRegionAvail();
        if (canvas_size_global.x > 0 && canvas_size_global.y > 0)
        {
            ImGui::Image((void*)(intptr_t)2, canvas_size_global, ImVec2(0, 1), ImVec2(1, 0)); // Use a different texture ID
            ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, NULL);
            ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
                // For now, just render the same global frame.
                // Later, this will need to render frames for all active projectors.
                frame_buffer* fb = (frame_buffer*)cmd->UserCallbackData;
                RenderILDAFrame(*fb);
            }, &g_ilda_frame);
        }
        ImGui::End();

        // Projection Mapping Window
        ImGui::Begin("Projection Mapping");
        ImGui::Text("Projector Settings:");
        static char projector_ip[16] = "192.168.1.1";
        ImGui::InputText("Projector IP", projector_ip, IM_ARRAYSIZE(projector_ip));
        static float offset_x = 0.0f;
        static float offset_y = 0.0f;
        static float scale = 1.0f;
        static bool safe_zone_enabled = false;
        ImGui::SliderFloat("Offset X", &offset_x, -1.0f, 1.0f);
        ImGui::SliderFloat("Offset Y", &offset_y, -1.0f, 1.0f);
        ImGui::SliderFloat("Scale", &scale, 0.1f, 2.0f);
        ImGui::Checkbox("Enable Safe Zone", &safe_zone_enabled);
        ImGui::End();

        ImGui::End();

        // Rendering

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
    std::cout << "Exiting main loop." << std::endl;
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    showbridge.stopDiscovery(); // Stop the discovery thread
    showbridge.shutdown(); // Added shutdown call
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    getchar(); // Keep console open for debugging

    return 0;
}