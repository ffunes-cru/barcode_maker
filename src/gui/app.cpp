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
    params_.input = "A0100";
    params_.module_width = 8.5f;       // ~0.72 mm @ 300 DPI
    params_.bar_height = 110.0f;       // ~9.3 mm
    params_.text_size = 38.0f;         // ~3.2 mm
    params_.quiet_zone_x = 12.0f;      // 12 modules
    params_.margin_y = 10.0f;          // ~0.8 mm
    params_.text_gap_y = 8.0f;

    strip_settings_.preset = BrotherRollPreset::DK_22246_103mm;
    strip_settings_.roll_width_mm = 103.6f;
    strip_settings_.printable_width_mm = 99.0f;
    strip_settings_.vertical_feed = true;
    strip_settings_.repeat_count = 10;
    strip_settings_.label_gap_mm = 4.0f;
    strip_settings_.show_cut_lines = true;
    strip_settings_.rotate_90 = false;
    strip_settings_.center_on_tape = true;

    print_job_settings_.printer_name = "Brother_QL-1110NWB";
    print_job_settings_.copies = 1;
    print_job_settings_.fit_to_page = true;
    print_job_settings_.orientation = 0;
}

App::~App() = default;

bool App::init(const std::string& resource_dir) {
    if (!engine_.init(resource_dir)) {
        status_notification_ = "Aviso: Motor inicializado con advertencias";
        status_notification_timer_ = 4.0f;
    }

    preset_manager_.init(resource_dir);

    if (fs::exists("input_rep.txt")) {
        load_batch_file("input_rep.txt");
    } else if (fs::exists("../input_rep.txt")) {
        load_batch_file("../input_rep.txt");
    }

    update_barcode_data();
    return true;
}

void App::apply_preset(const Preset& p) {
    params_.module_width = p.module_width;
    params_.bar_height = p.bar_height;
    params_.text_size = p.text_size;
    params_.quiet_zone_x = p.quiet_zone_x;
    params_.margin_y = p.margin_y;
    params_.text_gap_y = p.text_gap_y;

    strip_settings_.roll_width_mm = p.roll_width_mm;
    strip_settings_.printable_width_mm = p.printable_width_mm;
    strip_settings_.label_gap_mm = p.label_gap_mm;
    strip_settings_.repeat_count = p.repeat_count;
    strip_settings_.rotate_90 = p.rotate_90;
    strip_settings_.show_cut_lines = p.show_cut_lines;

    update_barcode_data();
    status_notification_ = "Preset '" + p.name + "' aplicado";
    status_notification_timer_ = 3.5f;
}

void App::apply_preset_by_index(size_t index) {
    const Preset* p = preset_manager_.get_preset(index);
    if (p) {
        selected_preset_idx_ = (int)index;
        apply_preset(*p);
    }
}

void App::update_barcode_data() {
    std::string err;
    if (!engine_.validate_text(params_.input, err)) {
        is_code_valid_ = false;
        code_error_msg_ = err;
        current_encoded_bits_.clear();
        return;
    }

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
        status_notification_ = "Cargados " + std::to_string(batch_items_.size()) + " codigos desde " + filepath;
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
        status_notification_ = "Tira exportada con exito: " + out.string();
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

    status_notification_ = "Lote completado: " + std::to_string(success_count) + " / " + std::to_string(total) + " exportados";
    status_notification_timer_ = 5.0f;
}

void App::render_ui() {
    if (status_notification_timer_ > 0.0f) {
        status_notification_timer_ -= ImGui::GetIO().DeltaTime;
        if (status_notification_timer_ <= 0.0f) {
            status_notification_.clear();
        }
    }

    if (abm_status_timer_ > 0.0f) {
        abm_status_timer_ -= ImGui::GetIO().DeltaTime;
        if (abm_status_timer_ <= 0.0f) {
            abm_status_msg_.clear();
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

    ImGui::Begin("Code128StudioMainWindow", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    // --- Top Action Header (Responsive Layout & ABM Entrypoint) ---
    ImGui::BeginGroup();
    {
        ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.0f, 1.0f), "CODE 128 STUDIO");
        ImGui::SameLine();
        ImGui::TextDisabled("| Brother QL-1110NWB v%s", CODE128_APP_VERSION);

        float pad_x = ImGui::GetStyle().FramePadding.x * 2.0f;
        float b1_w = ImGui::CalcTextSize("[ Actualizaciones ]").x + pad_x + 8.0f;
        float b2_w = ImGui::CalcTextSize("[ ABM Presets ]").x + pad_x + 8.0f;
        float b3_w = ImGui::CalcTextSize("[ Imprimir Directo ]").x + pad_x + 8.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float total_w = b1_w + b2_w + b3_w + (spacing * 2.0f);

        float avail = ImGui::GetContentRegionAvail().x;
        if (avail > total_w) {
            ImGui::SameLine(ImGui::GetWindowWidth() - total_w - ImGui::GetStyle().WindowPadding.x - 4.0f);
        } else {
            ImGui::SameLine();
        }

        if (ImGui::Button("[ Actualizaciones ]", ImVec2(b1_w, 0))) {
            show_update_dialog_ = true;
            is_checking_update_ = true;
            std::string err;
            AppUpdater::CheckForUpdates(CODE128_GITHUB_REPO, update_info_, err);
            is_checking_update_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("[ ABM Presets ]", ImVec2(b2_w, 0))) {
            show_preset_abm_modal_ = true;
            abm_selected_preset_idx_ = selected_preset_idx_;
            const Preset* p = preset_manager_.get_preset(abm_selected_preset_idx_);
            if (p) {
                strncpy(abm_preset_name_buf_, p->name.c_str(), sizeof(abm_preset_name_buf_) - 1);
                strncpy(abm_preset_desc_buf_, p->description.c_str(), sizeof(abm_preset_desc_buf_) - 1);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("[ Imprimir Directo ]", ImVec2(b3_w, 0))) {
            show_print_dialog_ = true;
        }
    }
    ImGui::EndGroup();

    ImGui::Separator();
    ImGui::Spacing();

    // Mode Selector
    ImGui::Text("Modo de Entrada:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Manual", input_mode_ == InputMode::Manual)) {
        input_mode_ = InputMode::Manual;
        params_.input = manual_input_buf_;
        update_barcode_data();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Archivo de Lote (.txt)", input_mode_ == InputMode::BatchFile)) {
        input_mode_ = InputMode::BatchFile;
        if (!batch_items_.empty()) {
            params_.input = batch_items_[selected_batch_index_];
            strncpy(manual_input_buf_, params_.input.c_str(), sizeof(manual_input_buf_) - 1);
        }
        update_barcode_data();
    }

    if (!status_notification_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[INFO] %s", status_notification_.c_str());
    }

    ImGui::Spacing();

    float left_panel_width = 405.0f;
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
    render_preset_abm_modal();

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
            ImGui::Text("(%zu codigos)", batch_items_.size());

            if (!batch_items_.empty()) {
                ImGui::Text("Limite de elementos (-A):");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputInt("##LimitA", &batch_array_len_limit_)) {
                    if (batch_array_len_limit_ < 0) batch_array_len_limit_ = 0;
                }
            }
        }
    }

    ImGui::Spacing();

    // --- 2. Fractional Barcode Geometry Parameters ---
    if (ImGui::CollapsingHeader("2. Geometria Fraccional", ImGuiTreeNodeFlags_DefaultOpen)) {
        float mod_mm = params_.module_width / (300.0f / 25.4f);
        ImGui::Text("Ancho de Modulo (X-Dim): %.2f px (%.2f mm)", params_.module_width, mod_mm);
        if (ImGui::SliderFloat("##ModWidth", &params_.module_width, 1.0f, 16.0f, "%.2f px")) {
            update_barcode_data();
        }

        float bar_h_mm = params_.bar_height / (300.0f / 25.4f);
        ImGui::Text("Altura de Barras: %.1f px (%.1f mm)", params_.bar_height, bar_h_mm);
        if (ImGui::SliderFloat("##BarHeight", &params_.bar_height, 10.0f, 250.0f, "%.1f px")) {
            update_barcode_data();
        }

        float txt_mm = params_.text_size / (300.0f / 25.4f);
        ImGui::Text("Tamano de Texto: %.1f px (%.1f mm)", params_.text_size, txt_mm);
        if (ImGui::SliderFloat("##TextSize", &params_.text_size, 8.0f, 60.0f, "%.1f px")) {
            update_barcode_data();
        }

        float qz_mm = (params_.quiet_zone_x * params_.module_width) / (300.0f / 25.4f);
        ImGui::Text("Quiet Zone (Margen X): %.1f mod (%.1f mm)", params_.quiet_zone_x, qz_mm);
        if (ImGui::SliderFloat("##QuietZone", &params_.quiet_zone_x, 2.0f, 30.0f, "%.1f mod")) {
            update_barcode_data();
        }
        if (params_.quiet_zone_x < 10.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "[!] Quiet zone recomendada: >= 10 modulos");
        }

        ImGui::Text("Margen Superior / Inferior (Y):");
        if (ImGui::SliderFloat("##MarginY", &params_.margin_y, 0.0f, 30.0f, "%.1f px")) {
            update_barcode_data();
        }

        ImGui::Text("Espacio Barras-Texto (Y):");
        if (ImGui::SliderFloat("##TextGapY", &params_.text_gap_y, 0.0f, 30.0f, "%.1f px")) {
            update_barcode_data();
        }
    }

    ImGui::Spacing();

    // --- 3. Brother Label & Presets Management (ABM integrado) ---
    if (ImGui::CollapsingHeader("3. Impresora Brother QL y Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& presets = preset_manager_.get_presets();
        ImGui::Text("Preset Activo:");

        if (!presets.empty()) {
            if (selected_preset_idx_ >= (int)presets.size()) selected_preset_idx_ = 0;
            const char* current_preset_name = presets[selected_preset_idx_].name.c_str();

            if (ImGui::BeginCombo("##PresetActiveCombo", current_preset_name)) {
                for (size_t i = 0; i < presets.size(); i++) {
                    bool is_selected = (selected_preset_idx_ == (int)i);
                    std::string label = presets[i].name + (presets[i].is_builtin ? " [Fabrica]" : " [Usuario]");
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        apply_preset_by_index(i);
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Columns(2, "preset_quick_actions", false);
        if (ImGui::Button("+ Guardar Nuevo", ImVec2(-1, 0))) {
            show_preset_abm_modal_ = true;
            snprintf(abm_preset_name_buf_, sizeof(abm_preset_name_buf_), "Preset %s (%.1fmm)",
                     params_.input.c_str(), strip_settings_.roll_width_mm);
            snprintf(abm_preset_desc_buf_, sizeof(abm_preset_desc_buf_), "Creado por el usuario");
        }
        ImGui::NextColumn();
        if (ImGui::Button("Gestionar ABM...", ImVec2(-1, 0))) {
            show_preset_abm_modal_ = true;
            abm_selected_preset_idx_ = selected_preset_idx_;
            const Preset* p = preset_manager_.get_preset(abm_selected_preset_idx_);
            if (p) {
                strncpy(abm_preset_name_buf_, p->name.c_str(), sizeof(abm_preset_name_buf_) - 1);
                strncpy(abm_preset_desc_buf_, p->description.c_str(), sizeof(abm_preset_desc_buf_) - 1);
            }
        }
        ImGui::Columns(1);

        ImGui::Spacing();

        if (ImGui::Button("[ Auto-Ajustar al Ancho de Rollo ]", ImVec2(-1, 0))) {
            float dots_per_mm = strip_settings_.dpi / 25.4f;
            float printable_w_px = strip_settings_.printable_width_mm * dots_per_mm;
            int code_len = (int)current_encoded_bits_.length();
            if (code_len > 0) {
                float total_mods = (float)code_len + 2.0f + (params_.quiet_zone_x * 2.0f);
                if (!strip_settings_.rotate_90) {
                    params_.module_width = std::clamp(printable_w_px / total_mods, 1.0f, 16.0f);
                } else {
                    params_.bar_height = std::clamp(printable_w_px - (params_.margin_y * 2.0f + params_.text_size + params_.text_gap_y), 20.0f, 300.0f);
                }
                update_barcode_data();
            }
        }

        ImGui::Text("Ancho Cinta / Rollo (mm):");
        ImGui::SetNextItemWidth(130);
        if (ImGui::InputFloat("##TapeWidthMm", &strip_settings_.roll_width_mm, 1.0f, 5.0f, "%.1f mm")) {
            if (strip_settings_.roll_width_mm < 10.0f) strip_settings_.roll_width_mm = 10.0f;
            if (strip_settings_.printable_width_mm > strip_settings_.roll_width_mm) {
                strip_settings_.printable_width_mm = strip_settings_.roll_width_mm - 4.0f;
            }
        }
        ImGui::SameLine();
        ImGui::Text("Imprimible:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputFloat("##PrintWidthMm", &strip_settings_.printable_width_mm, 1.0f, 5.0f, "%.1f mm")) {
            if (strip_settings_.printable_width_mm > strip_settings_.roll_width_mm) {
                strip_settings_.printable_width_mm = strip_settings_.roll_width_mm;
            }
            if (strip_settings_.printable_width_mm < 5.0f) strip_settings_.printable_width_mm = 5.0f;
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
        ImGui::Checkbox("Rotar 90 deg", &strip_settings_.rotate_90);
    }

    ImGui::Spacing();

    // --- 4. Export Options ---
    if (ImGui::CollapsingHeader("4. Exportacion y Salida", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Carpeta de salida (-o):");
        ImGui::InputText("##OutputDir", output_dir_buf_, sizeof(output_dir_buf_));

        if (ImGui::Button("[ Guardar PNG Individual ]", ImVec2(-1, 0))) {
            export_current_png();
        }

        if (ImGui::Button("[ Exportar Tira Completa PNG ]", ImVec2(-1, 0))) {
            export_strip_png();
        }

        if (input_mode_ == InputMode::BatchFile && !batch_items_.empty()) {
            if (ImGui::Button("[ Exportar Lote Completo PNGs ]", ImVec2(-1, 0))) {
                export_batch();
            }
        }
    }
}

void App::render_right_panel() {
    ImGuiTabItemFlags tab1_flags = (request_tab_switch_ && target_tab_index_ == 0) ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags tab2_flags = (request_tab_switch_ && target_tab_index_ == 1) ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags tab3_flags = (request_tab_switch_ && target_tab_index_ == 2) ? ImGuiTabItemFlags_SetSelected : 0;
    request_tab_switch_ = false;

    if (ImGui::BeginTabBar("MainRightTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Previsualizacion Individual", nullptr, tab1_flags)) {
            render_live_preview_tab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Tira Continua Brother QL", nullptr, tab2_flags)) {
            render_strip_preview_tab();
            ImGui::EndTabItem();
        }

        if (input_mode_ == InputMode::BatchFile && ImGui::BeginTabItem("Lista de Lote", nullptr, tab3_flags)) {
            render_batch_table_tab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void App::render_live_preview_tab() {
    if (!is_code_valid_) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[ERROR] %s", code_error_msg_.c_str());
        return;
    }

    // Professional Clean Metric Card
    ImGui::BeginChild("MetricSummaryCard", ImVec2(0, 48), true);
    {
        float mm_w = (float)calculated_width_px_ / (300.0f / 25.4f);
        float mm_h = (float)calculated_height_px_ / (300.0f / 25.4f);
        float cm_w = mm_w / 10.0f;
        float cm_h = mm_h / 10.0f;

        ImGui::Columns(4, "metrics_col", false);
        ImGui::Text("Texto: %s", params_.input.c_str());
        ImGui::NextColumn();
        ImGui::Text("Pixeles: %d x %d px", calculated_width_px_, calculated_height_px_);
        ImGui::NextColumn();
        ImGui::Text("Medida: %.1f x %.1f cm (%.1f mm)", cm_w, cm_h, mm_w);
        ImGui::NextColumn();
        ImGui::Text("Modulo X: %.2f px (%.2f mm)", params_.module_width, params_.module_width / (300.0f / 25.4f));
        ImGui::Columns(1);
    }
    ImGui::EndChild();

    ImGui::Spacing();

    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat("##PreviewZoom", &preview_zoom_, 0.5f, 3.0f, "%.1fx");
    ImGui::SameLine();
    if (ImGui::SmallButton("100%")) preview_zoom_ = 1.0f;

    ImGui::Spacing();

    // --- GPU Direct DrawList Canvas with Unified Fractional Math & QZ Visual Indicators ---
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("GPUCanvasChild", ImVec2(avail.x, avail.y - 30), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

        float scale = preview_zoom_;
        float barcode_w = (float)calculated_width_px_ * scale;
        float barcode_h = (float)calculated_height_px_ * scale;

        float offset_x = std::max(20.0f, (avail.x - barcode_w) * 0.5f);
        float offset_y = std::max(20.0f, (avail.y - barcode_h - 40.0f) * 0.5f);

        ImVec2 card_min = ImVec2(canvas_pos.x + offset_x, canvas_pos.y + offset_y);
        ImVec2 card_max = ImVec2(card_min.x + barcode_w, card_min.y + barcode_h);

        // White paper background
        draw_list->AddRectFilled(card_min, card_max, IM_COL32(255, 255, 255, 255), 2.0f);
        draw_list->AddRect(card_min, card_max, IM_COL32(90, 90, 90, 255), 2.0f);

        // Draw barcode bars directly in GPU using unified float module_width
        float mod_w = params_.module_width * scale;
        float bar_start_x = card_min.x + (params_.quiet_zone_x * mod_w);
        float bar_y0 = card_min.y + (params_.margin_y * scale);
        float bar_y1 = bar_y0 + (params_.bar_height * scale);

        int code_len = (int)current_encoded_bits_.length();
        for (int i = 0; i < code_len; i++) {
            if (current_encoded_bits_[i] == '1') {
                float bx0 = bar_start_x + (float)i * mod_w;
                float bx1 = bx0 + mod_w;
                draw_list->AddRectFilled(ImVec2(bx0, bar_y0), ImVec2(bx1, bar_y1), IM_COL32(0, 0, 0, 255));
            }
        }

        // Additional stop bar (2 modules wide)
        float stop_x0 = bar_start_x + (float)code_len * mod_w;
        float stop_x1 = stop_x0 + (2.0f * mod_w);
        draw_list->AddRectFilled(ImVec2(stop_x0, bar_y0), ImVec2(stop_x1, bar_y1), IM_COL32(0, 0, 0, 255));

        // Subtle Quiet Zone indicators (highlighting the safety margin)
        draw_list->AddRectFilled(ImVec2(card_min.x, bar_y0), ImVec2(bar_start_x, bar_y1),
                                 IM_COL32(180, 220, 255, 45));
        draw_list->AddRectFilled(ImVec2(stop_x1, bar_y0), ImVec2(card_max.x, bar_y1),
                                 IM_COL32(180, 220, 255, 45));

        // High-Quality Crisp Text Rendering
        ImFont* text_font = font_barcode_ ? font_barcode_ : ImGui::GetFont();
        float target_font_size = params_.text_size * scale;
        
        ImVec2 text_size = text_font->CalcTextSizeA(target_font_size, FLT_MAX, 0.0f, params_.input.c_str());
        float text_x = card_min.x + (barcode_w - text_size.x) * 0.5f;
        float text_y = bar_y1 + (params_.text_gap_y * scale);

        draw_list->AddText(text_font, target_font_size, ImVec2(text_x, text_y),
                           IM_COL32(0, 0, 0, 255), params_.input.c_str());

        ImGui::Dummy(ImVec2(offset_x * 2 + barcode_w, offset_y * 2 + barcode_h));
    }
    ImGui::EndChild();

    ImGui::Text("Patron binario: %s", current_encoded_bits_.c_str());
}

void App::render_strip_preview_tab() {
    float dots_per_mm = strip_settings_.dpi / 25.4f;
    float tape_w_mm = strip_settings_.roll_width_mm;
    float print_w_mm = strip_settings_.printable_width_mm;

    std::vector<std::string> labels_to_render;
    if (input_mode_ == InputMode::BatchFile && strip_settings_.use_batch_list && !batch_items_.empty()) {
        int count = (batch_array_len_limit_ > 0) ? std::min((int)batch_items_.size(), batch_array_len_limit_)
                                                 : (int)batch_items_.size();
        labels_to_render.assign(batch_items_.begin(), batch_items_.begin() + count);
    } else {
        labels_to_render.assign(std::clamp(strip_settings_.repeat_count, 1, 30), params_.input);
    }

    float single_w = (float)calculated_width_px_;
    float single_h = (float)calculated_height_px_;
    float gap_px = strip_settings_.label_gap_mm * dots_per_mm;
    float lead_px = strip_settings_.leading_margin_mm * dots_per_mm;
    float trail_px = strip_settings_.trailing_margin_mm * dots_per_mm;

    float label_on_tape_w = strip_settings_.rotate_90 ? single_h : single_w;
    float label_on_tape_h = strip_settings_.rotate_90 ? single_w : single_h;

    float tape_width_px = tape_w_mm * dots_per_mm;
    float total_strip_h = lead_px + (label_on_tape_h + gap_px) * labels_to_render.size() - gap_px + trail_px;
    float total_strip_mm = total_strip_h / dots_per_mm;
    float total_strip_cm = total_strip_mm / 10.0f;
    float tape_w_cm = tape_w_mm / 10.0f;

    bool barcode_exceeds_tape = (label_on_tape_w > tape_width_px);
    float barcode_mm_w = label_on_tape_w / dots_per_mm;
    float qz_mm = (params_.quiet_zone_x * params_.module_width) / dots_per_mm;
    float tape_margin_mm = std::max(0.0f, (tape_w_mm - barcode_mm_w) * 0.5f);
    float total_free_mm = tape_margin_mm + qz_mm;

    ImGui::BeginChild("StripMetricCard", ImVec2(0, 52), true);
    {
        ImGui::Columns(4, "strip_metrics_col", false);
        ImGui::Text("Ancho Rollo: %.1f cm (%.1f mm)", tape_w_cm, tape_w_mm);
        ImGui::NextColumn();
        ImGui::Text("Largo Tira: %.1f cm (%.1f in)", total_strip_cm, total_strip_mm / 25.4f);
        ImGui::NextColumn();
        ImGui::Text("Etiquetas: %zu en tira", labels_to_render.size());
        ImGui::NextColumn();
        if (barcode_exceeds_tape) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[!] Excede Rollo (%.1f mm)", barcode_mm_w);
        } else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Margen Total: %.1f mm", total_free_mm);
            ImGui::TextDisabled("Cinta: %.1f mm | QZ: %.1f mm", tape_margin_mm, qz_mm);
        }
        ImGui::Columns(1);
    }
    ImGui::EndChild();

    if (barcode_exceeds_tape) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
            "[!] Aviso: El codigo mide %.1f mm y excede el rollo de %.1f mm. Activa 'Rotar 90 deg' o presiona 'Auto-Ajustar a Ancho de Rollo'.",
            barcode_mm_w, tape_w_mm);
    }

    ImGui::Spacing();

    ImGui::Text("Escala de Vista:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat("##StripZoom", &strip_zoom_, 0.2f, 2.0f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::SmallButton("100%")) strip_zoom_ = 1.0f;
    ImGui::SameLine();
    if (ImGui::SmallButton("Ajustar al Ancho")) {
        float avail_w = ImGui::GetContentRegionAvail().x - 60.0f;
        if (tape_width_px > 0) strip_zoom_ = std::clamp(avail_w / tape_width_px, 0.3f, 1.5f);
    }

    ImGui::Spacing();

    // --- GPU Vertical Tape Roll Canvas (Unique Barcodes per Batch Item + QZ Markers) ---
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("GPUVerticalStripCanvas", ImVec2(avail.x, avail.y - 10), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

        float scale = strip_zoom_;
        float tape_render_w = tape_width_px * scale;
        float tape_render_h = total_strip_h * scale;

        float offset_x = std::max(20.0f, (avail.x - tape_render_w - 30.0f) * 0.5f);
        ImVec2 tape_min = ImVec2(canvas_pos.x + offset_x, canvas_pos.y + 20.0f);
        ImVec2 tape_max = ImVec2(tape_min.x + tape_render_w, tape_min.y + tape_render_h);

        // 1. Draw Continuous Paper Roll
        draw_list->AddRectFilled(tape_min, tape_max, IM_COL32(252, 252, 252, 255), 3.0f);
        draw_list->AddRect(tape_min, tape_max, IM_COL32(110, 110, 110, 255), 3.0f, 0, 1.0f);

        // 2. Draw Printable Margins
        float margin_side_px = ((tape_w_mm - print_w_mm) * 0.5f) * dots_per_mm * scale;
        if (margin_side_px > 1.0f) {
            draw_list->AddLine(ImVec2(tape_min.x + margin_side_px, tape_min.y),
                               ImVec2(tape_min.x + margin_side_px, tape_max.y), IM_COL32(210, 210, 210, 160), 1.0f);
            draw_list->AddLine(ImVec2(tape_max.x - margin_side_px, tape_min.y),
                               ImVec2(tape_max.x - margin_side_px, tape_max.y), IM_COL32(210, 210, 210, 160), 1.0f);
        }

        // 3. Render labels vertically stacked downwards with unique bit patterns per label
        float cur_y = tape_min.y + (lead_px * scale);
        ImFont* text_font = font_barcode_ ? font_barcode_ : ImGui::GetFont();
        float target_font_size = params_.text_size * scale;
        float mod_w = params_.module_width * scale;

        for (size_t l = 0; l < labels_to_render.size(); l++) {
            float label_y0 = cur_y;
            float label_y1 = label_y0 + (label_on_tape_h * scale);

            // Encode unique Code128 bits for this specific label in the batch
            std::string label_bits = engine_.encode_to_bits(labels_to_render[l]);
            if (label_bits.empty()) label_bits = current_encoded_bits_;
            int code_len = (int)label_bits.length();

            if (!strip_settings_.rotate_90) {
                // --- Normal Horizontal Barcode ---
                float label_x0 = tape_min.x + ((tape_render_w - (single_w * scale)) * 0.5f);
                float bar_start_x = label_x0 + (params_.quiet_zone_x * mod_w);
                float bar_y0 = label_y0 + (params_.margin_y * scale);
                float bar_y1 = bar_y0 + (params_.bar_height * scale);

                // Subtle Quiet Zone indicators (soft highlight)
                draw_list->AddRectFilled(ImVec2(label_x0, bar_y0), ImVec2(bar_start_x, bar_y1),
                                         IM_COL32(180, 220, 255, 45));

                // Draw unique bars for this label
                for (int i = 0; i < code_len; i++) {
                    if (label_bits[i] == '1') {
                        float bx0 = bar_start_x + (float)i * mod_w;
                        float bx1 = bx0 + mod_w;
                        draw_list->AddRectFilled(ImVec2(bx0, bar_y0), ImVec2(bx1, bar_y1), IM_COL32(0, 0, 0, 255));
                    }
                }

                // Stop bar
                float stop_x0 = bar_start_x + (float)code_len * mod_w;
                float stop_x1 = stop_x0 + (2.0f * mod_w);
                draw_list->AddRectFilled(ImVec2(stop_x0, bar_y0), ImVec2(stop_x1, bar_y1), IM_COL32(0, 0, 0, 255));

                // Right quiet zone indicator
                float label_right_x = label_x0 + (single_w * scale);
                draw_list->AddRectFilled(ImVec2(stop_x1, bar_y0), ImVec2(label_right_x, bar_y1),
                                         IM_COL32(180, 220, 255, 45));

                // Crisp Text
                ImVec2 text_size = text_font->CalcTextSizeA(target_font_size, FLT_MAX, 0.0f, labels_to_render[l].c_str());
                float text_x = label_x0 + ((single_w * scale - text_size.x) * 0.5f);
                float text_y = bar_y1 + (params_.text_gap_y * scale);

                draw_list->AddText(text_font, target_font_size, ImVec2(text_x, text_y),
                                   IM_COL32(0, 0, 0, 255), labels_to_render[l].c_str());
            } else {
                // --- Rotated 90° Barcode ---
                float label_x0 = tape_min.x + ((tape_render_w - (single_h * scale)) * 0.5f);
                float bar_start_y = label_y0 + (params_.quiet_zone_x * mod_w);
                float bar_x0 = label_x0 + (params_.margin_y * scale);
                float bar_x1 = bar_x0 + (params_.bar_height * scale);

                // Quiet zone highlights
                draw_list->AddRectFilled(ImVec2(bar_x0, label_y0), ImVec2(bar_x1, bar_start_y),
                                         IM_COL32(180, 220, 255, 45));

                for (int i = 0; i < code_len; i++) {
                    if (label_bits[i] == '1') {
                        float by0 = bar_start_y + (float)i * mod_w;
                        float by1 = by0 + mod_w;
                        draw_list->AddRectFilled(ImVec2(bar_x0, by0), ImVec2(bar_x1, by1), IM_COL32(0, 0, 0, 255));
                    }
                }

                // Stop bar
                float stop_y0 = bar_start_y + (float)code_len * mod_w;
                float stop_y1 = stop_y0 + (2.0f * mod_w);
                draw_list->AddRectFilled(ImVec2(bar_x0, stop_y0), ImVec2(bar_x1, stop_y1), IM_COL32(0, 0, 0, 255));

                // Bottom quiet zone highlight
                float label_bottom_y = label_y0 + (label_on_tape_h * scale);
                draw_list->AddRectFilled(ImVec2(bar_x0, stop_y1), ImVec2(bar_x1, label_bottom_y),
                                         IM_COL32(180, 220, 255, 45));

                // Rotated text alongside bars
                float text_x = bar_x1 + (params_.text_gap_y * scale);
                float text_y = label_y0 + (label_on_tape_h * scale * 0.35f);

                draw_list->AddText(text_font, target_font_size, ImVec2(text_x, text_y),
                                   IM_COL32(0, 0, 0, 255), labels_to_render[l].c_str());
            }

            // Horizontal cut guideline between labels
            if (strip_settings_.show_cut_lines && l < labels_to_render.size() - 1) {
                float cut_y = label_y1 + (gap_px * scale * 0.5f);
                for (float cx = tape_min.x; cx < tape_max.x; cx += 10.0f) {
                    draw_list->AddLine(ImVec2(cx, cut_y), ImVec2(std::min(tape_max.x, cx + 5.0f), cut_y),
                                       IM_COL32(200, 40, 40, 220), 1.5f);
                }
            }

            cur_y += (label_on_tape_h + gap_px) * scale;
        }

        ImGui::Dummy(ImVec2(tape_render_w + offset_x * 2, tape_render_h + 40.0f));
    }
    ImGui::EndChild();
}

void App::render_batch_table_tab() {
    if (batch_items_.empty()) {
        ImGui::Text("No hay archivo de lote cargado.");
        return;
    }

    ImGui::Text("Filtrar codigos:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(250);
    ImGui::InputText("##SearchFilter", search_filter_buf_, sizeof(search_filter_buf_));

    std::string filter(search_filter_buf_);
    ImGui::Spacing();

    int total_items = (batch_array_len_limit_ > 0) ? std::min((int)batch_items_.size(), batch_array_len_limit_)
                                                   : (int)batch_items_.size();

    static ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                         ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("BatchItemsTable", 3, table_flags, ImVec2(0, ImGui::GetContentRegionAvail().y - 10))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Codigo", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Accion", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < total_items; i++) {
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
                target_tab_index_ = 0;
                request_tab_switch_ = true;
            }

            ImGui::TableNextColumn();
            std::string btn_id = "Ver##" + std::to_string(i);
            if (ImGui::SmallButton(btn_id.c_str())) {
                selected_batch_index_ = i;
                params_.input = batch_items_[i];
                strncpy(manual_input_buf_, batch_items_[i].c_str(), sizeof(manual_input_buf_) - 1);
                update_barcode_data();
                target_tab_index_ = 0;
                request_tab_switch_ = true;
            }
        }
        ImGui::EndTable();
    }
}

void App::render_print_modal() {
    if (!show_print_dialog_) return;

    ImGui::OpenPopup("Dialogo de Impresion Directa");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 360), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Dialogo de Impresion Directa", &show_print_dialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.0f, 1.0f), "Enviar Trabajo a Brother QL-1110NWB");
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

        static int print_target_mode = 1;
        ImGui::Text("Contenido a Imprimir:");
        ImGui::RadioButton("Codigo Actual (Etiqueta Individual)", &print_target_mode, 0);
        ImGui::RadioButton("Tira Continua Completa (Rollo Brother QL)", &print_target_mode, 1);

        ImGui::Spacing();

        ImGui::Text("Copias:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("##PrintCopies", &print_job_settings_.copies);
        if (print_job_settings_.copies < 1) print_job_settings_.copies = 1;

        ImGui::Checkbox("Ajustar a la pagina (fit-to-page)", &print_job_settings_.fit_to_page);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("[ Enviar a Impresora ]", ImVec2(200, 0))) {
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

    ImGui::OpenPopup("Actualizacion de Code128 Studio");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 240), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Actualizacion de Code128 Studio", &show_update_dialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Actualizador Automatico");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Version instalada: v%s", CODE128_APP_VERSION);

        if (is_checking_update_) {
            ImGui::Text("Conectando con GitHub Releases...");
        } else if (update_info_.update_available) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Nueva version disponible: %s", update_info_.latest_version.c_str());
            ImGui::Spacing();

            if (!is_performing_update_) {
                if (ImGui::Button("[ Iniciar Actualizacion ]", ImVec2(200, 0))) {
                    is_performing_update_ = true;
                    std::string err;
                    bool ok = AppUpdater::PerformHotUpdate(update_info_, [this](float p, const std::string& msg) {
                        update_progress_ = p;
                        update_status_text_ = msg;
                    }, err);

                    if (ok) {
                        status_notification_ = "Actualizacion instalada con exito";
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
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[OK] Ultima version instalada.");
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

// --- Complete ABM de Presets Modal Window ---
void App::render_preset_abm_modal() {
    if (!show_preset_abm_modal_) return;

    ImGui::OpenPopup("ABM de Presets - Configuracion de Etiquetas");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(880, 560), ImVec2(1600, 1200));
    ImGui::SetNextWindowSize(ImVec2(920, 600), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("ABM de Presets - Configuracion de Etiquetas", &show_preset_abm_modal_, ImGuiWindowFlags_None)) {
        ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.0f, 1.0f), "ADMINISTRADOR DE PRESETS (ALTA / BAJA / MODIFICACION)");
        ImGui::Separator();
        ImGui::Spacing();

        if (!abm_status_msg_.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[INFO] %s", abm_status_msg_.c_str());
            ImGui::Spacing();
        }

        float left_col_w = 260.0f;
        float right_col_w = ImGui::GetContentRegionAvail().x - left_col_w - 15.0f;

        // --- Left: Presets List ---
        ImGui::BeginChild("ABMPresetsListCol", ImVec2(left_col_w, ImGui::GetContentRegionAvail().y - 50.0f), true);
        {
            ImGui::Text("Presets Registrados:");
            ImGui::Separator();

            const auto& presets = preset_manager_.get_presets();
            for (size_t i = 0; i < presets.size(); i++) {
                bool is_sel = (abm_selected_preset_idx_ == (int)i);
                std::string badge = presets[i].is_builtin ? "[Fabrica]" : "[Usuario]";
                std::string item_title = badge + " " + presets[i].name;

                if (ImGui::Selectable(item_title.c_str(), is_sel)) {
                    abm_selected_preset_idx_ = (int)i;
                    abm_edit_preset_ = presets[i];
                    strncpy(abm_preset_name_buf_, presets[i].name.c_str(), sizeof(abm_preset_name_buf_) - 1);
                    strncpy(abm_preset_desc_buf_, presets[i].description.c_str(), sizeof(abm_preset_desc_buf_) - 1);
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // --- Right: Interactive Preset Editor ---
        ImGui::BeginChild("ABMPresetDetailCol", ImVec2(right_col_w, ImGui::GetContentRegionAvail().y - 50.0f), true);
        {
            const Preset* current_p = preset_manager_.get_preset(abm_selected_preset_idx_);
            if (current_p) {
                // Ensure abm_edit_preset_ is initialized
                if (abm_edit_preset_.id.empty() || abm_edit_preset_.name.empty()) {
                    abm_edit_preset_ = *current_p;
                    strncpy(abm_preset_name_buf_, current_p->name.c_str(), sizeof(abm_preset_name_buf_) - 1);
                    strncpy(abm_preset_desc_buf_, current_p->description.c_str(), sizeof(abm_preset_desc_buf_) - 1);
                }

                ImGui::Text("Editar Parametros del Preset:");
                ImGui::SameLine();
                if (current_p->is_builtin) {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[Preset Oficial Brother QL-1110NWB]");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Preset Personalizado]");
                }
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Text("Nombre del Preset:");
                if (ImGui::InputText("##ABMName", abm_preset_name_buf_, sizeof(abm_preset_name_buf_))) {
                    abm_edit_preset_.name = abm_preset_name_buf_;
                }

                ImGui::Text("Descripcion / Notas:");
                if (ImGui::InputText("##ABMDesc", abm_preset_desc_buf_, sizeof(abm_preset_desc_buf_))) {
                    abm_edit_preset_.description = abm_preset_desc_buf_;
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.0f, 1.0f), "Dimensiones de la Cinta / Rollo Brother:");
                ImGui::Columns(2, "abm_tape_edit", false);
                ImGui::Text("Ancho Cinta (mm):");
                ImGui::SetNextItemWidth(130);
                if (ImGui::InputFloat("##ABMRollW", &abm_edit_preset_.roll_width_mm, 1.0f, 5.0f, "%.1f mm")) {
                    if (abm_edit_preset_.roll_width_mm < 10.0f) abm_edit_preset_.roll_width_mm = 10.0f;
                    if (abm_edit_preset_.printable_width_mm > abm_edit_preset_.roll_width_mm) {
                        abm_edit_preset_.printable_width_mm = abm_edit_preset_.roll_width_mm - 4.0f;
                    }
                }

                ImGui::Text("Espaciado Etiquetas (mm):");
                ImGui::SetNextItemWidth(130);
                ImGui::InputFloat("##ABMGap", &abm_edit_preset_.label_gap_mm, 0.5f, 2.0f, "%.1f mm");

                ImGui::NextColumn();
                ImGui::Text("Ancho Imprimible (mm):");
                ImGui::SetNextItemWidth(130);
                if (ImGui::InputFloat("##ABMPrintW", &abm_edit_preset_.printable_width_mm, 1.0f, 5.0f, "%.1f mm")) {
                    if (abm_edit_preset_.printable_width_mm > abm_edit_preset_.roll_width_mm) {
                        abm_edit_preset_.printable_width_mm = abm_edit_preset_.roll_width_mm;
                    }
                    if (abm_edit_preset_.printable_width_mm < 5.0f) abm_edit_preset_.printable_width_mm = 5.0f;
                }

                ImGui::Text("Repeticiones en tira:");
                ImGui::SetNextItemWidth(130);
                ImGui::InputInt("##ABMRepeat", &abm_edit_preset_.repeat_count);
                if (abm_edit_preset_.repeat_count < 1) abm_edit_preset_.repeat_count = 1;
                ImGui::Columns(1);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.0f, 1.0f), "Geometria Fraccional del Codigo de Barras:");
                ImGui::Columns(2, "abm_barcode_edit", false);
                ImGui::Text("Ancho Modulo (X-Dim):");
                ImGui::SetNextItemWidth(130);
                ImGui::SliderFloat("##ABMModW", &abm_edit_preset_.module_width, 1.0f, 16.0f, "%.2f px");

                ImGui::Text("Tamano Texto:");
                ImGui::SetNextItemWidth(130);
                ImGui::SliderFloat("##ABMTxtSz", &abm_edit_preset_.text_size, 8.0f, 60.0f, "%.1f px");

                ImGui::Text("Margen Superior/Inf (Y):");
                ImGui::SetNextItemWidth(130);
                ImGui::SliderFloat("##ABMMargY", &abm_edit_preset_.margin_y, 0.0f, 30.0f, "%.1f px");

                ImGui::NextColumn();
                ImGui::Text("Altura de Barras:");
                ImGui::SetNextItemWidth(130);
                ImGui::SliderFloat("##ABMBarH", &abm_edit_preset_.bar_height, 10.0f, 250.0f, "%.1f px");

                ImGui::Text("Quiet Zone (Margen X):");
                ImGui::SetNextItemWidth(130);
                ImGui::SliderFloat("##ABMQZ", &abm_edit_preset_.quiet_zone_x, 2.0f, 30.0f, "%.1f mod");

                ImGui::Text("Espacio Barras-Texto:");
                ImGui::SetNextItemWidth(130);
                ImGui::SliderFloat("##ABMGapY", &abm_edit_preset_.text_gap_y, 0.0f, 30.0f, "%.1f px");
                ImGui::Columns(1);

                ImGui::Spacing();
                ImGui::Checkbox("Rotar 90 deg##ABMRot", &abm_edit_preset_.rotate_90);
                ImGui::SameLine();
                ImGui::Checkbox("Mostrar marcas de corte##ABMCut", &abm_edit_preset_.show_cut_lines);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Actions for selected preset
                ImGui::Columns(3, "abm_actions_col", false);
                if (ImGui::Button("[ Guardar Cambios ]", ImVec2(-1, 0))) {
                    abm_edit_preset_.name = abm_preset_name_buf_;
                    abm_edit_preset_.description = abm_preset_desc_buf_;
                    std::string err;
                    if (preset_manager_.update_preset(abm_selected_preset_idx_, abm_edit_preset_, err)) {
                        abm_status_msg_ = "Preset '" + abm_edit_preset_.name + "' guardado con exito";
                        abm_status_timer_ = 3.0f;
                    } else {
                        abm_status_msg_ = "Error: " + err;
                        abm_status_timer_ = 4.0f;
                    }
                }
                ImGui::NextColumn();
                if (ImGui::Button("[ Copiar del Lienzo ]", ImVec2(-1, 0))) {
                    abm_edit_preset_.module_width = params_.module_width;
                    abm_edit_preset_.bar_height = params_.bar_height;
                    abm_edit_preset_.text_size = params_.text_size;
                    abm_edit_preset_.quiet_zone_x = params_.quiet_zone_x;
                    abm_edit_preset_.margin_y = params_.margin_y;
                    abm_edit_preset_.text_gap_y = params_.text_gap_y;
                    abm_edit_preset_.roll_width_mm = strip_settings_.roll_width_mm;
                    abm_edit_preset_.printable_width_mm = strip_settings_.printable_width_mm;
                    abm_edit_preset_.label_gap_mm = strip_settings_.label_gap_mm;
                    abm_edit_preset_.repeat_count = strip_settings_.repeat_count;
                    abm_edit_preset_.rotate_90 = strip_settings_.rotate_90;
                    abm_edit_preset_.show_cut_lines = strip_settings_.show_cut_lines;
                    abm_status_msg_ = "Parametros del lienzo copiados al editor";
                    abm_status_timer_ = 3.0f;
                }
                ImGui::NextColumn();
                if (ImGui::Button("[ Aplicar a la App ]", ImVec2(-1, 0))) {
                    apply_preset(abm_edit_preset_);
                    selected_preset_idx_ = abm_selected_preset_idx_;
                    abm_status_msg_ = "Preset '" + abm_edit_preset_.name + "' aplicado al lienzo";
                    abm_status_timer_ = 3.0f;
                }
                ImGui::Columns(1);
            } else {
                ImGui::Text("Selecciona o crea un preset en la lista izquierda.");
            }
        }
        ImGui::EndChild();

        // --- Bottom Control Bar: Alta, Duplicar, Baja, Restaurar, Cerrar ---
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("[ + Nuevo Preset ]", ImVec2(130, 0))) {
            Preset new_p;
            new_p.name = (abm_preset_name_buf_[0] != '\0') ? std::string(abm_preset_name_buf_) + " (Nuevo)" : "Nuevo Preset";
            new_p.description = "Configuracion personalizada";
            new_p.module_width = params_.module_width;
            new_p.bar_height = params_.bar_height;
            new_p.text_size = params_.text_size;
            new_p.quiet_zone_x = params_.quiet_zone_x;
            new_p.margin_y = params_.margin_y;
            new_p.text_gap_y = params_.text_gap_y;
            new_p.roll_width_mm = strip_settings_.roll_width_mm;
            new_p.printable_width_mm = strip_settings_.printable_width_mm;
            new_p.label_gap_mm = strip_settings_.label_gap_mm;
            new_p.repeat_count = strip_settings_.repeat_count;
            new_p.rotate_90 = strip_settings_.rotate_90;
            new_p.show_cut_lines = strip_settings_.show_cut_lines;

            std::string err;
            if (preset_manager_.add_preset(new_p, err)) {
                abm_selected_preset_idx_ = (int)preset_manager_.get_presets().size() - 1;
                abm_edit_preset_ = new_p;
                strncpy(abm_preset_name_buf_, new_p.name.c_str(), sizeof(abm_preset_name_buf_) - 1);
                strncpy(abm_preset_desc_buf_, new_p.description.c_str(), sizeof(abm_preset_desc_buf_) - 1);
                abm_status_msg_ = "Nuevo preset '" + new_p.name + "' creado con exito";
                abm_status_timer_ = 3.0f;
            } else {
                abm_status_msg_ = "Error: " + err;
                abm_status_timer_ = 4.0f;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("[ Duplicar ]", ImVec2(100, 0))) {
            std::string err;
            if (preset_manager_.duplicate_preset(abm_selected_preset_idx_, err)) {
                abm_selected_preset_idx_ = (int)preset_manager_.get_presets().size() - 1;
                const Preset* dup_p = preset_manager_.get_preset(abm_selected_preset_idx_);
                if (dup_p) {
                    abm_edit_preset_ = *dup_p;
                    strncpy(abm_preset_name_buf_, dup_p->name.c_str(), sizeof(abm_preset_name_buf_) - 1);
                    strncpy(abm_preset_desc_buf_, dup_p->description.c_str(), sizeof(abm_preset_desc_buf_) - 1);
                }
                abm_status_msg_ = "Preset duplicado con exito";
                abm_status_timer_ = 3.0f;
            } else {
                abm_status_msg_ = "Error: " + err;
                abm_status_timer_ = 4.0f;
            }
        }

        ImGui::SameLine();
        bool can_del = (preset_manager_.get_presets().size() > 1);
        if (!can_del) ImGui::BeginDisabled();
        if (ImGui::Button("[ Eliminar ]", ImVec2(100, 0))) {
            std::string err;
            if (preset_manager_.delete_preset(abm_selected_preset_idx_, err)) {
                if (abm_selected_preset_idx_ > 0) abm_selected_preset_idx_--;
                const Preset* prev_p = preset_manager_.get_preset(abm_selected_preset_idx_);
                if (prev_p) {
                    abm_edit_preset_ = *prev_p;
                    strncpy(abm_preset_name_buf_, prev_p->name.c_str(), sizeof(abm_preset_name_buf_) - 1);
                    strncpy(abm_preset_desc_buf_, prev_p->description.c_str(), sizeof(abm_preset_desc_buf_) - 1);
                }
                abm_status_msg_ = "Preset eliminado con exito";
                abm_status_timer_ = 3.0f;
            } else {
                abm_status_msg_ = "Error: " + err;
                abm_status_timer_ = 4.0f;
            }
        }
        if (!can_del) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("[ Restaurar QL-1110NWB ]", ImVec2(200, 0))) {
            preset_manager_.reset_to_defaults();
            abm_selected_preset_idx_ = 0;
            const Preset* def_p = preset_manager_.get_preset(0);
            if (def_p) {
                abm_edit_preset_ = *def_p;
                strncpy(abm_preset_name_buf_, def_p->name.c_str(), sizeof(abm_preset_name_buf_) - 1);
                strncpy(abm_preset_desc_buf_, def_p->description.c_str(), sizeof(abm_preset_desc_buf_) - 1);
                apply_preset(*def_p);
            }
            abm_status_msg_ = "Preset Brother QL-1110NWB restaurado por defecto";
            abm_status_timer_ = 3.0f;
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
        if (ImGui::Button("Cerrar", ImVec2(95, 0))) {
            show_preset_abm_modal_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
