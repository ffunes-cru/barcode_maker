#include "app.hpp"
#include "win11_theme.hpp"
#include "../../third_party/imgui/imgui.h"

#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>

extern "C" {
#include "../../lib/img/libattopng.h"
}

namespace fs = std::filesystem;

static void upload_texture_rgba(GLuint& tex_id, const std::vector<uint8_t>& rgba, int width, int height) {
    if (width <= 0 || height <= 0 || rgba.empty()) return;

    if (tex_id == 0) {
        glGenTextures(1, &tex_id);
    }
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}

App::App() {
    // Default Brother QL recommended settings from readme.txt:
    // -H 13 --res-fact 8 -C 1 --height-txt 16 --padd-txt-y 1 --padd-y 1 -X 5
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

App::~App() {
    if (single_texture_ != 0) {
        glDeleteTextures(1, &single_texture_);
        single_texture_ = 0;
    }
    if (strip_texture_ != 0) {
        glDeleteTextures(1, &strip_texture_);
        strip_texture_ = 0;
    }
}

bool App::init(const std::string& resource_dir) {
    if (!engine_.init(resource_dir)) {
        status_notification_ = "Error inicializando motor Barcode (verifique diccionarios/fuente)";
        status_notification_timer_ = 8.0f;
        return false;
    }

    // Attempt to load default input_rep.txt if present
    if (fs::exists("input_rep.txt")) {
        load_batch_file("input_rep.txt");
    } else if (fs::exists("../input_rep.txt")) {
        load_batch_file("../input_rep.txt");
    }

    single_dirty_ = true;
    strip_dirty_ = true;
    return true;
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
        // Trim line
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
        }
        single_dirty_ = true;
        strip_dirty_ = true;
    }
}

void App::update_single_texture() {
    if (!single_dirty_) return;
    single_dirty_ = false;

    current_single_img_ = engine_.generate(params_);
    if (current_single_img_.valid) {
        upload_texture_rgba(single_texture_, current_single_img_.rgba,
                            current_single_img_.width, current_single_img_.height);
    }
}

void App::update_strip_texture() {
    if (!strip_dirty_) return;
    strip_dirty_ = false;

    std::vector<std::string> labels_to_render;
    if (input_mode_ == InputMode::BatchFile && strip_settings_.use_batch_list && !batch_items_.empty()) {
        int count = (batch_array_len_limit_ > 0) ? std::min((int)batch_items_.size(), batch_array_len_limit_)
                                                 : (int)batch_items_.size();
        // Limit strip preview to max 50 for memory & GPU texture limits
        int preview_count = std::min(count, 50);
        labels_to_render.assign(batch_items_.begin(), batch_items_.begin() + preview_count);
    } else {
        int count = std::clamp(strip_settings_.repeat_count, 1, 50);
        labels_to_render.assign(count, params_.input);
    }

    current_strip_img_ = StripGenerator::GenerateStrip(engine_, params_, labels_to_render, strip_settings_);
    if (current_strip_img_.valid) {
        upload_texture_rgba(strip_texture_, current_strip_img_.rgba,
                            current_strip_img_.width, current_strip_img_.height);
    }
}

void App::export_current_png() {
    if (!current_single_img_.valid) return;
    fs::create_directories(output_dir_buf_);
    fs::path out = fs::path(output_dir_buf_) / (params_.input + ".png");
    if (engine_.save_png(current_single_img_, out.string())) {
        status_notification_ = "Guardado: " + out.string();
        status_notification_timer_ = 4.0f;
    } else {
        status_notification_ = "Error al guardar: " + out.string();
        status_notification_timer_ = 5.0f;
    }
}

void App::export_strip_png() {
    // Generate full strip (without preview limit if in batch)
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

    // Save strip PNG
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
    // Brother QL recommended preset
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
    single_dirty_ = true;
    strip_dirty_ = true;
    status_notification_ = "Preset Brother QL aplicado (-H 13 -T 16 -R 8 -C 1 -X 5 -Y 1 -y 1)";
    status_notification_timer_ = 3.0f;
}

void App::render_ui() {
    // Update textures if dirty
    update_single_texture();
    update_strip_texture();

    // Notification timer countdown
    if (status_notification_timer_ > 0.0f) {
        status_notification_timer_ -= ImGui::GetIO().DeltaTime;
        if (status_notification_timer_ <= 0.0f) {
            status_notification_.clear();
        }
    }

    // Main viewport window
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

    // --- Header Section ---
    ImGui::BeginGroup();
    {
        ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.0f, 1.0f), "CODE 128 STUDIO");
        ImGui::SameLine();
        ImGui::TextDisabled("| Windows 11 Fluent Barcode Suite");

        ImGui::SameLine(ImGui::GetWindowWidth() - 360.0f);
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
        single_dirty_ = true;
        strip_dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Lote por Archivo (.txt)", input_mode_ == InputMode::BatchFile)) {
        input_mode_ = InputMode::BatchFile;
        if (!batch_items_.empty()) {
            params_.input = batch_items_[selected_batch_index_];
            strncpy(manual_input_buf_, params_.input.c_str(), sizeof(manual_input_buf_) - 1);
        }
        single_dirty_ = true;
        strip_dirty_ = true;
    }

    // Status Banner if active
    if (!status_notification_.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.40f, 0.70f, 0.35f));
        ImGui::BeginChild("NotificationBanner", ImVec2(0, 32), true);
        ImGui::Text("ℹ️  %s", status_notification_.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // Split View Layout
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

    ImGui::End();
}

void App::render_left_panel() {
    // --- Section 1: Input Data ---
    if (ImGui::CollapsingHeader("1. Datos de Entrada", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (input_mode_ == InputMode::Manual) {
            ImGui::Text("Texto a codificar (-s):");
            if (ImGui::InputText("##ManualInput", manual_input_buf_, sizeof(manual_input_buf_))) {
                params_.input = manual_input_buf_;
                single_dirty_ = true;
                strip_dirty_ = true;
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
                    strip_dirty_ = true;
                }
                if (batch_array_len_limit_ == 0) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(Todos)");
                }
            }
        }
    }

    ImGui::Spacing();

    // --- Section 2: Barcode CLI Parameters ---
    if (ImGui::CollapsingHeader("2. Parámetros del Código (CLI)", ImGuiTreeNodeFlags_DefaultOpen)) {
        // -R / res_fact
        ImGui::Text("Factor Resolución (-R):");
        if (ImGui::SliderInt("##ResFact", &params_.res_fact, 1, 16, "%dx")) {
            if (params_.comp_fact > params_.res_fact) params_.comp_fact = params_.res_fact;
            single_dirty_ = true;
            strip_dirty_ = true;
        }

        // -C / comp_fact
        ImGui::Text("Factor Compresión (-C):");
        if (ImGui::SliderInt("##CompFact", &params_.comp_fact, 1, params_.res_fact, "%dx")) {
            single_dirty_ = true;
            strip_dirty_ = true;
        }

        // -H / height
        ImGui::Text("Altura de Barras (-H):");
        if (ImGui::SliderInt("##HeightH", &params_.height, 5, 60, "%d px")) {
            single_dirty_ = true;
            strip_dirty_ = true;
        }

        // -T / height_txt
        ImGui::Text("Altura de Texto (-T):");
        if (ImGui::SliderInt("##HeightT", &params_.height_txt, 4, 35, "%d px")) {
            single_dirty_ = true;
            strip_dirty_ = true;
        }

        // -X / padd_x (Quiet Zone)
        ImGui::Text("Margen X / Quiet Zone (-X):");
        if (ImGui::SliderInt("##PaddX", &params_.padd_x, 0, 20, "%d px")) {
            single_dirty_ = true;
            strip_dirty_ = true;
        }
        if (params_.padd_x < 5) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "⚠️ Quiet zone baja (< 5): riesgo de no lectura");
        }

        // -Y / padd_y
        ImGui::Text("Margen Y (-Y):");
        if (ImGui::SliderInt("##PaddY", &params_.padd_y, 0, 10, "%d px")) {
            single_dirty_ = true;
            strip_dirty_ = true;
        }

        // -y / padd_txt_y
        ImGui::Text("Margen Texto Y (-y):");
        if (ImGui::SliderInt("##PaddTxtY", &params_.padd_txt_y, 0, 10, "%d px")) {
            single_dirty_ = true;
            strip_dirty_ = true;
        }
    }

    ImGui::Spacing();

    // --- Section 3: Brother Label & Strip Settings ---
    if (ImGui::CollapsingHeader("3. Impresora Brother / Tiras", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Roll preset combo
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
            strip_dirty_ = true;
        }

        if (input_mode_ == InputMode::Manual) {
            ImGui::Text("Repeticiones en tira:");
            if (ImGui::SliderInt("##RepeatCount", &strip_settings_.repeat_count, 1, 30, "%d copias")) {
                strip_dirty_ = true;
            }
        } else {
            if (ImGui::Checkbox("Generar tira con lista de lote", &strip_settings_.use_batch_list)) {
                strip_dirty_ = true;
            }
        }

        ImGui::Text("Espaciado entre etiquetas (mm):");
        if (ImGui::SliderFloat("##LabelGap", &strip_settings_.label_gap_mm, 0.0f, 15.0f, "%.1f mm")) {
            strip_dirty_ = true;
        }

        if (ImGui::Checkbox("Mostrar marcas de corte", &strip_settings_.show_cut_lines)) {
            strip_dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Rotar 90°", &strip_settings_.rotate_90)) {
            strip_dirty_ = true;
        }
    }

    ImGui::Spacing();

    // --- Section 4: Export Options ---
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
        if (ImGui::BeginTabItem("🔍 Previsualización en Vivo")) {
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
    if (!current_single_img_.valid) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "❌ Error: %s",
                           current_single_img_.error_message.empty() ? "Código inválido" : current_single_img_.error_message.c_str());
        return;
    }

    // Metric Summary Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::BeginChild("MetricSummaryCard", ImVec2(0, 60), true);
    {
        float mm_w = (float)current_single_img_.width / (300.0f / 25.4f);
        float mm_h = (float)current_single_img_.height / (300.0f / 25.4f);
        ImGui::Columns(4, "metrics_col", false);
        ImGui::Text("Texto: %s", params_.input.c_str());
        ImGui::NextColumn();
        ImGui::Text("Píxeles: %d x %d px", current_single_img_.width, current_single_img_.height);
        ImGui::NextColumn();
        ImGui::Text("Medida @300DPI: %.1f x %.1f mm", mm_w, mm_h);
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "● Válido Code 128");
        ImGui::Columns(1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Render Preview Box with Checkerboard / Canvas Background
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("SingleBarcodeCanvas", ImVec2(avail.x, avail.y - 40), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        if (single_texture_ != 0) {
            float aspect = (float)current_single_img_.width / (float)current_single_img_.height;
            float display_w = (float)current_single_img_.width;
            float display_h = (float)current_single_img_.height;

            // Center image in canvas
            float pad_x = std::max(0.0f, (avail.x - display_w) * 0.5f);
            float pad_y = std::max(0.0f, (avail.y - display_h - 60.0f) * 0.5f);

            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + pad_x, ImGui::GetCursorPosY() + pad_y));
            ImGui::Image((ImTextureID)(intptr_t)single_texture_, ImVec2(display_w, display_h));
        }
    }
    ImGui::EndChild();

    // Bottom info line
    ImGui::TextDisabled("Patrón binario: %s", current_single_img_.encoded_bits.c_str());
}

void App::render_strip_preview_tab() {
    if (!current_strip_img_.valid) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "❌ Error generando tira: %s",
                           current_strip_img_.error_message.c_str());
        return;
    }

    // Strip Metrics Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::BeginChild("StripMetricCard", ImVec2(0, 60), true);
    {
        ImGui::Columns(4, "strip_metrics_col", false);
        ImGui::Text("Ancho Rollo: %.1f mm", current_strip_img_.tape_width_mm);
        ImGui::NextColumn();
        ImGui::Text("Largo Total: %.1f mm (%.1f\")", current_strip_img_.total_length_mm, current_strip_img_.total_length_mm / 25.4f);
        ImGui::NextColumn();
        ImGui::Text("Etiquetas: %d en tira", current_strip_img_.total_labels);
        ImGui::NextColumn();
        ImGui::Text("Resolución: %d DPI", strip_settings_.dpi);
        ImGui::Columns(1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Horizontal Scrollable Tape Canvas
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("StripTapeCanvas", ImVec2(avail.x, avail.y - 10), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        if (strip_texture_ != 0) {
            float display_w = (float)current_strip_img_.width;
            float display_h = (float)current_strip_img_.height;

            ImGui::Image((ImTextureID)(intptr_t)strip_texture_, ImVec2(display_w, display_h));
        }
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
                single_dirty_ = true;
                strip_dirty_ = true;
            }

            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Válido");

            ImGui::TableNextColumn();
            std::string btn_id = "Ver##" + std::to_string(i);
            if (ImGui::SmallButton(btn_id.c_str())) {
                selected_batch_index_ = i;
                params_.input = batch_items_[i];
                strncpy(manual_input_buf_, batch_items_[i].c_str(), sizeof(manual_input_buf_) - 1);
                single_dirty_ = true;
                strip_dirty_ = true;
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

        // Printer Selector
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

        // Mode of what to print
        static int print_target_mode = 0; // 0 = Single Barcode, 1 = Continuous Strip
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
                // Print single barcode
                ok = print_manager_.print_rgba_buffer(current_single_img_.rgba,
                                                      current_single_img_.width,
                                                      current_single_img_.height,
                                                      print_job_settings_, out_msg);
            } else {
                // Print strip
                ok = print_manager_.print_rgba_buffer(current_strip_img_.rgba,
                                                      current_strip_img_.width,
                                                      current_strip_img_.height,
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
