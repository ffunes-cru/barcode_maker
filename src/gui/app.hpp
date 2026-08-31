#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../engine/barcode_core.hpp"
#include "../print/print_manager.hpp"
#include "strip_preview.hpp"

// Forward declaration for OpenGL texture type
typedef unsigned int GLuint;

class App {
public:
    App();
    ~App();

    bool init(const std::string& resource_dir = "");
    void render_ui();

private:
    void render_left_panel();
    void render_right_panel();
    void render_live_preview_tab();
    void render_strip_preview_tab();
    void render_batch_table_tab();
    void render_print_modal();

    void update_single_texture();
    void update_strip_texture();
    void load_batch_file(const std::string& filepath);
    void export_batch();
    void export_current_png();
    void export_strip_png();
    void apply_brother_preset();

    BarcodeEngine engine_;
    PrintManager print_manager_;

    // Barcode & Strip configuration
    BarcodeParams params_;
    StripSettings strip_settings_;
    PrintJobSettings print_job_settings_;

    // OpenGL Textures for 60fps GPU preview
    GLuint single_texture_ = 0;
    BarcodeImage current_single_img_;
    bool single_dirty_ = true;

    GLuint strip_texture_ = 0;
    StripImage current_strip_img_;
    bool strip_dirty_ = true;

    // UI State & Buffers
    enum class InputMode { Manual, BatchFile };
    InputMode input_mode_ = InputMode::Manual;

    char manual_input_buf_[256] = "A0101";
    char batch_file_path_[512] = "input_rep.txt";
    char output_dir_buf_[512] = "output_files_r";
    char search_filter_buf_[128] = "";

    std::vector<std::string> batch_items_;
    int selected_batch_index_ = 0;
    int batch_array_len_limit_ = 0; // 0 = all

    // Batch Export progress
    bool is_exporting_batch_ = false;
    int export_progress_ = 0;
    int export_total_ = 0;
    std::string status_notification_;
    float status_notification_timer_ = 0.0f;

    // Modals & Dialogs
    bool show_print_dialog_ = false;
    std::string last_print_result_;
    bool last_print_success_ = true;
};
