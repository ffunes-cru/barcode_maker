#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <getopt.h>
#include <cstdlib>

#include <GLFW/glfw3.h>

#include "engine/barcode_core.hpp"
#include "gui/win11_theme.hpp"
#include "gui/app.hpp"
#include "updater/updater.hpp"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_glfw.h"
#include "../third_party/imgui/backends/imgui_impl_opengl3.h"

namespace fs = std::filesystem;

// --- CLI Runner ---
static int run_cli(int argc, char* argv[]) {
    BarcodeParams params;
    std::string input_file = "";
    std::string output_dir = "";
    int array_len = 0;

    static struct option long_options[] = {
        {"input",       required_argument, 0, 's'},
        {"input-file",  required_argument, 0, 'c'},
        {"output-dir",  required_argument, 0, 'o'},
        {"arr-len",     required_argument, 0, 'A'},
        {"height",      required_argument, 0, 'H'},
        {"height-txt",  required_argument, 0, 'T'},
        {"padd-x",      required_argument, 0, 'X'},
        {"padd-y",      required_argument, 0, 'Y'},
        {"padd-txt-y",  required_argument, 0, 'y'},
        {"res-fact",    required_argument, 0, 'R'},
        {"comp-fact",   required_argument, 0, 'C'},
        {"version",     no_argument,       0, 'v'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt_index = 0;
    int c;
    bool has_input_s = false;

    while ((c = getopt_long(argc, argv, "c:o:s:A:H:T:X:Y:y:R:C:vh", long_options, &opt_index)) != -1) {
        switch (c) {
            case 's':
                params.input = optarg ? optarg : "";
                has_input_s = true;
                break;
            case 'c':
                input_file = optarg ? optarg : "";
                break;
            case 'o':
                output_dir = optarg ? optarg : "";
                break;
            case 'A':
                array_len = std::atoi(optarg);
                break;
            case 'H':
                params.height = std::atoi(optarg);
                break;
            case 'T':
                params.height_txt = std::atoi(optarg);
                break;
            case 'X':
                params.padd_x = std::atoi(optarg);
                break;
            case 'Y':
                params.padd_y = std::atoi(optarg);
                break;
            case 'y':
                params.padd_txt_y = std::atoi(optarg);
                break;
            case 'R':
                params.res_fact = std::atoi(optarg);
                break;
            case 'C':
                params.comp_fact = std::atoi(optarg);
                break;
            case 'v':
                std::cout << "Code128 Studio v" << CODE128_APP_VERSION << "\n";
                return 0;
            case 'h':
                std::cout << "Usage: " << argv[0] << " [OPTIONS]\n";
                std::cout << "  -s, --input <string>       Generate single barcode\n";
                std::cout << "  -c, --input-file <path>    Generate from batch file\n";
                std::cout << "  -o, --output-dir <path>    Output directory\n";
                std::cout << "  -A, --arr-len <val>        Max batch count\n";
                std::cout << "  --gui, -g                  Launch graphical user interface\n";
                return 0;
            default:
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

    GLFWwindow* window = glfwCreateWindow(1360, 840, "Code128 Studio - Windows 11 Fluent Suite", nullptr, nullptr);
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

    Win11Theme::ApplyFluentDarkTheme();

    // Fonts
    std::vector<std::string> font_candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "font.otf"
    };
    bool font_loaded = false;
    for (const auto& path : font_candidates) {
        if (fs::exists(path)) {
            io.Fonts->AddFontFromFileTTF(path.c_str(), 16.0f);
            font_loaded = true;
            break;
        }
    }
    if (!font_loaded) io.Fonts->AddFontDefault();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    std::string exe_dir = "";
    if (argc > 0 && argv[0]) {
        exe_dir = fs::path(argv[0]).parent_path().string();
    }

    App app;
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
        glClearColor(0.125f, 0.125f, 0.125f, 1.0f);
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
    // If command-line flags (like -s, -c, -h, -v) are passed, run in CLI mode
    if (argc > 1) {
        if (std::strcmp(argv[1], "--gui") == 0 || std::strcmp(argv[1], "-g") == 0) {
            return run_gui(argc, argv);
        }
        return run_cli(argc, argv);
    }

    // Default without arguments: Launch GUI!
    return run_gui(argc, argv);
}
