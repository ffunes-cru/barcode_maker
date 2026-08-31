#include "strip_preview.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

StripGenerator::StripGenerator() = default;
StripGenerator::~StripGenerator() = default;

void StripGenerator::GetPresetDimensions(BrotherRollPreset preset, float& out_tape_w, float& out_print_w, std::string& out_name) {
    switch (preset) {
        case BrotherRollPreset::DK_22205_62mm:
            out_tape_w = 62.0f;
            out_print_w = 58.0f;
            out_name = "Brother DK-22205 (62 mm Continuo)";
            break;
        case BrotherRollPreset::DK_22243_102mm:
            out_tape_w = 102.0f;
            out_print_w = 98.0f;
            out_name = "Brother DK-22243 (102 mm Continuo / Envíos)";
            break;
        case BrotherRollPreset::DK_22210_29mm:
            out_tape_w = 29.0f;
            out_print_w = 26.0f;
            out_name = "Brother DK-22210 (29 mm Continuo / Estrecho)";
            break;
        case BrotherRollPreset::DK_22225_38mm:
            out_tape_w = 38.0f;
            out_print_w = 34.0f;
            out_name = "Brother DK-22225 (38 mm Continuo)";
            break;
        case BrotherRollPreset::DK_11201_29x90:
            out_tape_w = 29.0f;
            out_print_w = 26.0f;
            out_name = "Brother DK-11201 (29 x 90 mm Precortada)";
            break;
        case BrotherRollPreset::CustomRoll:
        default:
            out_tape_w = 62.0f;
            out_print_w = 58.0f;
            out_name = "Personalizado (Medida Libre)";
            break;
    }
}

StripImage StripGenerator::GenerateStrip(
    BarcodeEngine& engine,
    const BarcodeParams& base_params,
    const std::vector<std::string>& label_texts,
    const StripSettings& settings
) {
    StripImage strip;
    if (label_texts.empty()) {
        strip.error_message = "No hay etiquetas para generar la tira";
        return strip;
    }

    float dots_per_mm = settings.dpi / 25.4f;

    float tape_w_mm = settings.roll_width_mm;
    float print_w_mm = settings.printable_width_mm;
    if (settings.preset != BrotherRollPreset::CustomRoll) {
        std::string name;
        GetPresetDimensions(settings.preset, tape_w_mm, print_w_mm, name);
    }

    int tape_height_px = (int)std::round(tape_w_mm * dots_per_mm);
    if (tape_height_px < 20) tape_height_px = 20;

    int lead_margin_px = (int)std::round(settings.leading_margin_mm * dots_per_mm);
    int trail_margin_px = (int)std::round(settings.trailing_margin_mm * dots_per_mm);
    int gap_px = (int)std::round(settings.label_gap_mm * dots_per_mm);

    // 1. Generate individual label images
    struct PlacedLabel {
        BarcodeImage img;
        int placed_x = 0;
        int placed_y = 0;
        int render_w = 0;
        int render_h = 0;
    };

    std::vector<PlacedLabel> placed_labels;
    placed_labels.reserve(label_texts.size());

    int current_x = lead_margin_px;

    for (size_t i = 0; i < label_texts.size(); i++) {
        BarcodeParams p = base_params;
        p.input = label_texts[i];

        BarcodeImage bimg = engine.generate(p);
        if (!bimg.valid) {
            continue;
        }

        PlacedLabel pl;
        pl.img = std::move(bimg);

        if (!settings.rotate_90) {
            pl.render_w = pl.img.width;
            pl.render_h = pl.img.height;
        } else {
            pl.render_w = pl.img.height;
            pl.render_h = pl.img.width;
        }

        pl.placed_x = current_x;
        if (settings.center_on_tape) {
            pl.placed_y = (tape_height_px - pl.render_h) / 2;
            if (pl.placed_y < 0) pl.placed_y = 0;
        } else {
            pl.placed_y = 4; // Top aligned with small margin
        }

        current_x += pl.render_w + gap_px;
        placed_labels.push_back(std::move(pl));
    }

    if (placed_labels.empty()) {
        strip.error_message = "Ningún código de barras pudo ser generado";
        return strip;
    }

    int total_width_px = current_x - gap_px + trail_margin_px;
    if (total_width_px < 50) total_width_px = 50;

    strip.width = total_width_px;
    strip.height = tape_height_px;
    strip.tape_width_mm = tape_w_mm;
    strip.total_length_mm = (float)total_width_px / dots_per_mm;
    strip.total_labels = (int)placed_labels.size();

    // Allocate RGBA buffer filled with white
    strip.rgba.assign(strip.width * strip.height * 4, 255);

    auto set_pixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || x >= strip.width || y < 0 || y >= strip.height) return;
        size_t idx = (y * strip.width + x) * 4;
        strip.rgba[idx + 0] = r;
        strip.rgba[idx + 1] = g;
        strip.rgba[idx + 2] = b;
        strip.rgba[idx + 3] = 255;
    };

    // 2. Composite labels onto strip
    for (const auto& pl : placed_labels) {
        if (!settings.rotate_90) {
            for (int ly = 0; ly < pl.img.height; ly++) {
                for (int lx = 0; lx < pl.img.width; lx++) {
                    size_t src_idx = (ly * pl.img.width + lx) * 4;
                    uint8_t r = pl.img.rgba[src_idx + 0];
                    uint8_t g = pl.img.rgba[src_idx + 1];
                    uint8_t b = pl.img.rgba[src_idx + 2];
                    set_pixel(pl.placed_x + lx, pl.placed_y + ly, r, g, b);
                }
            }
        } else {
            // 90 deg clockwise rotation
            for (int ly = 0; ly < pl.img.height; ly++) {
                for (int lx = 0; lx < pl.img.width; lx++) {
                    size_t src_idx = (ly * pl.img.width + lx) * 4;
                    uint8_t r = pl.img.rgba[src_idx + 0];
                    uint8_t g = pl.img.rgba[src_idx + 1];
                    uint8_t b = pl.img.rgba[src_idx + 2];

                    int rx = (pl.img.height - 1 - ly);
                    int ry = lx;
                    set_pixel(pl.placed_x + rx, pl.placed_y + ry, r, g, b);
                }
            }
        }

        // Draw cut guideline if enabled
        if (settings.show_cut_lines) {
            int cut_x = pl.placed_x + pl.render_w + (gap_px / 2);
            if (cut_x < strip.width - 2) {
                for (int cy = 0; cy < strip.height; cy++) {
                    // Dashed line pattern
                    if ((cy / 6) % 2 == 0) {
                        set_pixel(cut_x, cy, 180, 180, 180); // subtle gray cut mark
                    }
                }
            }
        }
    }

    // Draw tape edge borders (subtle paper bounds)
    for (int x = 0; x < strip.width; x++) {
        set_pixel(x, 0, 220, 220, 220);
        set_pixel(x, strip.height - 1, 220, 220, 220);
    }

    strip.valid = true;
    return strip;
}
