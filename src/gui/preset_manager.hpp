#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../engine/barcode_core.hpp"
#include "strip_preview.hpp"

struct Preset {
    std::string id;
    std::string name;
    std::string description;
    bool is_builtin = false;

    // Barcode Geometry
    float module_width = 5.5f;
    float bar_height = 80.0f;
    float text_size = 30.0f;
    float quiet_zone_x = 10.0f;
    float margin_y = 8.0f;
    float margin_bottom = 8.0f;
    float text_gap_y = 6.0f;

    // Brother QL Roll & Strip Settings
    float roll_width_mm = 62.0f;
    float printable_width_mm = 58.0f;
    float label_gap_mm = 4.0f;
    int repeat_count = 12;
    bool rotate_90 = false;
    bool show_cut_lines = true;
    int cut_line_style = 0;
};

class PresetManager {
public:
    PresetManager();
    ~PresetManager();

    bool init(const std::string& base_dir = "");

    // ABM / CRUD Methods
    const std::vector<Preset>& get_presets() const { return presets_; }
    const Preset* get_preset_by_id(const std::string& id) const;
    const Preset* get_preset(size_t index) const;
    int get_preset_index(const std::string& id) const;

    // Alta (Create)
    bool add_preset(const Preset& preset, std::string& out_error);

    // Modificación (Update)
    bool update_preset(size_t index, const Preset& updated_preset, std::string& out_error);

    // Baja (Delete)
    bool delete_preset(size_t index, std::string& out_error);

    // Duplicar (Duplicate)
    bool duplicate_preset(size_t index, std::string& out_error);

    // Restaurar valores predeterminados
    void reset_to_defaults();

    // Persistencia en disco
    bool save_to_file();
    bool load_from_file();

private:
    void init_defaults();

    std::vector<Preset> presets_;
    std::string filepath_;
};
