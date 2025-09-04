// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#define _WIN32_WINNT 0x0601 // Windows 7
#include "imgui.h"
#include "../sdk/Easysocket.h"
#include "../sdk/DameiSDK.h"
#include <stdio.h> // Ensure stdio is included for printf
#pragma message("---" "main.cpp is being compiled" "---")

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <stdio.h>
#include <vector>
#include <string>
#include "Showbridge.h"
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

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

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void GenerateSampleILDAFrame(frame_buffer& fb)
{
    // Clear existing points
    fb.count = 0;

    // Define a simple triangle
    // Point 1 (Red)
    fb.points[fb.count].x = -0.5f;
    fb.points[fb.count].y = -0.5f;
    fb.points[fb.count].blanking = 1; // Light point
    fb.points[fb.count].r = 255;
    fb.points[fb.count].g = 0;
    fb.points[fb.count].b = 0;
    fb.count++;

    // Point 2 (Green)
    fb.points[fb.count].x = 0.0f;
    fb.points[fb.count].y = 0.5f;
    fb.points[fb.count].blanking = 1; // Light point
    fb.points[fb.count].r = 0;
    fb.points[fb.count].g = 255;
    fb.points[fb.count].b = 0;
    fb.count++;

    // Point 3 (Blue)
    fb.points[fb.count].x = 0.5f;
    fb.points[fb.count].y = -0.5f;
    fb.points[fb.count].blanking = 1; // Light point
    fb.points[fb.count].r = 0;
    fb.points[fb.count].g = 0;
    fb.points[fb.count].b = 255;
    fb.count++;

    // Close the triangle (blanked point to avoid drawing line back to start)
    fb.points[fb.count].x = -0.5f;
    fb.points[fb.count].y = -0.5f;
    fb.points[fb.count].blanking = 0; // Dark point
    fb.points[fb.count].r = 0;
    fb.points[fb.count].g = 0;
    fb.points[fb.count].b = 0;
    fb.count++;

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

// Define Clip struct
struct Clip {
    // Placeholder for clip properties
    std::string name;
    // ImTextureID thumbnail; // Will be added later
    // int trigger_style; // Will be added later
    // ...

    Clip() : name("New Clip") {}
};

// Define Layer struct
struct Layer {
    std::string name;
    int blend_mode;
    float intensity;
    std::vector<Clip> clips; // Clips for this layer

    Layer() : name("New Layer"), blend_mode(0), intensity(1.0f) {
        // Initialize with default clips if needed
        // For now, let's assume columns are added dynamically
    }
};

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
    printf("GLFW initialized successfully.\n");

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

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // Disable window decorations

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

    // Global UI state variables
    // static int num_layers = 5; // Replaced by layers.size()
    // static int num_columns = 13; // Replaced by layers[0].clips.size() (assuming all layers have same number of columns)
    std::vector<Layer> layers;
    std::vector<std::string> column_names; // Store column names for ImGui::TableSetupColumn

    // DameiSDK variables
    Showbridge showbridge;
    bool device_selected = false;
    show_list g_show_list; // Store the list of shows
    int g_selected_show_index = -1; // Index of the currently selected show
    show_info g_selected_show_info; // Information about the selected show
    bool g_show_extern_mode = false; // Current external mode of the selected show
    frame_buffer g_ilda_frame = {0}; // Global ILDA frame for rendering
    std::vector<std::string> g_local_interfaces; // Store local interface IPs for diagnostics
    std::string g_selected_local_interface_ip; // Store the selected local interface IP
    std::vector<DiscoveredDACInfo> discovered_dacs; // Declare discovered_dacs here

    // Initialize layers with 3 default layers and 13 default clips each
    for (int i = 0; i < 3; ++i) {
        layers.emplace_back(); // Add a default layer
        for (int j = 0; j < 13; ++j) {
            layers.back().clips.emplace_back(); // Add a default clip to the new layer
        }
    }

    // Initialize column names
    for (int j = 0; j < 13; ++j) { // Assuming 13 columns initially
        column_names.push_back("Col " + std::to_string(j + 1));
    }

    // Main loop
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
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("TrueLazer DockSpace", nullptr, window_flags);
		ImGui::PopStyleVar(3);

        // DockSpace
        ImGuiID dockspace_id = ImGui::GetID("TrueLazerDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

		static bool first_time = true;
		if (first_time)
		{
			first_time = false;
			ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add back the dockspace node
			ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

			// Split the dockspace into a top and bottom section
			ImGuiID dock_main_id = dockspace_id; // This variable will track the main docking space
			ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.4f, nullptr, &dock_main_id);
			ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.5f, nullptr, &dock_main_id);

			// Dock the windows
			ImGui::DockBuilderDockWindow("ClipDeck", dock_id_top);
			ImGui::DockBuilderDockWindow("SDK Control", dock_id_bottom);
			ImGui::DockBuilderDockWindow("Media Browser", dock_id_bottom);
			ImGui::DockBuilderDockWindow("Extensive Effects Library", dock_id_bottom);
			ImGui::DockBuilderDockWindow("Generative Content (Sources)", dock_id_bottom);
			ImGui::DockBuilderDockWindow("Selected Clip Preview", dock_id_bottom);
			ImGui::DockBuilderDockWindow("World/Global Preview", dock_id_bottom);
			ImGui::DockBuilderDockWindow("Projection Mapping", dock_id_bottom);


			ImGui::DockBuilderFinish(dockspace_id);
		}

        // Custom Title Bar
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("TrueLazer"))
            {
                ImGui::MenuItem("About");
                ImGui::MenuItem("Version");
                ImGui::MenuItem("Github");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings"))
            {
                ImGui::MenuItem("Placeholder");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Layer"))
            {
                if (ImGui::MenuItem("New")) { layers.emplace_back(); }
                ImGui::MenuItem("Insert Above");
                ImGui::MenuItem("Insert Below");
                ImGui::MenuItem("Rename");
                ImGui::MenuItem("Clear Clips");
                ImGui::MenuItem("Trigger Style");
                if (ImGui::MenuItem("Remove")) { if (layers.size() > 1) layers.pop_back(); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Column"))
            {
                if (ImGui::MenuItem("New")) { 
                    for (auto& layer : layers) { 
                        layer.clips.emplace_back(); 
                    }
                    column_names.push_back("Col " + std::to_string(column_names.size() + 1));
                }
                ImGui::MenuItem("Insert Before");
                ImGui::MenuItem("Insert After");
                ImGui::MenuItem("Duplicate");
                ImGui::MenuItem("Rename");
                ImGui::MenuItem("Clear Clips");
                if (ImGui::MenuItem("Remove")) { 
                    if (!layers.empty() && layers[0].clips.size() > 1) { 
                        for (auto& layer : layers) { 
                            layer.clips.pop_back(); 
                        }
                        column_names.pop_back();
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Clip"))
            {
                ImGui::MenuItem("Trigger Style");
                ImGui::MenuItem("Thumbnail");
                ImGui::MenuItem("Cut");
                ImGui::MenuItem("Copy");
                ImGui::MenuItem("Paste");
                ImGui::MenuItem("Rename");
                ImGui::MenuItem("Clear");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Output"))
            {
                ImGui::MenuItem("Open Output Window");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Shortcuts"))
            {
                ImGui::MenuItem("DMX/Artnet");
                ImGui::MenuItem("MIDI");
                ImGui::MenuItem("OSC");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Layouts");
                ImGui::MenuItem("Color Theme");
                ImGui::MenuItem("Render Mode");
                ImGui::EndMenu();
            }
			// Custom Window Controls (Minimize, Maximize, Close)
            // Position them at the top-right of the main window
            float button_width = 40.0f;
            float button_height = ImGui::GetFrameHeight();
            float padding = ImGui::GetStyle().FramePadding.x;
            float buttons_total_width = (button_width * 3) + (padding * 2); // 3 buttons + 2 paddings

            // Position for custom window controls
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (buttons_total_width + (padding*2)));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY()); // Keep current Y position

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.5f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
			
			// Minimize Button
            if (ImGui::Button("-", ImVec2(button_width, button_height))) {
                glfwIconifyWindow(window);
            }

            // Maximize/Restore Button
            ImGui::SameLine();
            if (ImGui::Button("[]", ImVec2(button_width, button_height))) {
                if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) {
                    glfwRestoreWindow(window);
                } else {
                    glfwMaximizeWindow(window);
                }
            }

            // Close Button
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			
            if (ImGui::Button("X", ImVec2(button_width, button_height))) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            ImGui::PopStyleColor(3); // Pop Close button colors

            ImGui::PopStyleColor(3); // Pop general button colors
            ImGui::PopStyleVar(2); // Pop FrameRounding and FramePadding
			
            ImGui::EndMainMenuBar();
        }

        // Combined Layer Controls and Clip Deck Window
        ImGui::Begin("ClipDeck");
        // Main Content
        if (ImGui::BeginTable("ClipDeckTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_ScrollX))
        {
            ImGui::TableSetupColumn("Layer Controls", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableSetupColumn("Clips", ImGuiTableColumnFlags_WidthStretch);

            // Header Row for Clips
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            ImGui::TableNextColumn();			// Skip layer controls column for header
			// Header Row
			ImGui::BeginChild("Header", ImVec2(0, ImGui::GetFrameHeightWithSpacing() * 1.2), true);
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 0);
			ImGui::Text("Comp");
			ImGui::SameLine();
			ImGui::Button("X");
			ImGui::SameLine();
			ImGui::Button("B");
			ImGui::SameLine();
			static float master_intensity = 1.0f;
			ImGui::SliderFloat("##MasterIntensity", &master_intensity, 0.0f, 1.0f);
			ImGui::NextColumn();
			static bool laser_on = false;
			ImGui::Checkbox("Laser On/Off", &laser_on);
			ImGui::Columns(1);
			ImGui::EndChild();
			
            ImGui::TableNextColumn();
            for (int j = 0; j < column_names.size(); ++j)
            {
                ImGui::Button(column_names[j].c_str());
                ImGui::SameLine();
            }

            for (int i = 0; i < layers.size(); ++i)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // -- Layer Controls Cell --
                ImGui::PushID(i);
                ImGui::Text("Layer %d", i + 1);

				ImGui::Columns(2, "layer_cols", false);
				// Column 1: X, B, S buttons
				ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.2f);
				if (ImGui::Button("X", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 2.5f))) { /* Clear Clips */ }
				if (ImGui::Button("B", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 1.25f))) { /* Blackout */ }
				if (ImGui::Button("S", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 1.25f))) { /* Solo */ }
				ImGui::NextColumn();
				// Column 2: Blend-mode, Intensity Slider, Preview Thumbnail
				const char* blend_modes[] = { "Normal", "Add", "Subtract" };
				ImGui::Combo("Blend", &layers[i].blend_mode, blend_modes, IM_ARRAYSIZE(blend_modes));
				ImGui::VSliderFloat("##intensity", ImVec2(ImGui::GetColumnWidth() * 0.5f, ImGui::GetTextLineHeightWithSpacing() * 5.0f), &layers[i].intensity, 0.0f, 1.0f, "Int");
				ImGui::SameLine();
				ImGui::Button("Preview", ImVec2(ImGui::GetColumnWidth() * 0.5f, ImGui::GetTextLineHeightWithSpacing() * 5.0f));
				ImGui::Columns(1);
				char layer_name_buf[64];
				strncpy(layer_name_buf, layers[i].name.c_str(), sizeof(layer_name_buf) - 1);
				layer_name_buf[sizeof(layer_name_buf) - 1] = '\0';
				ImGui::InputText("##layer_name", layer_name_buf, IM_ARRAYSIZE(layer_name_buf));
				layers[i].name = layer_name_buf;

                ImGui::PopID();

                ImGui::TableNextColumn();

                // -- Clips Cell --
                for (int j = 0; j < layers[i].clips.size(); ++j)
                {
                    ImGui::Button(layers[i].clips[j].name.c_str(), ImVec2(100, 80));
                    ImGui::SameLine();
                }
            }

            ImGui::EndTable();
        }


        ImGui::End(); // End Combined Window

        // SDK Control Window
        ImGui::Begin("SDK Control");

        // --- Network Diagnostics ---
        if (ImGui::Button("Refresh Network Interfaces"))
        {
            g_local_interfaces.clear();
            SocketLib::UDPSocket temp_udp_socket(0);
            temp_udp_socket.get_interfaces(g_local_interfaces);
        }
        ImGui::Text("Detected Network Interfaces:");
        ImGui::BeginChild("##local_interfaces", ImVec2(0, 100), ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        if (g_local_interfaces.empty())
        {
            ImGui::Text("(No interfaces found. Click Refresh)");
        }
        else
        {
            for (const auto& ip : g_local_interfaces)
            {
                if (ImGui::RadioButton(ip.c_str(), ip == g_selected_local_interface_ip))
                {
                    g_selected_local_interface_ip = ip;
                    showbridge.startScanning(g_selected_local_interface_ip);
                }
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        // --- End Network Diagnostics ---

        ImGui::Text("Scanning for devices: %s", showbridge.isScanning() ? "Yes" : "No");

        discovered_dacs = showbridge.getDiscoveredDACs();
        if (!discovered_dacs.empty())
        {
            ImGui::Text("Discovered Devices:");
            ImGui::BeginChild("##discovered_ips", ImVec2(0, 100), ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            for (int i = 0; i < discovered_dacs.size(); ++i)
            {
                char dac_label[128];
                sprintf(dac_label, "%s (Channel: %d)", discovered_dacs[i].ip_address.c_str(), discovered_dacs[i].dac_information.channel);
                if (ImGui::Selectable(dac_label, showbridge.getSelectedDeviceIndex() == i))
                {
                    if (showbridge.selectDevice(i)) {
                        device_selected = true;
                        // Now that a device is selected, we can get the show list.
                        // The SDK will be initialized on demand by the getSdk() method if not already.
                        if (showbridge.getSdk() && showbridge.getSdk()->GetShowList(g_show_list)) {
                             g_selected_show_index = -1;
                        } else {
                            ImGui::OpenPopup("SDK Error");
                        }
                    }
                }
            }
            ImGui::EndChild();
        } else {
            ImGui::Text("No devices found.");
        }

        // Modal for SDK errors
        if (ImGui::BeginPopupModal("SDK Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Failed to communicate with device.\nPlease check network connection.");
            if (ImGui::Button("OK")) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        ImGui::Text("Device Selected: %s", device_selected ? "Yes" : "No");
        if (device_selected)
        {
            ImGui::Text("Selected IP: %s", showbridge.getSelectedDeviceIp().c_str());
        }

        if (device_selected)
        {
            ImGui::Separator();
            ImGui::Text("Available Shows: %d", g_show_list.count);

            const char* combo_preview_value = "Select a Show"; // Default value

            if (g_selected_show_index >= 0 && g_selected_show_index < g_show_list.count)
            {
                if (g_show_list.udpPort[g_selected_show_index] > 0)
                {
                    combo_preview_value = g_selected_show_info.showName;
                }
            }

            if (ImGui::BeginCombo("##shows", combo_preview_value))
            {
                for (int i = 0; i < g_show_list.count; i++)
                {
                    show_info temp_info;
                    if (showbridge.getSdk() && showbridge.getSdk()->GetShowInfo(i, temp_info)) 
                    {
                        bool is_selected = (g_selected_show_index == i);
                        if (ImGui::Selectable(temp_info.showName, is_selected))
                        {
                            g_selected_show_index = i;
                            g_selected_show_info = temp_info;
                            g_show_extern_mode = (g_selected_show_info.cannerInfo.status[1] == 3);
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (g_selected_show_index != -1)
            {
                ImGui::Text("Selected Show: %s", g_selected_show_info.showName);
                ImGui::Text("UDP Port: %d", g_selected_show_info.udpPort);
                ImGui::Text("Channel: %d", g_selected_show_info.cannerInfo.channel);
                ImGui::Text("External Mode: %s", g_show_extern_mode ? "Enabled" : "Disabled");

                if (ImGui::Button(g_show_extern_mode ? "Disable External Mode" : "Enable External Mode"))
                {
                    if (showbridge.getSdk() && showbridge.getSdk()->SetShowExternMode(g_selected_show_index, !g_show_extern_mode))
                    {
                        g_show_extern_mode = !g_show_extern_mode;
                    }
                }

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
        }
        ImGui::End(); // End SDK Control Window

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
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    showbridge.stopScanning();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    getchar(); // Keep console open for debugging

    return 0;
}
