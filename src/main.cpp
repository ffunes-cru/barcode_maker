#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

#include <GLFW/glfw3.h>

#include "engine/barcode_core.hpp"
#include "gui/win11_theme.hpp"
#include "gui/app.hpp"
#include "updater/updater.hpp"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_glfw.h"
#include "../third_party/imgui/backends/imgui_impl_opengl3.h"

namespace fs = std::filesystem;

// --- Portable Cross-Platform CLI Runner ---
static int run_cli(int argc, char* argv[]) {
    BarcodeParams params;
    std::string input_file = "";
    std::string output_dir = "";
    int array_len = 0;
    bool has_input_s = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-v" || arg == "--version") {
            std::cout << "Code128 Studio v" << CODE128_APP_VERSION << "\n";
            return 0;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n";
            std::cout << "  -s, --input <string>       Generate single barcode\n";
            std::cout << "  -c, --input-file <path>    Generate from batch file\n";
            std::cout << "  -o, --output-dir <path>    Output directory\n";
            std::cout << "  -A, --arr-len <val>        Max batch count\n";
            std::cout << "  -H, --height <val>         Bar height (px)\n";
            std::cout << "  -T, --height-txt <val>     Text size (px)\n";
            std::cout << "  -X, --padd-x <val>         Quiet zone margin (modules)\n";
            std::cout << "  -Y, --padd-y <val>         Margin Top Y (px)\n";
            std::cout << "  -B, --padd-bottom-y <val>  Margin Bottom / Separation (px)\n";
            std::cout << "  -y, --padd-txt-y <val>     Text gap Y (px)\n";
            std::cout << "  -R, --res-fact <val>       Module width (px)\n";
            std::cout << "  -C, --comp-fact <val>      Compression factor\n";
            std::cout << "  --gui, -g                  Launch graphical user interface\n";
            return 0;
        } else if ((arg == "-s" || arg == "--input") && i + 1 < argc) {
            params.input = argv[++i];
            has_input_s = true;
        } else if ((arg == "-c" || arg == "--input-file") && i + 1 < argc) {
            input_file = argv[++i];
        } else if ((arg == "-o" || arg == "--output-dir") && i + 1 < argc) {
            output_dir = argv[++i];
        } else if ((arg == "-A" || arg == "--arr-len") && i + 1 < argc) {
            array_len = std::atoi(argv[++i]);
        } else if ((arg == "-H" || arg == "--height") && i + 1 < argc) {
            params.bar_height = (float)std::atof(argv[++i]);
        } else if ((arg == "-T" || arg == "--height-txt") && i + 1 < argc) {
            params.text_size = (float)std::atof(argv[++i]);
        } else if ((arg == "-X" || arg == "--padd-x") && i + 1 < argc) {
            params.quiet_zone_x = (float)std::atof(argv[++i]);
        } else if ((arg == "-Y" || arg == "--padd-y") && i + 1 < argc) {
            params.margin_y = (float)std::atof(argv[++i]);
        } else if ((arg == "-B" || arg == "--padd-bottom-y") && i + 1 < argc) {
            params.margin_bottom = (float)std::atof(argv[++i]);
        } else if ((arg == "-y" || arg == "--padd-txt-y") && i + 1 < argc) {
            params.text_gap_y = (float)std::atof(argv[++i]);
        } else if ((arg == "-R" || arg == "--res-fact") && i + 1 < argc) {
            params.module_width = (float)std::atof(argv[++i]);
        } else if ((arg == "-C" || arg == "--comp-fact") && i + 1 < argc) {
            float comp = (float)std::atof(argv[++i]);
            if (comp > 0.01f) params.module_width /= comp;
        } else if (arg == "--gui" || arg == "-g") {
            // Handled in main()
        } else {
            std::cerr << "Unknown or incomplete option: " << arg << "\n";
            return 1;
        }
    }

    if (!has_input_s && input_file.empty()) {
        std::cerr << "Error: Please specify the input option (--input-file -c or -s --input) or launch with --gui\n";
        return 1;
    }

    std::string exe_dir = "";
    if (argc > 0 && argv[0]) {
        exe_dir = fs::path(argv[0]).parent_path().string();
    }

    BarcodeEngine engine;
    if (!engine.init(exe_dir)) {
        std::cerr << "Error: Failed to initialize BarcodeEngine.\n";
        return 1;
    }

    if (!output_dir.empty()) {
        fs::create_directories(output_dir);
    }

    if (has_input_s) {
        fs::path out_path = output_dir.empty() ? fs::path(params.input + ".png") : (fs::path(output_dir) / (params.input + ".png"));
        if (engine.save_png(params.input, params, out_path.string())) {
            std::cout << "Generated: " << out_path.string() << "\n";
            return 0;
        } else {
            std::cerr << "Error generating barcode for: " << params.input << "\n";
            return 1;
        }
    } else {
        std::ifstream file(input_file);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << input_file << "\n";
            return 1;
        }

        std::string line;
        int count = 0;
        while (std::getline(file, line) && (array_len <= 0 || count < array_len)) {
            size_t first = line.find_first_not_of(" \t\r\n");
            size_t last = line.find_last_not_of(" \t\r\n");
            if (first == std::string::npos || last == std::string::npos) continue;
            std::string code_str = line.substr(first, last - first + 1);
            if (code_str.empty()) continue;

            fs::path out_path = output_dir.empty() ? fs::path(code_str + ".png") : (fs::path(output_dir) / (code_str + ".png"));
            if (engine.save_png(code_str, params, out_path.string())) {
                std::cout << out_path.string() << "\n";
            }
            count++;
        }
        return 0;
    }
}

// --- GUI Runner ---
static int run_gui(int argc, char* argv[]) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1360, 840, "Code128 Studio", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Apply Balanced Windows 11 Fluent Theme
    Win11Theme::ApplyBalancedFluentTheme();

    // High Quality Crisp TTF/OTF Font Loading
    std::string exe_dir = "";
    if (argc > 0 && argv[0]) {
        exe_dir = fs::path(argv[0]).parent_path().string();
    }

    std::vector<std::string> search_paths = {
        "font.otf",
        "font.ttf",
        exe_dir + "/font.otf",
        exe_dir + "/../font.otf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf"
    };

    std::string loaded_font_path = "";
    for (const auto& p : search_paths) {
        if (!p.empty() && fs::exists(p)) {
            loaded_font_path = p;
            break;
        }
    }

    ImFontConfig font_cfg;
    font_cfg.OversampleH = 3;
    font_cfg.OversampleV = 2;
    font_cfg.PixelSnapH = true;

    ImFont* font_ui = nullptr;
    ImFont* font_barcode = nullptr;

    if (!loaded_font_path.empty()) {
        font_ui = io.Fonts->AddFontFromFileTTF(loaded_font_path.c_str(), 16.0f, &font_cfg);
        font_barcode = io.Fonts->AddFontFromFileTTF(loaded_font_path.c_str(), 96.0f, &font_cfg);
    }

    if (!font_ui) {
        font_ui = io.Fonts->AddFontDefault();
        font_barcode = font_ui;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    App app;
    app.set_fonts(font_ui, font_barcode);
    app.init(exe_dir);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.render_ui();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// --- Main Unified Entrypoint ---
int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (std::strcmp(argv[1], "--gui") == 0 || std::strcmp(argv[1], "-g") == 0) {
            return run_gui(argc, argv);
        }
        return run_cli(argc, argv);
    }

    return run_gui(argc, argv);
}
