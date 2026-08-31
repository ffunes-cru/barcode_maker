#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "../engine/barcode_core.hpp"

enum class BrotherRollPreset {
    DK_22246_103mm, // Brother QL-1110NWB 4-inch Wide Tape (103.6mm / Printable ~99mm)
    DK_22205_62mm,  // Standard QL Continuous Tape (62mm / Printable ~58mm)
    DK_22210_29mm,  // Narrow QL Continuous Tape (29mm / Printable ~26mm)
    DK_22223_50mm,  // Medium Continuous Tape (50mm / Printable ~46mm)
    DK_22225_38mm,  // Medium-Narrow Continuous Tape (38mm / Printable ~34mm)
    DK_11241_102x152, // Large Shipping Die-Cut Label for QL-1110NWB (102x152mm)
    DK_11201_29x90,   // Standard Die-Cut Label (29x90mm)
    CustomRoll      // Custom mm width
};

struct StripSettings {
    BrotherRollPreset preset = BrotherRollPreset::DK_22205_62mm;
    float roll_width_mm = 62.0f;
    float printable_width_mm = 58.0f;
    int dpi = 300;

    bool vertical_feed = true;         // True: Strip feeds downwards (natural Brother roll feed)
    int repeat_count = 12;             // Number of copies
    bool use_batch_list = false;       // Use batch file items
    float label_gap_mm = 4.0f;         // Spacing between labels
    float leading_margin_mm = 4.0f;    // Lead margin
    float trailing_margin_mm = 4.0f;   // Tail margin
    bool show_cut_lines = true;        // Draw cut guidelines
    bool rotate_90 = false;            // Rotate individual barcode 90 deg
    bool center_on_tape = true;        // Center barcode horizontally on tape
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

    // Generates composite continuous strip bitmap (vertical downwards feed)
    static StripImage GenerateStrip(
        BarcodeEngine& engine,
        const BarcodeParams& base_params,
        const std::vector<std::string>& label_texts,
        const StripSettings& settings
    );
};
