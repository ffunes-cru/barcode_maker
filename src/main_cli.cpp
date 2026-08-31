#include "engine/barcode_core.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <getopt.h>
#include <cstdlib>

namespace fs = std::filesystem;

static void print_help_and_exit(const char* prog_name, int exit_code) {
    std::cout << "\n";
    std::cout << "Usage: " << prog_name << " [GENERAL OPTIONS] [CONFIGURATION OPTIONS]\n";
    std::cout << "Description: Creates Code128 images with configuration parameters (C++ Engine).\n\n";
    std::cout << "General Options:\n";
    std::cout << "  -s, --input <string>       Uses console input for creating a single Code128 image.\n";
    std::cout << "  -c, --input-file <path>    Route to input file, each string separated by newline.\n";
    std::cout << "  -o, --output-dir <path>    Code128 image/s output directory.\n\n";
    std::cout << "Configuration Options:\n";
    std::cout << "  -A, --array-len <val>      Define input array max length (Mandatory if using -c).\n";
    std::cout << "  -H, --height <val>         Define bar height (Default: 20).\n";
    std::cout << "  -T, --height-txt <val>     Define text height (Default: 7).\n";
    std::cout << "  -X, --padd-x <val>         Define x padding (Default: 5).\n";
    std::cout << "                             WARNING: Lower values may eliminate the quiet zone.\n";
    std::cout << "  -Y, --padd-y <val>         Define y padding (Default: 2).\n";
    std::cout << "  -y, --padd-txt-y <val>     Define y padding for text (Default: 2).\n";
    std::cout << "  -R, --res-fact <val>       Define integer scaling factor (Default: 4).\n";
    std::cout << "  -C, --comp-fact <val>      Define code128 compression factor (Default: 2).\n";
    std::cout << "  -h, --help                 Show this message and exit.\n\n";
    std::exit(exit_code);
}

int main(int argc, char* argv[]) {
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
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt_index = 0;
    int c;
    bool has_input_s = false;

    while ((c = getopt_long(argc, argv, "c:o:s:A:H:T:X:Y:y:R:C:h", long_options, &opt_index)) != -1) {
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
            case 'h':
                print_help_and_exit(argv[0], 0);
                break;
            default:
                print_help_and_exit(argv[0], 1);
                break;
        }
    }

    if (!has_input_s && input_file.empty()) {
        std::cerr << "Error: Please specify the input option (--input-file -c or -s --input).\n";
        print_help_and_exit(argv[0], 1);
    }

    if (has_input_s && !input_file.empty()) {
        std::cerr << "Error: Either console input or file input, not both.\n";
        print_help_and_exit(argv[0], 1);
    }

    if (!input_file.empty() && array_len <= 0) {
        std::cerr << "Error: Please provide the number of elements to generate (-A).\n";
        print_help_and_exit(argv[0], 1);
    }

    // Initialize Engine
    std::string exe_dir = "";
    if (argc > 0 && argv[0]) {
        exe_dir = fs::path(argv[0]).parent_path().string();
    }

    BarcodeEngine engine;
    if (!engine.init(exe_dir)) {
        std::cerr << "Error: Failed to initialize BarcodeEngine. Check resources.\n";
        return 1;
    }

    if (!output_dir.empty()) {
        fs::create_directories(output_dir);
    }

    if (has_input_s) {
        fs::path out_path = output_dir.empty() ? fs::path(params.input + ".png") : (fs::path(output_dir) / (params.input + ".png"));
        if (engine.save_png(params.input, params, out_path.string())) {
            std::cout << "Generated: " << out_path.string() << "\n";
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
        while (std::getline(file, line) && count < array_len) {
            size_t first = line.find_first_not_of(" \t\r\n");
            size_t last = line.find_last_not_of(" \t\r\n");
            if (first == std::string::npos || last == std::string::npos) continue;
            std::string code_str = line.substr(first, last - first + 1);
            if (code_str.empty()) continue;

            fs::path out_path = output_dir.empty() ? fs::path(code_str + ".png") : (fs::path(output_dir) / (code_str + ".png"));
            if (engine.save_png(code_str, params, out_path.string())) {
                std::cout << out_path.string() << "\n";
            } else {
                std::cerr << "Error generating: " << code_str << "\n";
            }
            count++;
        }
    }

    return 0;
}
