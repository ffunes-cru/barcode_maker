#include "app.hpp"
#include "win11_theme.hpp"
#include "../../third_party/imgui/imgui.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cmath>

extern "C" {
#include "../../lib/img/libattopng.h"
}

namespace fs = std::filesystem;

App::App() {
    // Default Brother QL recommended settings
    params_.input = "A0101";
    params_.height = 13;
    params_.height_txt = 16;
    params_.padd_x = 5;
    params_.padd_y = 1;
    params_.padd_txt_y = 1;
    params_.res_fact = 8;
    params_.comp_fact = 1;

    strip_settings_.preset = BrotherRollPreset::DK_22205_62mm;
    strip_settings_.roll_width_mm = 62.0f;
    strip_settings_.printable_width_mm = 58.0f;
    strip_settings_.repeat_count = 12;
    strip_settings_.label_gap_mm = 3.0f;
    strip_settings_.show_cut_lines = true;
    strip_settings_.rotate_90 = false;
    strip_settings_.center_on_tape = true;

    print_job_settings_.printer_name = print_manager_.get_default_printer();
    print_job_settings_.copies = 1;
    print_job_settings_.fit_to_page = true;
    print_job_settings_.orientation = 0;
}

App::~App() = default;

bool App::init(const std::string& resource_dir) {
    if (!engine_.init(resource_dir)) {
        status_notification_ = "Aviso: Motor de exportación inicializado con advertencias";
        status_notification_timer_ = 4.0f;
    }

    if (fs::exists("input_rep.txt")) {
        load_batch_file("input_rep.txt");
    } else if (fs::exists("../input_rep.txt")) {
        load_batch_file("../input_rep.txt");
    }

    update_barcode_data();
    return true;
}

void App::update_barcode_data() {
    std::string err;
    if (!engine_.validate_text(params_.input, err)) {
        is_code_valid_ = false;
        code_error_msg_ = err;
        current_encoded_bits_.clear();
        return;
    }

    // Generate code in memory
    BarcodeImage img = engine_.generate(params_);
    if (img.valid) {
        is_code_valid_ = true;
        current_encoded_bits_ = img.encoded_bits;
        calculated_width_px_ = img.width;
        calculated_height_px_ = img.height;
    } else {
        is_code_valid_ = false;
        code_error_msg_ = img.error_message;
    }
}

void App::load_batch_file(const std::string& filepath) {
    if (!fs::exists(filepath)) {
        status_notification_ = "Archivo no encontrado: " + filepath;
        status_notification_timer_ = 5.0f;
        return;
    }

    std::ifstream f(filepath);
    if (!f.is_open()) {
        status_notification_ = "No se pudo abrir el archivo: " + filepath;
        status_notification_timer_ = 5.0f;
        return;
    }

    batch_items_.clear();
    std::string line;
    while (std::getline(f, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        size_t last = line.find_last_not_of(" \t\r\n");
        if (first != std::string::npos && last != std::string::npos) {
            std::string code = line.substr(first, last - first + 1);
            if (!code.empty()) {
                batch_items_.push_back(code);
            }
        }
    }

    if (!batch_items_.empty()) {
        selected_batch_index_ = 0;
        strncpy(batch_file_path_, filepath.c_str(), sizeof(batch_file_path_) - 1);
        status_notification_ = "Cargados " + std::to_string(batch_items_.size()) + " códigos desde " + filepath;
        status_notification_timer_ = 4.0f;
        if (input_mode_ == InputMode::BatchFile) {
            params_.input = batch_items_[0];
            strncpy(manual_input_buf_, batch_items_[0].c_str(), sizeof(manual_input_buf_) - 1);
            update_barcode_data();
        }
    }
}

void App::export_current_png() {
    fs::create_directories(output_dir_buf_);
    fs::path out = fs::path(output_dir_buf_) / (params_.input + ".png");
    if (engine_.save_png(params_.input, params_, out.string())) {
        status_notification_ = "Guardado: " + out.string();
        status_notification_timer_ = 4.0f;
    } else {
        status_notification_ = "Error al guardar: " + out.string();
        status_notification_timer_ = 5.0f;
    }
}

void App::export_strip_png() {
    std::vector<std::string> labels_to_render;
    if (input_mode_ == InputMode::BatchFile && strip_settings_.use_batch_list && !batch_items_.empty()) {
        int count = (batch_array_len_limit_ > 0) ? std::min((int)batch_items_.size(), batch_array_len_limit_)
                                                 : (int)batch_items_.size();
        labels_to_render.assign(batch_items_.begin(), batch_items_.begin() + count);
    } else {
        labels_to_render.assign(strip_settings_.repeat_count, params_.input);
    }

    StripImage full_strip = StripGenerator::GenerateStrip(engine_, params_, labels_to_render, strip_settings_);
    if (!full_strip.valid) {
        status_notification_ = "Error generando tira: " + full_strip.error_message;
        status_notification_timer_ = 5.0f;
        return;
    }

    fs::create_directories(output_dir_buf_);
    std::string filename = "tira_" + params_.input + "_" + std::to_string(labels_to_render.size()) + "x.png";
    fs::path out = fs::path(output_dir_buf_) / filename;

    libattopng_t* png = libattopng_new(full_strip.width, full_strip.height, PNG_GRAYSCALE);
    if (png) {
        for (int y = 0; y < full_strip.height; y++) {
            for (int x = 0; x < full_strip.width; x++) {
                size_t idx = (y * full_strip.width + x) * 4;
                uint8_t val = full_strip.rgba[idx];
                libattopng_set_pixel(png, x, y, val);
            }
        }
        libattopng_save(png, out.string().c_str());
        libattopng_destroy(png);
        status_notification_ = "Tira exportada con éxito: " + out.string();
        status_notification_timer_ = 4.0f;
    }
}

void App::export_batch() {
    if (batch_items_.empty()) {
        status_notification_ = "No hay elementos en el lote para exportar";
        status_notification_timer_ = 4.0f;
        return;
    }

    fs::create_directories(output_dir_buf_);
    int total = (batch_array_len_limit_ > 0) ? std::min((int)batch_items_.size(), batch_array_len_limit_)
                                             : (int)batch_items_.size();
    int success_count = 0;

    for (int i = 0; i < total; i++) {
        fs::path out = fs::path(output_dir_buf_) / (batch_items_[i] + ".png");
        if (engine_.save_png(batch_items_[i], params_, out.string())) {
            success_count++;
        }
    }

    status_notification_ = "Lote completado: " + std::to_string(success_count) + " / " + std::to_string(total) + " exportados en " + output_dir_buf_;
    status_notification_timer_ = 5.0f;
}

void App::apply_brother_preset() {
    params_.height = 13;
    params_.height_txt = 16;
    params_.padd_x = 5;
    params_.padd_y = 1;
    params_.padd_txt_y = 1;
    params_.res_fact = 8;
    params_.comp_fact = 1;
    strip_settings_.preset = BrotherRollPreset::DK_22205_62mm;
    strip_settings_.repeat_count = 12;
    strip_settings_.label_gap_mm = 3.0f;
    update_barcode_data();
    status_notification_ = "Preset Brother QL aplicado (-H 13 -T 16 -R 8 -C 1 -X 5 -Y 1 -y 1)";
    status_notification_timer_ = 3.0f;
}

void App::render_ui() {
    // Notification timer countdown
    if (status_notification_timer_ > 0.0f) {
        status_notification_timer_ -= ImGui::GetIO().DeltaTime;
        if (status_notification_timer_ <= 0.0f) {
            status_notification_.clear();
        }
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));

    ImGui::Begin("Code128StudioMainWindow", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // --- Header Bar ---
    ImGui::BeginGroup();
    {
        ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.0f, 1.0f), "CODE 128 STUDIO");
        ImGui::SameLine();
        ImGui::TextDisabled("| Windows 11 Fluent Barcode Suite (GPU Accelerated)");

        ImGui::SameLine(ImGui::GetWindowWidth() - 480.0f);
        if (ImGui::Button("🔄 Actualizaciones")) {
            show_update_dialog_ = true;
            is_checking_update_ = true;
            std::string err;
            AppUpdater::CheckForUpdates(CODE128_GITHUB_REPO, update_info_, err);
            is_checking_update_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Preset Brother QL")) {
            apply_brother_preset();
        }
        ImGui::SameLine();
        if (ImGui::Button("🖨️ Imprimir Directo", ImVec2(140, 0))) {
            show_print_dialog_ = true;
        }
    }
    ImGui::EndGroup();

    ImGui::Separator();
    ImGui::Spacing();

    // Mode Selector
    ImGui::Text("Modo de Entrada:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Manual Directo", input_mode_ == InputMode::Manual)) {
        input_mode_ = InputMode::Manual;
        params_.input = manual_input_buf_;
        update_barcode_data();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Lote por Archivo (.txt)", input_mode_ == InputMode::BatchFile)) {
        input_mode_ = InputMode::BatchFile;
        if (!batch_items_.empty()) {
            params_.input = batch_items_[selected_batch_index_];
            strncpy(manual_input_buf_, params_.input.c_str(), sizeof(manual_input_buf_) - 1);
        }
        update_barcode_data();
    }

    if (!status_notification_.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.40f, 0.70f, 0.35f));
        ImGui::BeginChild("NotificationBanner", ImVec2(0, 32), true);
        ImGui::Text("ℹ️  %s", status_notification_.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    float left_panel_width = 380.0f;
    float right_panel_width = ImGui::GetContentRegionAvail().x - left_panel_width - 12.0f;

    ImGui::BeginChild("LeftPanelChild", ImVec2(left_panel_width, 0), true);
    render_left_panel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RightPanelChild", ImVec2(right_panel_width, 0), true);
    render_right_panel();
    ImGui::EndChild();

    render_print_modal();
    render_update_modal();

    ImGui::End();
}

void App::render_left_panel() {
    // --- 1. Input Data ---
    if (ImGui::CollapsingHeader("1. Datos de Entrada", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (input_mode_ == InputMode::Manual) {
            ImGui::Text("Texto a codificar (-s):");
            if (ImGui::InputText("##ManualInput", manual_input_buf_, sizeof(manual_input_buf_))) {
                params_.input = manual_input_buf_;
                update_barcode_data();
            }
        } else {
            ImGui::Text("Ruta del archivo (-c):");
            ImGui::InputText("##BatchFilePath", batch_file_path_, sizeof(batch_file_path_));
            if (ImGui::Button("Cargar Archivo")) {
                load_batch_file(batch_file_path_);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu códigos)", batch_items_.size());

            if (!batch_items_.empty()) {
                ImGui::Text("Límite de elementos (-A):");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputInt("##LimitA", &batch_array_len_limit_)) {
                    if (batch_array_len_limit_ < 0) batch_array_len_limit_ = 0;
                }
            }
        }
    }

    ImGui::Spacing();

    // --- 2. Barcode CLI Parameters ---
    if (ImGui::CollapsingHeader("2. Parámetros del Código (CLI)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Factor Resolución (-R):");
        if (ImGui::SliderInt("##ResFact", &params_.res_fact, 1, 16, "%dx")) {
            if (params_.comp_fact > params_.res_fact) params_.comp_fact = params_.res_fact;
            update_barcode_data();
        }

        ImGui::Text("Factor Compresión (-C):");
        if (ImGui::SliderInt("##CompFact", &params_.comp_fact, 1, params_.res_fact, "%dx")) {
            update_barcode_data();
        }

        ImGui::Text("Altura de Barras (-H):");
        if (ImGui::SliderInt("##HeightH", &params_.height, 5, 60, "%d px")) {
            update_barcode_data();
        }

        ImGui::Text("Altura de Texto (-T):");
        if (ImGui::SliderInt("##HeightT", &params_.height_txt, 4, 35, "%d px")) {
            update_barcode_data();
        }

        ImGui::Text("Margen X / Quiet Zone (-X):");
        if (ImGui::SliderInt("##PaddX", &params_.padd_x, 0, 20, "%d px")) {
            update_barcode_data();
        }
        if (params_.padd_x < 5) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "⚠️ Quiet zone baja (< 5): riesgo de no lectura");
        }

        ImGui::Text("Margen Y (-Y):");
        if (ImGui::SliderInt("##PaddY", &params_.padd_y, 0, 10, "%d px")) {
            update_barcode_data();
        }

        ImGui::Text("Margen Texto Y (-y):");
        if (ImGui::SliderInt("##PaddTxtY", &params_.padd_txt_y, 0, 10, "%d px")) {
            update_barcode_data();
        }
    }

    ImGui::Spacing();

    // --- 3. Brother Label & Strip Settings ---
    if (ImGui::CollapsingHeader("3. Impresora Brother / Tiras", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* presets[] = {
            "Brother DK-22205 (62 mm Continuo)",
            "Brother DK-22243 (102 mm Continuo)",
            "Brother DK-22210 (29 mm Continuo)",
            "Brother DK-22225 (38 mm Continuo)",
            "Brother DK-11201 (29x90 mm Precortada)",
            "Personalizado"
        };
        int current_preset = (int)strip_settings_.preset;
        ImGui::Text("Rollo / Cinta:");
        if (ImGui::Combo("##PresetCombo", &current_preset, presets, IM_ARRAYSIZE(presets))) {
            strip_settings_.preset = (BrotherRollPreset)current_preset;
            float tw, pw; std::string nm;
            StripGenerator::GetPresetDimensions(strip_settings_.preset, tw, pw, nm);
            strip_settings_.roll_width_mm = tw;
            strip_settings_.printable_width_mm = pw;
        }

        if (input_mode_ == InputMode::Manual) {
            ImGui::Text("Repeticiones en tira:");
            ImGui::SliderInt("##RepeatCount", &strip_settings_.repeat_count, 1, 30, "%d copias");
        } else {
            ImGui::Checkbox("Generar tira con lista de lote", &strip_settings_.use_batch_list);
        }

        ImGui::Text("Espaciado entre etiquetas (mm):");
        ImGui::SliderFloat("##LabelGap", &strip_settings_.label_gap_mm, 0.0f, 15.0f, "%.1f mm");

        ImGui::Checkbox("Mostrar marcas de corte", &strip_settings_.show_cut_lines);
        ImGui::SameLine();
        ImGui::Checkbox("Rotar 90°", &strip_settings_.rotate_90);
    }

    ImGui::Spacing();

    // --- 4. Export Options ---
    if (ImGui::CollapsingHeader("4. Exportación & Salida", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Carpeta de salida (-o):");
        ImGui::InputText("##OutputDir", output_dir_buf_, sizeof(output_dir_buf_));

        if (ImGui::Button("💾 Guardar PNG Individual", ImVec2(-1, 0))) {
            export_current_png();
        }

        if (ImGui::Button("🎞️ Exportar Tira Completa PNG", ImVec2(-1, 0))) {
            export_strip_png();
        }

        if (input_mode_ == InputMode::BatchFile && !batch_items_.empty()) {
            if (ImGui::Button("📦 Exportar Lote Completo PNGs", ImVec2(-1, 0))) {
                export_batch();
            }
        }
    }
}

void App::render_right_panel() {
    if (ImGui::BeginTabBar("MainRightTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("⚡ Previsualización en Caliente (GPU)")) {
            render_live_preview_tab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("🏷️ Vista de Tira / Impresora Brother")) {
            render_strip_preview_tab();
            ImGui::EndTabItem();
        }

        if (input_mode_ == InputMode::BatchFile && ImGui::BeginTabItem("📋 Lista de Lote")) {
            render_batch_table_tab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void App::render_live_preview_tab() {
    if (!is_code_valid_) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "❌ Error: %s", code_error_msg_.c_str());
        return;
    }

    // Metric Summary Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::BeginChild("MetricSummaryCard", ImVec2(0, 56), true);
    {
        float mm_w = (float)calculated_width_px_ / (300.0f / 25.4f);
        float mm_h = (float)calculated_height_px_ / (300.0f / 25.4f);
        ImGui::Columns(4, "metrics_col", false);
        ImGui::Text("Texto: %s", params_.input.c_str());
        ImGui::NextColumn();
        ImGui::Text("Píxeles: %d x %d px", calculated_width_px_, calculated_height_px_);
        ImGui::NextColumn();
        ImGui::Text("Medida @300DPI: %.1f x %.1f mm", mm_w, mm_h);
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "● GPU Instantáneo (60+ FPS)");
        ImGui::Columns(1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Zoom slider
    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat("##PreviewZoom", &preview_zoom_, 0.5f, 3.0f, "%.1fx");
    ImGui::SameLine();
    if (ImGui::SmallButton("100%")) preview_zoom_ = 1.0f;

    ImGui::Spacing();

    // --- 100% GPU Immediate DrawList Canvas ---
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("GPUCanvasChild", ImVec2(avail.x, avail.y - 30), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

        int comp_fact = params_.comp_fact <= 0 ? 1 : params_.comp_fact;
        int res_fact = params_.res_fact <= 0 ? 1 : params_.res_fact;
        int code_res_fac = (int)std::floor((double)res_fact / comp_fact);
        if (code_res_fac < 1) code_res_fac = 1;

        float scale = preview_zoom_;
        float barcode_w = (float)calculated_width_px_ * scale;
        float barcode_h = (float)calculated_height_px_ * scale;

        // Center inside canvas
        float offset_x = std::max(20.0f, (avail.x - barcode_w) * 0.5f);
        float offset_y = std::max(20.0f, (avail.y - barcode_h - 40.0f) * 0.5f);

        ImVec2 card_min = ImVec2(canvas_pos.x + offset_x, canvas_pos.y + offset_y);
        ImVec2 card_max = ImVec2(card_min.x + barcode_w, card_min.y + barcode_h);

        // 1. Draw pure white paper background
        draw_list->AddRectFilled(card_min, card_max, IM_COL32(255, 255, 255, 255), 4.0f);
        draw_list->AddRect(card_min, card_max, IM_COL32(180, 180, 180, 255), 4.0f, 0, 1.0f);

        // 2. Draw black barcode bars directly in GPU vertex buffer
        float bar_start_x = card_min.x + (float)(params_.padd_x * code_res_fac) * scale;
        float bar_y0 = card_min.y + (float)(params_.padd_y * res_fact) * scale;
        float bar_y1 = card_min.y + (float)(params_.height * res_fact) * scale;
        float module_w = (float)code_res_fac * scale;

        int code_len = (int)current_encoded_bits_.length();
        for (int i = 0; i < code_len; i++) {
            if (current_encoded_bits_[i] == '1') {
                float bx0 = bar_start_x + (float)i * module_w;
                float bx1 = bx0 + module_w;
                draw_list->AddRectFilled(ImVec2(bx0, bar_y0), ImVec2(bx1, bar_y1), IM_COL32(0, 0, 0, 255));
            }
        }

        // Additional stop bar
        float stop_extra_x0 = bar_start_x + (float)code_len * module_w;
        float stop_extra_x1 = stop_extra_x0 + module_w * 2.0f;
        draw_list->AddRectFilled(ImVec2(stop_extra_x0, bar_y0), ImVec2(stop_extra_x1, bar_y1), IM_COL32(0, 0, 0, 255));

        // Top border line
        draw_list->AddLine(card_min, ImVec2(card_max.x, card_min.y), IM_COL32(0, 0, 0, 255), 1.0f * scale);

        // 3. Draw Human Readable Text Centered
        float text_font_size = (float)std::max(10, (params_.height_txt - params_.padd_txt_y) * res_fact) * scale;
        ImVec2 text_size = ImGui::CalcTextSize(params_.input.c_str());
        float text_scale = (text_font_size / ImGui::GetFontSize());

        float text_x = card_min.x + (barcode_w - text_size.x * text_scale) * 0.5f;
        float text_y = card_min.y + (float)(params_.height + params_.padd_txt_y) * res_fact * scale;

        draw_list->AddText(ImGui::GetFont(), text_font_size, ImVec2(text_x, text_y),
                           IM_COL32(0, 0, 0, 255), params_.input.c_str());

        // Dummy item to allocate scrollable area
        ImGui::Dummy(ImVec2(offset_x * 2 + barcode_w, offset_y * 2 + barcode_h));
    }
    ImGui::EndChild();

    ImGui::TextDisabled("Patrón binario: %s", current_encoded_bits_.c_str());
}

void App::render_strip_preview_tab() {
    float dots_per_mm = strip_settings_.dpi / 25.4f;
    float tape_w_mm = strip_settings_.roll_width_mm;
    float print_w_mm = strip_settings_.printable_width_mm;
    if (strip_settings_.preset != BrotherRollPreset::CustomRoll) {
        std::string name;
        StripGenerator::GetPresetDimensions(strip_settings_.preset, tape_w_mm, print_w_mm, name);
    }

    std::vector<std::string> labels_to_render;
    if (input_mode_ == InputMode::BatchFile && strip_settings_.use_batch_list && !batch_items_.empty()) {
        int count = (batch_array_len_limit_ > 0) ? std::min((int)batch_items_.size(), batch_array_len_limit_)
                                                 : (int)batch_items_.size();
        labels_to_render.assign(batch_items_.begin(), batch_items_.begin() + count);
    } else {
        labels_to_render.assign(std::clamp(strip_settings_.repeat_count, 1, 30), params_.input);
    }

    float gap_px = strip_settings_.label_gap_mm * dots_per_mm;
    float lead_px = strip_settings_.leading_margin_mm * dots_per_mm;
    float trail_px = strip_settings_.trailing_margin_mm * dots_per_mm;
    float single_w = (float)calculated_width_px_;
    float single_h = (float)calculated_height_px_;

    float total_strip_w = lead_px + (single_w + gap_px) * labels_to_render.size() - gap_px + trail_px;
    float total_strip_h = tape_w_mm * dots_per_mm;
    float total_strip_mm = total_strip_w / dots_per_mm;

    // Strip Metrics Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::BeginChild("StripMetricCard", ImVec2(0, 56), true);
    {
        ImGui::Columns(4, "strip_metrics_col", false);
        ImGui::Text("Ancho Rollo: %.1f mm", tape_w_mm);
        ImGui::NextColumn();
        ImGui::Text("Largo Total: %.1f mm (%.1f\")", total_strip_mm, total_strip_mm / 25.4f);
        ImGui::NextColumn();
        ImGui::Text("Etiquetas: %zu en tira", labels_to_render.size());
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "● Render GPU Tira Activo");
        ImGui::Columns(1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Zoom slider
    ImGui::Text("Escala de Vista:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat("##StripZoom", &strip_zoom_, 0.2f, 1.5f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::SmallButton("Ajustar")) strip_zoom_ = 0.5f;

    ImGui::Spacing();

    // --- GPU Continuous Tape Render ---
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("GPUStripTapeCanvas", ImVec2(avail.x, avail.y - 10), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

        float scale = strip_zoom_;
        float tape_render_w = total_strip_w * scale;
        float tape_render_h = total_strip_h * scale;

        ImVec2 tape_min = ImVec2(canvas_pos.x + 20.0f, canvas_pos.y + 20.0f);
        ImVec2 tape_max = ImVec2(tape_min.x + tape_render_w, tape_min.y + tape_render_h);

        // 1. Draw continuous paper tape roll
        draw_list->AddRectFilled(tape_min, tape_max, IM_COL32(250, 250, 250, 255), 2.0f);
        draw_list->AddRect(tape_min, tape_max, IM_COL32(160, 160, 160, 255), 2.0f, 0, 1.0f);

        // 2. Draw each label along the tape
        int comp_fact = params_.comp_fact <= 0 ? 1 : params_.comp_fact;
        int res_fact = params_.res_fact <= 0 ? 1 : params_.res_fact;
        int code_res_fac = (int)std::floor((double)res_fact / comp_fact);
        if (code_res_fac < 1) code_res_fac = 1;

        float cur_x = tape_min.x + lead_px * scale;
        float label_y0 = tape_min.y + ((tape_render_h - single_h * scale) * 0.5f);

        for (size_t l = 0; l < labels_to_render.size(); l++) {
            float label_x0 = cur_x;
            float label_x1 = label_x0 + single_w * scale;

            // Draw bars
            float bar_start_x = label_x0 + (float)(params_.padd_x * code_res_fac) * scale;
            float bar_y0 = label_y0 + (float)(params_.padd_y * res_fact) * scale;
            float bar_y1 = label_y0 + (float)(params_.height * res_fact) * scale;
            float module_w = (float)code_res_fac * scale;

            int code_len = (int)current_encoded_bits_.length();
            for (int i = 0; i < code_len; i++) {
                if (current_encoded_bits_[i] == '1') {
                    float bx0 = bar_start_x + (float)i * module_w;
                    float bx1 = bx0 + module_w;
                    draw_list->AddRectFilled(ImVec2(bx0, bar_y0), ImVec2(bx1, bar_y1), IM_COL32(0, 0, 0, 255));
                }
            }

            // Stop bar
            float stop_x0 = bar_start_x + (float)code_len * module_w;
            float stop_x1 = stop_x0 + module_w * 2.0f;
            draw_list->AddRectFilled(ImVec2(stop_x0, bar_y0), ImVec2(stop_x1, bar_y1), IM_COL32(0, 0, 0, 255));

            // Top border
            draw_list->AddLine(ImVec2(label_x0, label_y0), ImVec2(label_x1, label_y0), IM_COL32(0, 0, 0, 255), 1.0f * scale);

            // Human readable text
            float text_font_size = (float)std::max(8, (params_.height_txt - params_.padd_txt_y) * res_fact) * scale;
            ImVec2 text_size = ImGui::CalcTextSize(labels_to_render[l].c_str());
            float text_scale = (text_font_size / ImGui::GetFontSize());
            float text_x = label_x0 + (single_w * scale - text_size.x * text_scale) * 0.5f;
            float text_y = label_y0 + (float)(params_.height + params_.padd_txt_y) * res_fact * scale;

            draw_list->AddText(ImGui::GetFont(), text_font_size, ImVec2(text_x, text_y),
                               IM_COL32(0, 0, 0, 255), labels_to_render[l].c_str());

            // Cut guideline
            if (strip_settings_.show_cut_lines && l < labels_to_render.size() - 1) {
                float cut_x = label_x1 + (gap_px * scale * 0.5f);
                for (float cy = tape_min.y; cy < tape_max.y; cy += 8.0f) {
                    draw_list->AddLine(ImVec2(cut_x, cy), ImVec2(cut_x, std::min(tape_max.y, cy + 4.0f)),
                                       IM_COL32(180, 50, 50, 200), 1.5f);
                }
            }

            cur_x += (single_w + gap_px) * scale;
        }

        ImGui::Dummy(ImVec2(tape_render_w + 40.0f, tape_render_h + 40.0f));
    }
    ImGui::EndChild();
}

void App::render_batch_table_tab() {
    if (batch_items_.empty()) {
        ImGui::TextDisabled("No hay archivo de lote cargado.");
        return;
    }

    ImGui::Text("Filtrar códigos:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(250);
    ImGui::InputText("##SearchFilter", search_filter_buf_, sizeof(search_filter_buf_));

    std::string filter(search_filter_buf_);

    ImGui::Spacing();

    static ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                         ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("BatchItemsTable", 4, table_flags, ImVec2(0, ImGui::GetContentRegionAvail().y - 10))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("Código", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Estado", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Acción", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)batch_items_.size(); i++) {
            if (!filter.empty() && batch_items_[i].find(filter) == std::string::npos) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", i + 1);

            ImGui::TableNextColumn();
            bool is_selected = (selected_batch_index_ == i);
            if (ImGui::Selectable(batch_items_[i].c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_batch_index_ = i;
                params_.input = batch_items_[i];
                strncpy(manual_input_buf_, batch_items_[i].c_str(), sizeof(manual_input_buf_) - 1);
                update_barcode_data();
            }

            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Válido");

            ImGui::TableNextColumn();
            std::string btn_id = "Ver##" + std::to_string(i);
            if (ImGui::SmallButton(btn_id.c_str())) {
                selected_batch_index_ = i;
                params_.input = batch_items_[i];
                strncpy(manual_input_buf_, batch_items_[i].c_str(), sizeof(manual_input_buf_) - 1);
                update_barcode_data();
            }
        }
        ImGui::EndTable();
    }
}

void App::render_print_modal() {
    if (!show_print_dialog_) return;

    ImGui::OpenPopup("Diálogo de Impresión Directa");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Diálogo de Impresión Directa", &show_print_dialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.0f, 1.0f), "🖨️ Enviar Trabajo de Impresión");
        ImGui::Separator();
        ImGui::Spacing();

        const auto& printers = print_manager_.get_available_printers();
        ImGui::Text("Impresora Destino:");

        static int selected_printer_idx = 0;
        if (!printers.empty()) {
            if (ImGui::BeginCombo("##PrinterCombo", printers[selected_printer_idx].c_str())) {
                for (int i = 0; i < (int)printers.size(); i++) {
                    bool is_selected = (selected_printer_idx == i);
                    if (ImGui::Selectable(printers[i].c_str(), is_selected)) {
                        selected_printer_idx = i;
                        print_job_settings_.printer_name = printers[i];
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            static char custom_printer[128] = "Brother_QL-1110NWB";
            ImGui::InputText("##CustomPrinter", custom_printer, sizeof(custom_printer));
            print_job_settings_.printer_name = custom_printer;
        }

        ImGui::Spacing();

        static int print_target_mode = 0;
        ImGui::Text("Contenido a Imprimir:");
        ImGui::RadioButton("Código Actual (Etiqueta Individual)", &print_target_mode, 0);
        ImGui::RadioButton("Tira Continua Completa (Rollo Brother)", &print_target_mode, 1);

        ImGui::Spacing();

        ImGui::Text("Copias:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("##PrintCopies", &print_job_settings_.copies);
        if (print_job_settings_.copies < 1) print_job_settings_.copies = 1;

        ImGui::Checkbox("Ajustar a la página (fit-to-page)", &print_job_settings_.fit_to_page);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("🚀 Imprimir Ahora", ImVec2(150, 0))) {
            std::string out_msg;
            bool ok = false;
            if (print_target_mode == 0) {
                BarcodeImage single = engine_.generate(params_);
                ok = print_manager_.print_rgba_buffer(single.rgba, single.width, single.height,
                                                      print_job_settings_, out_msg);
            } else {
                std::vector<std::string> labels;
                if (input_mode_ == InputMode::BatchFile && strip_settings_.use_batch_list && !batch_items_.empty()) {
                    int count = (batch_array_len_limit_ > 0) ? std::min((int)batch_items_.size(), batch_array_len_limit_)
                                                             : (int)batch_items_.size();
                    labels.assign(batch_items_.begin(), batch_items_.begin() + count);
                } else {
                    labels.assign(strip_settings_.repeat_count, params_.input);
                }
                StripImage strip = StripGenerator::GenerateStrip(engine_, params_, labels, strip_settings_);
                ok = print_manager_.print_rgba_buffer(strip.rgba, strip.width, strip.height,
                                                      print_job_settings_, out_msg);
            }
            status_notification_ = out_msg;
            status_notification_timer_ = 6.0f;
            show_print_dialog_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(100, 0))) {
            show_print_dialog_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void App::render_update_modal() {
    if (!show_update_dialog_) return;

    ImGui::OpenPopup("Actualización de Code128 Studio");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 260), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Actualización de Code128 Studio", &show_update_dialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.0f, 1.0f), "🔄 Actualizador Automático (Hot Update)");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Versión actual instalada: v%s", CODE128_APP_VERSION);

        if (is_checking_update_) {
            ImGui::Text("Conectando con GitHub Releases...");
        } else if (update_info_.update_available) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "¡Nueva versión disponible: %s!", update_info_.latest_version.c_str());
            ImGui::Spacing();

            if (!is_performing_update_) {
                if (ImGui::Button("⚡ Actualizar Ahora (Hot Update)", ImVec2(220, 0))) {
                    is_performing_update_ = true;
                    std::string err;
                    bool ok = AppUpdater::PerformHotUpdate(update_info_, [this](float p, const std::string& msg) {
                        update_progress_ = p;
                        update_status_text_ = msg;
                    }, err);

                    if (ok) {
                        status_notification_ = "¡Actualización instalada con éxito!";
                        status_notification_timer_ = 5.0f;
                        show_update_dialog_ = false;
                        ImGui::CloseCurrentPopup();
                    } else {
                        update_status_text_ = "Error: " + err;
                    }
                    is_performing_update_ = false;
                }
            } else {
                ImGui::ProgressBar(update_progress_, ImVec2(-1, 0));
                ImGui::Text("%s", update_status_text_.c_str());
            }
        } else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "✅ Ya tienes la última versión instalada.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Cerrar", ImVec2(100, 0))) {
            show_update_dialog_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
