#include "barcode_core.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H

extern "C" {
#include "../../lib/img/libattopng.h"
}

namespace fs = std::filesystem;

BarcodeEngine::BarcodeEngine() = default;

BarcodeEngine::~BarcodeEngine() {
    if (ft_face_) {
        FT_Done_Face((FT_Face)ft_face_);
        ft_face_ = nullptr;
    }
    if (ft_library_) {
        FT_Done_FreeType((FT_Library)ft_library_);
        ft_library_ = nullptr;
    }
}

bool BarcodeEngine::init(const std::string& resource_dir) {
    resource_dir_ = resource_dir;
    if (!load_dictionaries()) {
        std::cerr << "Advertencia: No se pudieron cargar los diccionarios desde '" << resource_dir_ << "'\n";
    }
    if (!init_freetype()) {
        std::cerr << "Advertencia: No se pudo inicializar FreeType / fuentes.\n";
    }
    initialized_ = true;
    return true;
}

bool BarcodeEngine::load_dictionaries() {
    dict_char_.clear();
    dict_int_.clear();

    auto parse_file = [&](const std::string& path) {
        if (!fs::exists(path)) return false;
        std::ifstream file(path);
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t c1 = line.find(',');
            if (c1 != std::string::npos) {
                size_t c2 = line.find(',', c1 + 1);
                if (c2 != std::string::npos) {
                    try {
                        int val = std::stoi(line.substr(0, c1));
                        char c = line[c1 + 1];
                        std::string pat = line.substr(c2 + 1);
                        while (!pat.empty() && (pat.back() == '\r' || pat.back() == '\n' || pat.back() == ' ')) pat.pop_back();
                        Code128Entry entry{val, c, pat};
                        dict_char_[c] = entry;
                        dict_int_[val] = entry;
                    } catch (...) {}
                }
            } else {
                std::istringstream ss(line);
                int val; char c; std::string pat;
                if (ss >> val >> c >> pat) {
                    Code128Entry entry{val, c, pat};
                    dict_char_[c] = entry;
                    dict_int_[val] = entry;
                }
            }
        }
        return true;
    };

    std::vector<std::string> prefixes = {"", "../", "../../", resource_dir_ + "/", resource_dir_ + "/../"};
    for (const auto& pfx : prefixes) {
        parse_file(pfx + "code128char.txt");
        parse_file(pfx + "code128int.txt");
        parse_file(pfx + "dicc/code128_char.dic");
        parse_file(pfx + "dicc/code128_int.dic");
    }

    if (dict_int_.find(104) != dict_int_.end()) {
        dict_char_['#'] = dict_int_[104];
    } else {
        dict_char_['#'] = Code128Entry{104, '#', "11010010000"};
        dict_int_[104] = dict_char_['#'];
    }

    if (dict_int_.find(106) != dict_int_.end()) {
        dict_char_['$'] = dict_int_[106];
    } else {
        dict_char_['$'] = Code128Entry{106, '$', "11000111010"};
        dict_int_[106] = dict_char_['$'];
    }

    return !dict_char_.empty() && !dict_int_.empty();
}

bool BarcodeEngine::init_freetype() {
    if (FT_Init_FreeType((FT_Library*)&ft_library_)) {
        std::cerr << "Error al inicializar FreeType library\n";
        return false;
    }

    std::vector<std::string> font_candidates = {
        resource_dir_ + "/font.otf",
        resource_dir_ + "/../font.otf",
        "font.otf",
        "../font.otf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf"
    };

    for (const auto& font_path : font_candidates) {
        if (fs::exists(font_path)) {
            if (FT_New_Face((FT_Library)ft_library_, font_path.c_str(), 0, (FT_Face*)&ft_face_) == 0) {
                return true;
            }
        }
    }

    std::cerr << "Aviso: No se encontró ningún archivo de fuente TTF/OTF.\n";
    return false;
}

bool BarcodeEngine::validate_text(const std::string& text, std::string& out_error) const {
    if (text.empty()) {
        out_error = "El texto no puede estar vacío";
        return false;
    }

    if (dict_char_.empty()) {
        out_error = "Diccionario Code128 no cargado";
        return false;
    }

    for (size_t i = 0; i < text.length(); i++) {
        char c = text[i];
        char lookup = (c == ' ') ? 'b' : c;
        if (dict_char_.find(lookup) == dict_char_.end()) {
            out_error = std::string("Carácter no soportado en Code 128: '") + c + "'";
            return false;
        }
    }
    return true;
}

std::string BarcodeEngine::encode_to_bits(const std::string& text) const {
    if (text.empty() || dict_char_.empty()) return "";
    if (dict_char_.find('#') == dict_char_.end() || dict_char_.find('$') == dict_char_.end()) return "";

    const auto& start_entry = dict_char_.at('#');
    const auto& stop_entry = dict_char_.at('$');

    std::string code = start_entry.pattern;
    int check_sum = start_entry.value;

    int idx = 1;
    for (char c : text) {
        char lookup = (c == ' ') ? 'b' : c;
        if (dict_char_.find(lookup) == dict_char_.end()) return "";
        const auto& entry = dict_char_.at(lookup);
        check_sum += entry.value * idx;
        code += entry.pattern;
        idx++;
    }

    int check_val = check_sum % 103;
    if (dict_int_.find(check_val) == dict_int_.end()) return "";

    code += dict_int_.at(check_val).pattern;
    code += stop_entry.pattern;
    return code;
}

BarcodeImage BarcodeEngine::generate(const BarcodeParams& params) {
    BarcodeImage result;
    result.valid = false;

    if (!initialized_) {
        result.error_message = "Motor no inicializado";
        return result;
    }

    std::string val_err;
    if (!validate_text(params.input, val_err)) {
        result.error_message = val_err;
        return result;
    }

    // 1. Build Code128 binary pattern
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

    // 2. Continuous Fractional Geometry Calculations
    float mod_w = params.module_width > 0.1f ? params.module_width : 1.0f;
    int code_len = (int)code.length();

    // Total modules across = left quiet zone + code modules + 2 stop modules + right quiet zone
    float total_modules = (float)code_len + 2.0f + (params.quiet_zone_x * 2.0f);
    int barcode_raw_w = (int)std::ceil(total_modules * mod_w);
    int image_width = barcode_raw_w;

    // Pre-measure FreeType text metrics if text is active
    int text_width = 0;
    int max_bearing_y = 0;
    int max_descender_y = 0;

    if (ft_face_ && params.text_size > 0.0f) {
        FT_Face face = (FT_Face)ft_face_;
        int font_size = (int)std::round(params.text_size);
        if (font_size < 6) font_size = 6;
        FT_Set_Pixel_Sizes(face, 0, font_size);

        for (char c : params.input) {
            if (FT_Load_Char(face, c, FT_LOAD_DEFAULT) == 0) {
                text_width += face->glyph->advance.x >> 6;
                int top = face->glyph->bitmap_top;
                int rows = face->glyph->bitmap.rows;
                if (top > max_bearing_y) max_bearing_y = top;
                int desc = rows - top;
                if (desc > max_descender_y) max_descender_y = desc;
            }
        }
        // If text is wider than the barcode, expand image_width so text has margin and is never clipped
        float qz_px = params.quiet_zone_x * mod_w;
        int min_needed_w = text_width + (int)std::round(qz_px * 2.0f);
        if (min_needed_w > image_width) {
            image_width = min_needed_w;
        }
    }

    int text_actual_h = (max_bearing_y + max_descender_y > 0) ? (max_bearing_y + max_descender_y) : (int)std::round(params.text_size);
    float extra_cut_space = params.show_cut_line ? 4.0f : 0.0f;

    // Exact Symmetrical Height:
    // Top margin (params.margin_y) + Bar height (params.bar_height) + Text gap (params.text_gap_y) + Text height + Bottom margin (params.margin_y)
    int image_height = (int)std::ceil(params.margin_y + params.bar_height + params.text_gap_y + (float)text_actual_h + params.margin_y + extra_cut_space);

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

    // 3. Draw barcode bars with exact fractional coordinates (centered in image_width)
    float raw_bar_span = total_modules * mod_w;
    float start_x = ((float)image_width - raw_bar_span) * 0.5f + (params.quiet_zone_x * mod_w);
    float start_y = params.margin_y;
    float end_y = params.margin_y + params.bar_height;

    for (int i = 0; i < code_len; i++) {
        if (code[i] == '1') {
            int x0 = (int)std::floor(start_x + (float)i * mod_w);
            int x1 = (int)std::ceil(start_x + (float)(i + 1) * mod_w);
            for (int x = x0; x < x1 && x < image_width; x++) {
                for (int y = (int)std::floor(start_y); y < (int)std::ceil(end_y) && y < image_height; y++) {
                    set_pixel(x, y, 0);
                }
            }
        }
    }

    // 4. Draw additional stop bar (2 modules wide)
    int stop_x0 = (int)std::floor(start_x + (float)code_len * mod_w);
    int stop_x1 = (int)std::ceil(start_x + (float)(code_len + 2) * mod_w);
    for (int x = stop_x0; x < stop_x1 && x < image_width; x++) {
        for (int y = (int)std::floor(start_y); y < (int)std::ceil(end_y) && y < image_height; y++) {
            set_pixel(x, y, 0);
        }
    }

    // 5. FreeType Text Rasterization (Pixel-Perfect Alignment)
    if (ft_face_ && params.text_size > 0.0f) {
        FT_Face face = (FT_Face)ft_face_;
        int font_size = (int)std::round(params.text_size);
        if (font_size < 6) font_size = 6;
        FT_Set_Pixel_Sizes(face, 0, font_size);

        int pen_x = (image_width - text_width) / 2;
        // The top of the highest glyph is placed EXACTLY at end_y + params.text_gap_y
        int baseline_y = (int)std::round(end_y + params.text_gap_y) + max_bearing_y;

        for (char c : params.input) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER) == 0) {
                FT_Bitmap* bmp = &face->glyph->bitmap;
                int left = face->glyph->bitmap_left;
                int top = face->glyph->bitmap_top;

                for (int row = 0; row < (int)bmp->rows; row++) {
                    for (int col = 0; col < (int)bmp->width; col++) {
                        uint8_t alpha = bmp->buffer[row * bmp->pitch + col];
                        if (alpha > 30) {
                            int px = pen_x + left + col;
                            int py = baseline_y - top + row;
                            if (px >= 0 && px < image_width && py >= 0 && py < image_height) {
                                uint8_t existing = result.grayscale[py * image_width + px];
                                uint8_t blended = (uint8_t)(((255 - alpha) * existing) / 255);
                                set_pixel(px, py, blended);
                            }
                        }
                    }
                }
                pen_x += face->glyph->advance.x >> 6;
            }
        }
    }

    // 6. Draw optional cut separator line for individual label
    if (params.show_cut_line) {
        int cut_y = image_height - 2;
        for (int cx = 0; cx < image_width; cx++) {
            bool draw_dot = false;
            if (params.cut_line_style == 0) {
                // Dashed black line: 12px dash, 8px gap
                draw_dot = ((cx % 20) < 12);
            } else if (params.cut_line_style == 1) {
                // Continuous solid black line
                draw_dot = true;
            } else if (params.cut_line_style == 2) {
                // Edge tick marks (30px on each border)
                draw_dot = (cx < 30 || cx >= image_width - 30);
            }
            if (draw_dot) {
                set_pixel(cx, cut_y, 0);
                set_pixel(cx, cut_y - 1, 0);
            }
        }
    }

    result.valid = true;
    return result;
}

bool BarcodeEngine::save_png(const BarcodeImage& img, const std::string& filepath) {
    if (!img.valid || img.grayscale.empty() || img.width <= 0 || img.height <= 0) {
        return false;
    }

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
