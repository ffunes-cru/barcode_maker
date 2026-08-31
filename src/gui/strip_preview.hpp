#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "../engine/barcode_core.hpp"

enum class BrotherRollPreset {
    DK_22205_62mm,  // 62mm continuous tape (printable ~58mm / ~685 px @ 300 DPI)
    DK_22243_102mm, // 102mm continuous tape (printable ~98mm / ~1157 px @ 300 DPI)
    DK_22210_29mm,  // 29mm continuous tape (printable ~26mm / ~307 px @ 300 DPI)
    DK_22225_38mm,  // 38mm continuous tape (printable ~34mm / ~401 px @ 300 DPI)
    DK_11201_29x90, // 29x90mm die-cut label
    CustomRoll      // Custom mm width
};

struct StripSettings {
    BrotherRollPreset preset = BrotherRollPreset::DK_22205_62mm;
    float roll_width_mm = 62.0f;
    float printable_width_mm = 58.0f;
    int dpi = 300;

    int repeat_count = 12;            // Number of copies when in single-barcode mode
    bool use_batch_list = false;       // If true, build strip from batch file items instead of repeating single
    float label_gap_mm = 3.0f;         // Spacing between labels
    float leading_margin_mm = 5.0f;    // Tape lead margin
    float trailing_margin_mm = 5.0f;   // Tape tail margin
    bool show_cut_lines = true;        // Draw dashed or solid cut guidelines
    bool rotate_90 = false;            // 90 deg rotation (horizontal vs vertical on tape)
    bool center_on_tape = true;        // Center barcode vertically on tape width
};

struct StripImage {
    int width = 0;
    int height = 0;
    float total_length_mm = 0.0f;
    float tape_width_mm = 0.0f;
    int total_labels = 0;
    std::vector<uint8_t> rgba;
    bool valid = false;
    std::string error_message;
};

class StripGenerator {
public:
    StripGenerator();
    ~StripGenerator();

    static void GetPresetDimensions(BrotherRollPreset preset, float& out_tape_w, float& out_print_w, std::string& out_name);

    // Generates the continuous strip image buffer
    static StripImage GenerateStrip(
        BarcodeEngine& engine,
        const BarcodeParams& base_params,
        const std::vector<std::string>& label_texts,
        const StripSettings& settings
    );
};
