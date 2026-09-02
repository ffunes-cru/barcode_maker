#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../engine/barcode_core.hpp"
#include "../print/print_manager.hpp"
#include "../updater/updater.hpp"
#include "strip_preview.hpp"
#include "preset_manager.hpp"

struct ImFont;

class App {
public:
    App();
    ~App();

    bool init(const std::string& resource_dir = "");
    void set_fonts(ImFont* ui_font, ImFont* barcode_font) {
        font_ui_ = ui_font;
        font_barcode_ = barcode_font;
    }
    void render_ui();

private:
    void render_left_panel();
    void render_right_panel();
    void render_live_preview_tab();
    void render_strip_preview_tab();
    void render_batch_table_tab();
    void render_print_modal();
    void render_update_modal();
    void render_preset_abm_modal();

    void update_barcode_data();
    void load_batch_file(const std::string& filepath);
    void export_batch();
    void export_current_png();
    void export_strip_png();

    // Preset Operations
    void apply_preset(const Preset& p);
    void apply_preset_by_index(size_t index);

    BarcodeEngine engine_;
    PrintManager print_manager_;
    PresetManager preset_manager_;

    // Fonts
    ImFont* font_ui_ = nullptr;
    ImFont* font_barcode_ = nullptr;

    // Barcode & Strip configuration
    BarcodeParams params_;
    StripSettings strip_settings_;
    PrintJobSettings print_job_settings_;

    // Fast GPU state (Bits & Metrics)
    std::string current_encoded_bits_;
    int calculated_width_px_ = 0;
    int calculated_height_px_ = 0;
    bool is_code_valid_ = true;
    std::string code_error_msg_;

    // UI State & Buffers
    enum class InputMode { Manual, BatchFile };
    InputMode input_mode_ = InputMode::Manual;

    char manual_input_buf_[256] = "A0100";
    char batch_file_path_[512] = "input_rep.txt";
    char output_dir_buf_[512] = "output_files_r";
    char search_filter_buf_[128] = "";

    std::vector<std::string> batch_items_;
    int selected_batch_index_ = 0;
    int batch_array_len_limit_ = 0; // 0 = all

    // Tab switching control
    int target_tab_index_ = 0;
    bool request_tab_switch_ = false;

    // Active preset tracking
    int selected_preset_idx_ = 0;

    // Preset ABM Modal State
    bool show_preset_abm_modal_ = false;
    int abm_selected_preset_idx_ = 0;
    Preset abm_edit_preset_;
    char abm_preset_name_buf_[128] = "";
    char abm_preset_desc_buf_[256] = "";
    std::string abm_status_msg_;
    float abm_status_timer_ = 0.0f;

    // Pan & Zoom controls for GPU preview
    float preview_zoom_ = 1.0f;
    float strip_zoom_ = 0.6f;

    // Status banner
    std::string status_notification_;
    float status_notification_timer_ = 0.0f;

    // Modals
    bool show_print_dialog_ = false;
    bool show_update_dialog_ = false;
    UpdateInfo update_info_;
    bool is_checking_update_ = false;
    bool is_performing_update_ = false;
    float update_progress_ = 0.0f;
    std::string update_status_text_;

    // Direct GPU Texture Preview (WYSIWYG 1:1)
    unsigned int single_texture_id_ = 0;
    int single_tex_w_ = 0;
    int single_tex_h_ = 0;

    unsigned int strip_texture_id_ = 0;
    int strip_tex_w_ = 0;
    int strip_tex_h_ = 0;

    struct BatchPreviewItem {
        std::string code;
        unsigned int texture_id = 0;
        int width = 0;
        int height = 0;
    };
    std::vector<BatchPreviewItem> batch_preview_cache_;

    void update_single_texture(const BarcodeImage& img);
    void update_strip_texture();
    void update_batch_preview_cache();
    void clear_batch_preview_cache();

    // Batch Chunk Printing System
    int batch_chunk_size_ = 30;
    bool batch_printing_active_ = false;
    int batch_print_current_index_ = 0;
    int batch_print_total_count_ = 0;
    int batch_print_success_count_ = 0;
    bool show_batch_chunk_dialog_ = false;
    PrintJobSettings current_batch_job_;

    void process_next_batch_chunk();
    void render_batch_chunk_modal();
};
