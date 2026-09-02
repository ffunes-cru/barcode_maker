#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

struct BarcodeParams {
    std::string input = "A0100";

    // Continuous Fractional Geometry (Floating Point)
    float module_width = 6.0f;       // X-Dimension: Width of 1 narrow module in pixels (e.g. 1.0f to 16.0f)
    float bar_height = 90.0f;        // Height of barcode bars in pixels
    float text_size = 32.0f;         // Human readable text size in pixels
    float quiet_zone_x = 10.0f;      // Quiet zone margins on left and right in modules (standard >= 10)
    float margin_y = 8.0f;           // Top paper margin in pixels
    float margin_bottom = 8.0f;      // Bottom margin / separation between labels in pixels
    float text_gap_y = 8.0f;         // Spacing between bars and text in pixels
    bool show_cut_line = false;      // Optional cut line on individual label
    int cut_line_style = 0;          // 0 = Dashed, 1 = Solid, 2 = Side marks
};

struct BarcodeImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;      // 4 bytes per pixel (RGBA) for OpenGL & GUI
    std::vector<uint8_t> grayscale; // 1 byte per pixel for PNG encoding
    std::string encoded_bits;       // Binary pattern string ("1101...")
    bool valid = false;
    std::string error_message;
};

struct Code128Entry {
    int value = 0;
    char ascii = 0;
    std::string pattern;
};

class BarcodeEngine {
public:
    BarcodeEngine();
    ~BarcodeEngine();

    bool init(const std::string& resource_dir = "");

    bool is_initialized() const { return initialized_; }
    const std::string& get_resource_dir() const { return resource_dir_; }

    // Generates barcode image in memory (RGBA & Grayscale buffers) with fractional precision
    BarcodeImage generate(const BarcodeParams& params);

    // Save generated image to PNG file
    bool save_png(const BarcodeImage& img, const std::string& filepath);
    bool save_png(const std::string& text, const BarcodeParams& params, const std::string& filepath);

    // Encode string to Code128 binary bit pattern string ("1101...")
    std::string encode_to_bits(const std::string& text) const;

    // Validation helper
    bool validate_text(const std::string& text, std::string& out_error) const;

private:
    bool load_dictionaries();
    bool init_freetype();

    std::unordered_map<char, Code128Entry> dict_char_;
    std::unordered_map<int, Code128Entry> dict_int_;
    std::string resource_dir_;
    bool initialized_ = false;

    void* ft_library_ = nullptr;
    void* ft_face_ = nullptr;
};
