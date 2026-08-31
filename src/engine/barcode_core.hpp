#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

struct BarcodeParams {
    std::string input = "A0101";
    int height = 20;
    int height_txt = 7;
    int padd_x = 5;
    int padd_y = 2;
    int padd_txt_y = 2;
    int res_fact = 8;
    int comp_fact = 1;
    float fractional_scale = 1.0f; // Subpixel fractional scaling (e.g. 0.25x to 3.00x)
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

    // Initializes dictionaries and FreeType font.
    // resource_dir can be specified; if empty, will search known candidate paths.
    bool init(const std::string& resource_dir = "");

    bool is_initialized() const { return initialized_; }
    const std::string& get_resource_dir() const { return resource_dir_; }

    // Generates barcode image in memory (returns RGBA & Grayscale buffers)
    BarcodeImage generate(const BarcodeParams& params);

    // Save generated image to PNG file
    bool save_png(const BarcodeImage& img, const std::string& filepath);
    bool save_png(const std::string& text, const BarcodeParams& params, const std::string& filepath);

    // Validation helper: checks if string can be encoded with current Code128 dictionary
    bool validate_text(const std::string& text, std::string& out_error) const;

private:
    bool load_dictionaries();
    bool init_freetype();
    void cleanup_freetype();
    std::string find_file(const std::string& filename);

    std::unordered_map<char, Code128Entry> dict_char_;
    std::unordered_map<int, Code128Entry> dict_int_;

    std::string resource_dir_;
    std::string font_path_;
    bool initialized_ = false;

    // FreeType opaque handles
    void* ft_library_ = nullptr;
    void* ft_face_ = nullptr;
};
