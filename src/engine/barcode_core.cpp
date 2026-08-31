#include "barcode_core.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

extern "C" {
#include "../../lib/img/libattopng.h"
}

namespace fs = std::filesystem;

BarcodeEngine::BarcodeEngine() = default;

BarcodeEngine::~BarcodeEngine() {
    cleanup_freetype();
}

std::string BarcodeEngine::find_file(const std::string& filename) {
    std::vector<std::string> search_dirs = {
        resource_dir_,
        ".",
        "..",
        "./bin",
        "../bin",
        "../../",
        "/usr/local/share/code128_maker",
        "/usr/share/code128_maker"
    };

    for (const auto& dir : search_dirs) {
        if (dir.empty()) continue;
        fs::path p = fs::path(dir) / filename;
        if (fs::exists(p)) {
            return p.string();
        }
    }
    return "";
}

bool BarcodeEngine::init(const std::string& resource_dir) {
    resource_dir_ = resource_dir;

    if (!load_dictionaries()) {
        std::cerr << "[BarcodeEngine] Error loading dictionaries\n";
        return false;
    }

    if (!init_freetype()) {
        std::cerr << "[BarcodeEngine] Error initializing FreeType\n";
        return false;
    }

    initialized_ = true;
    return true;
}

bool BarcodeEngine::load_dictionaries() {
    std::string char_file = find_file("code128char.txt");
    std::string int_file = find_file("code128int.txt");

    if (char_file.empty() || int_file.empty()) {
        std::cerr << "[BarcodeEngine] Could not locate code128char.txt or code128int.txt\n";
        return false;
    }

    dict_char_.clear();
    dict_int_.clear();

    // Parse code128char.txt
    {
        std::ifstream file(char_file);
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string val_str, ascii_str, pattern;
            if (std::getline(ss, val_str, ',') &&
                std::getline(ss, ascii_str, ',') &&
                std::getline(ss, pattern)) {
                
                Code128Entry entry;
                entry.value = std::stoi(val_str);
                entry.ascii = ascii_str.empty() ? ' ' : ascii_str[0];
                // Trim pattern
                pattern.erase(std::remove_if(pattern.begin(), pattern.end(), ::isspace), pattern.end());
                entry.pattern = pattern;

                dict_char_[entry.ascii] = entry;
            }
        }
    }

    // Parse code128int.txt
    {
        std::ifstream file(int_file);
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string val_str, ascii_str, pattern;
            if (std::getline(ss, val_str, ',') &&
                std::getline(ss, ascii_str, ',') &&
                std::getline(ss, pattern)) {
                
                Code128Entry entry;
                entry.value = std::stoi(val_str);
                entry.ascii = ascii_str.empty() ? ' ' : ascii_str[0];
                pattern.erase(std::remove_if(pattern.begin(), pattern.end(), ::isspace), pattern.end());
                entry.pattern = pattern;

                dict_int_[entry.value] = entry;
            }
        }
    }

    return (!dict_char_.empty() && !dict_int_.empty());
}

bool BarcodeEngine::init_freetype() {
    cleanup_freetype();

    FT_Library lib = nullptr;
    if (FT_Init_FreeType(&lib)) {
        return false;
    }
    ft_library_ = lib;

    font_path_ = find_file("font.otf");
    if (font_path_.empty()) {
        font_path_ = find_file("font.ttf");
    }
    if (font_path_.empty()) {
        std::cerr << "[BarcodeEngine] font.otf not found\n";
        return false;
    }

    FT_Face face = nullptr;
    if (FT_New_Face((FT_Library)ft_library_, font_path_.c_str(), 0, &face)) {
        std::cerr << "[BarcodeEngine] Failed to load font face from " << font_path_ << "\n";
        return false;
    }
    ft_face_ = face;

    return true;
}

void BarcodeEngine::cleanup_freetype() {
    if (ft_face_) {
        FT_Done_Face((FT_Face)ft_face_);
        ft_face_ = nullptr;
    }
    if (ft_library_) {
        FT_Done_FreeType((FT_Library)ft_library_);
        ft_library_ = nullptr;
    }
}

bool BarcodeEngine::validate_text(const std::string& text, std::string& out_error) const {
    if (text.empty()) {
        out_error = "El texto no puede estar vacío";
        return false;
    }
    if (dict_char_.find('#') == dict_char_.end() || dict_char_.find('$') == dict_char_.end()) {
        out_error = "Diccionario no inicializado (faltan START/STOP)";
        return false;
    }

    for (char c : text) {
        char lookup = (c == ' ') ? 'b' : c;
        if (dict_char_.find(lookup) == dict_char_.end()) {
            out_error = std::string("Carácter no soportado en Code128: '") + c + "'";
            return false;
        }
    }
    return true;
}

BarcodeImage BarcodeEngine::generate(const BarcodeParams& params) {
    BarcodeImage result;

    if (!initialized_) {
        result.error_message = "Motor no inicializado";
        return result;
    }

    std::string val_err;
    if (!validate_text(params.input, val_err)) {
        result.error_message = val_err;
        return result;
    }

    // 1. Build Code128 binary string
    const auto& start_entry = dict_char_.at('#');
    const auto& stop_entry = dict_char_.at('$');

    std::string code = start_entry.pattern;
    int check_sum = start_entry.value;

    int idx = 1;
    for (char c : params.input) {
        char lookup = (c == ' ') ? 'b' : c;
        const auto& entry = dict_char_.at(lookup);
        check_sum += entry.value * idx;
        code += entry.pattern;
        idx++;
    }

    int check_val = check_sum % 103;
    if (dict_int_.find(check_val) == dict_int_.end()) {
        result.error_message = "Error calculando dígito de control";
        return result;
    }
    code += dict_int_.at(check_val).pattern;
    code += stop_entry.pattern;

    result.encoded_bits = code;
    int code_len = (int)code.length();

    // 2. Geometry calculations
    int comp_fact = params.comp_fact <= 0 ? 1 : params.comp_fact;
    int res_fact = params.res_fact <= 0 ? 1 : params.res_fact;
    int code_res_fac = (int)std::floor((double)res_fact / comp_fact);
    if (code_res_fac < 1) code_res_fac = 1;

    int image_width = (code_len + 1 + (params.padd_x * 2)) * code_res_fac;
    int image_height = (params.height + (params.height_txt - (params.padd_txt_y * 2)) + (params.padd_y * 2)) * res_fact;

    if (image_width < 10 || image_height < 10) {
        result.error_message = "Dimensiones calculadas inválidas";
        return result;
    }

    result.width = image_width;
    result.height = image_height;

    // Allocate grayscale (1 byte) and RGBA (4 bytes) buffers with white background (255)
    result.grayscale.assign(image_width * image_height, 255);
    result.rgba.assign(image_width * image_height * 4, 255);

    auto set_pixel = [&](int x, int y, uint8_t val) {
        if (x < 0 || x >= image_width || y < 0 || y >= image_height) return;
        result.grayscale[y * image_width + x] = val;
        size_t rgba_idx = (y * image_width + x) * 4;
        result.rgba[rgba_idx + 0] = val;
        result.rgba[rgba_idx + 1] = val;
        result.rgba[rgba_idx + 2] = val;
        result.rgba[rgba_idx + 3] = 255;
    };

    // 3. Draw barcode bars
    int start_x = params.padd_x * code_res_fac;
    int end_x = (code_len + params.padd_x) * code_res_fac;
    int start_y = params.padd_y * res_fact;
    int end_y = params.height * res_fact;

    for (int x = start_x; x < end_x && x < image_width; x++) {
        int bit_idx = (int)std::floor((double)(x - start_x) / code_res_fac);
        if (bit_idx >= 0 && bit_idx < code_len) {
            uint8_t color = (code[bit_idx] == '1') ? 0 : 255;
            for (int y = start_y; y < end_y && y < image_height; y++) {
                set_pixel(x, y, color);
            }
        }
    }

    // 4. Draw additional stop bar
    for (int y = start_y; y < end_y && y < image_height; y++) {
        for (int x = 0; x < code_res_fac; x++) {
            set_pixel((params.padd_x + code_len) * code_res_fac + x, y, 0);
            set_pixel((params.padd_x + code_len) * code_res_fac + x + 1, y, 0);
        }
    }

    // 5. Draw top border line
    for (int x = 0; x < image_width; x++) {
        set_pixel(x, 0, 0);
    }

    // 6. FreeType Text Rasterization
    if (ft_face_) {
        FT_Face face = (FT_Face)ft_face_;
        int font_size = std::max(1, (params.height_txt - params.padd_txt_y) * res_fact);
        FT_Set_Pixel_Sizes(face, 0, font_size);

        // Measure text advance & max height
        int text_width = 0;
        int max_glyph_height = 0;
        for (char c : params.input) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER) == 0) {
                text_width += face->glyph->advance.x >> 6;
                if ((int)face->glyph->bitmap.rows > max_glyph_height) {
                    max_glyph_height = (int)face->glyph->bitmap.rows;
                }
            }
        }

        int center_pad = (image_width / 2) - (text_width / 2);
        int pen_x = 0;

        for (char c : params.input) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER) == 0) {
                FT_Bitmap* bmp = &face->glyph->bitmap;
                int left = face->glyph->bitmap_left;
                int top = face->glyph->bitmap_top;

                for (int gy = 0; gy < (int)bmp->rows; gy++) {
                    for (int gx = 0; gx < (int)bmp->width; gx++) {
                        int final_x = pen_x + left + gx + center_pad;
                        int final_y = (max_glyph_height - top + gy) + (params.height + params.padd_txt_y) * res_fact;

                        uint8_t alpha = bmp->buffer[gy * bmp->pitch + gx];
                        if (alpha > 0) {
                            uint8_t gray_val = (uint8_t)std::abs((int)alpha - 255);
                            set_pixel(final_x, final_y, gray_val);
                        }
                    }
                }
                pen_x += face->glyph->advance.x >> 6;
            }
        }
    }

    result.valid = true;
    return result;
}

bool BarcodeEngine::save_png(const BarcodeImage& img, const std::string& filepath) {
    if (!img.valid || img.grayscale.empty()) return false;

    libattopng_t* png = libattopng_new(img.width, img.height, PNG_GRAYSCALE);
    if (!png) return false;

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            uint8_t val = img.grayscale[y * img.width + x];
            libattopng_set_pixel(png, x, y, val);
        }
    }

    libattopng_save(png, filepath.c_str());
    libattopng_destroy(png);
    return true;
}

bool BarcodeEngine::save_png(const std::string& text, const BarcodeParams& params, const std::string& filepath) {
    BarcodeParams p = params;
    p.input = text;
    BarcodeImage img = generate(p);
    return save_png(img, filepath);
}
