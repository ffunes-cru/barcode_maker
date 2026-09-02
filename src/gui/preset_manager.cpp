#include "preset_manager.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

PresetManager::PresetManager() = default;
PresetManager::~PresetManager() = default;

void PresetManager::init_defaults() {
    presets_.clear();

    // 1. DK-22205 (62mm)
    Preset p1;
    p1.id = "dk_22205_std";
    p1.name = "Brother DK-22205 (62 mm Continuo Estandar)";
    p1.description = "Rollo continuo de 62 mm, ancho imprimible 58 mm";
    p1.is_builtin = true;
    p1.module_width = 5.5f;
    p1.bar_height = 80.0f;
    p1.text_size = 30.0f;
    p1.quiet_zone_x = 10.0f;
    p1.margin_y = 8.0f;
    p1.text_gap_y = 6.0f;
    p1.roll_width_mm = 62.0f;
    p1.printable_width_mm = 58.0f;
    p1.label_gap_mm = 4.0f;
    p1.repeat_count = 12;
    p1.rotate_90 = false;
    p1.show_cut_lines = true;
    presets_.push_back(p1);

    // 2. DK-22246 / DK-22243 (103.6mm QL-1110NWB)
    Preset p2;
    p2.id = "dk_22246_wide";
    p2.name = "Brother DK-22246 / 22243 (103.6 mm - QL-1110NWB 4\" Ancho)";
    p2.description = "Rollo ancho de 4 pulgadas para QL-1110NWB, imprimible 99 mm";
    p2.is_builtin = true;
    p2.module_width = 8.5f;
    p2.bar_height = 110.0f;
    p2.text_size = 38.0f;
    p2.quiet_zone_x = 12.0f;
    p2.margin_y = 10.0f;
    p2.text_gap_y = 8.0f;
    p2.roll_width_mm = 103.6f;
    p2.printable_width_mm = 99.0f;
    p2.label_gap_mm = 4.0f;
    p2.repeat_count = 10;
    p2.rotate_90 = false;
    p2.show_cut_lines = true;
    presets_.push_back(p2);

    // 3. DK-22210 (29mm)
    Preset p3;
    p3.id = "dk_22210_narrow";
    p3.name = "Brother DK-22210 (29 mm Continuo Estrecho)";
    p3.description = "Rollo estrecho continuo de 29 mm, imprimible 26 mm";
    p3.is_builtin = true;
    p3.module_width = 3.0f;
    p3.bar_height = 50.0f;
    p3.text_size = 22.0f;
    p3.quiet_zone_x = 8.0f;
    p3.margin_y = 6.0f;
    p3.text_gap_y = 4.0f;
    p3.roll_width_mm = 29.0f;
    p3.printable_width_mm = 26.0f;
    p3.label_gap_mm = 3.0f;
    p3.repeat_count = 15;
    p3.rotate_90 = false;
    p3.show_cut_lines = true;
    presets_.push_back(p3);

    // 4. DK-22210 Rotado 90°
    Preset p4;
    p4.id = "dk_22210_rot90";
    p4.name = "Brother DK-22210 Rotado 90 deg (29 mm Vertical)";
    p4.description = "Rollo de 29 mm con codigo rotado a lo largo de la cinta";
    p4.is_builtin = true;
    p4.module_width = 5.0f;
    p4.bar_height = 180.0f;
    p4.text_size = 28.0f;
    p4.quiet_zone_x = 10.0f;
    p4.margin_y = 6.0f;
    p4.text_gap_y = 6.0f;
    p4.roll_width_mm = 29.0f;
    p4.printable_width_mm = 26.0f;
    p4.label_gap_mm = 4.0f;
    p4.repeat_count = 10;
    p4.rotate_90 = true;
    p4.show_cut_lines = true;
    presets_.push_back(p4);

    // 5. DK-22223 (50mm)
    Preset p5;
    p5.id = "dk_22223_mid";
    p5.name = "Brother DK-22223 (50 mm Continuo)";
    p5.description = "Rollo continuo de 50 mm, imprimible 46 mm";
    p5.is_builtin = true;
    p5.module_width = 4.8f;
    p5.bar_height = 70.0f;
    p5.text_size = 28.0f;
    p5.quiet_zone_x = 10.0f;
    p5.margin_y = 8.0f;
    p5.text_gap_y = 6.0f;
    p5.roll_width_mm = 50.0f;
    p5.printable_width_mm = 46.0f;
    p5.label_gap_mm = 4.0f;
    p5.repeat_count = 12;
    p5.rotate_90 = false;
    p5.show_cut_lines = true;
    presets_.push_back(p5);

    // 6. DK-11241 (102x152mm)
    Preset p6;
    p6.id = "dk_11241_shipping";
    p6.name = "Brother DK-11241 (102 x 152 mm Envios QL-1110NWB)";
    p6.description = "Etiqueta precortada de envios de 4x6 pulgadas";
    p6.is_builtin = true;
    p6.module_width = 8.0f;
    p6.bar_height = 120.0f;
    p6.text_size = 40.0f;
    p6.quiet_zone_x = 12.0f;
    p6.margin_y = 12.0f;
    p6.text_gap_y = 8.0f;
    p6.roll_width_mm = 102.0f;
    p6.printable_width_mm = 98.0f;
    p6.label_gap_mm = 6.0f;
    p6.repeat_count = 6;
    p6.rotate_90 = false;
    p6.show_cut_lines = true;
    presets_.push_back(p6);
}

bool PresetManager::init(const std::string& base_dir) {
    if (!base_dir.empty()) {
        filepath_ = (fs::path(base_dir) / "presets.json").string();
    } else {
        filepath_ = "presets.json";
    }

    if (fs::exists(filepath_)) {
        if (!load_from_file()) {
            init_defaults();
            save_to_file();
        }
    } else {
        init_defaults();
        save_to_file();
    }

    return !presets_.empty();
}

const Preset* PresetManager::get_preset_by_id(const std::string& id) const {
    for (const auto& p : presets_) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

const Preset* PresetManager::get_preset(size_t index) const {
    if (index < presets_.size()) return &presets_[index];
    return nullptr;
}

int PresetManager::get_preset_index(const std::string& id) const {
    for (size_t i = 0; i < presets_.size(); i++) {
        if (presets_[i].id == id) return (int)i;
    }
    return -1;
}

bool PresetManager::add_preset(const Preset& preset, std::string& out_error) {
    if (preset.name.empty()) {
        out_error = "El nombre del preset no puede estar vacio";
        return false;
    }

    for (const auto& p : presets_) {
        if (p.name == preset.name) {
            out_error = "Ya existe un preset con el nombre '" + preset.name + "'";
            return false;
        }
    }

    Preset new_p = preset;
    if (new_p.id.empty()) {
        new_p.id = "user_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
    new_p.is_builtin = false;
    presets_.push_back(new_p);
    save_to_file();
    return true;
}

bool PresetManager::update_preset(size_t index, const Preset& updated, std::string& out_error) {
    if (index >= presets_.size()) {
        out_error = "Indice de preset invalido";
        return false;
    }

    if (updated.name.empty()) {
        out_error = "El nombre no puede estar vacio";
        return false;
    }

    for (size_t i = 0; i < presets_.size(); i++) {
        if (i != index && presets_[i].name == updated.name) {
            out_error = "Ya existe otro preset llamado '" + updated.name + "'";
            return false;
        }
    }

    bool was_builtin = presets_[index].is_builtin;
    std::string orig_id = presets_[index].id;

    presets_[index] = updated;
    presets_[index].id = orig_id;
    presets_[index].is_builtin = was_builtin;

    save_to_file();
    return true;
}

bool PresetManager::delete_preset(size_t index, std::string& out_error) {
    if (index >= presets_.size()) {
        out_error = "Indice invalido";
        return false;
    }

    if (presets_[index].is_builtin) {
        out_error = "No se pueden eliminar los presets predeterminados del sistema";
        return false;
    }

    presets_.erase(presets_.begin() + index);
    save_to_file();
    return true;
}

bool PresetManager::duplicate_preset(size_t index, std::string& out_error) {
    if (index >= presets_.size()) {
        out_error = "Indice invalido";
        return false;
    }

    Preset dup = presets_[index];
    dup.id = "user_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    dup.name = dup.name + " (Copia)";
    dup.is_builtin = false;

    return add_preset(dup, out_error);
}

void PresetManager::reset_to_defaults() {
    init_defaults();
    save_to_file();
}

bool PresetManager::save_to_file() {
    std::ofstream f(filepath_);
    if (!f.is_open()) return false;

    f << "[\n";
    for (size_t i = 0; i < presets_.size(); i++) {
        const auto& p = presets_[i];
        f << "  {\n";
        f << "    \"id\": \"" << p.id << "\",\n";
        f << "    \"name\": \"" << p.name << "\",\n";
        f << "    \"description\": \"" << p.description << "\",\n";
        f << "    \"is_builtin\": " << (p.is_builtin ? "true" : "false") << ",\n";
        f << "    \"module_width\": " << p.module_width << ",\n";
        f << "    \"bar_height\": " << p.bar_height << ",\n";
        f << "    \"text_size\": " << p.text_size << ",\n";
        f << "    \"quiet_zone_x\": " << p.quiet_zone_x << ",\n";
        f << "    \"margin_y\": " << p.margin_y << ",\n";
        f << "    \"text_gap_y\": " << p.text_gap_y << ",\n";
        f << "    \"roll_width_mm\": " << p.roll_width_mm << ",\n";
        f << "    \"printable_width_mm\": " << p.printable_width_mm << ",\n";
        f << "    \"label_gap_mm\": " << p.label_gap_mm << ",\n";
        f << "    \"repeat_count\": " << p.repeat_count << ",\n";
        f << "    \"rotate_90\": " << (p.rotate_90 ? "true" : "false") << ",\n";
        f << "    \"show_cut_lines\": " << (p.show_cut_lines ? "true" : "false") << "\n";
        f << "  }" << (i + 1 < presets_.size() ? "," : "") << "\n";
    }
    f << "]\n";
    return true;
}

bool PresetManager::load_from_file() {
    std::ifstream f(filepath_);
    if (!f.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (content.empty()) return false;

    // Lightweight robust JSON parser for the preset array
    std::vector<Preset> loaded;
    size_t pos = 0;

    auto extract_string = [&](const std::string& block, const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\":";
        size_t kp = block.find(pattern);
        if (kp == std::string::npos) return "";
        size_t start = block.find('"', kp + pattern.length());
        if (start == std::string::npos) return "";
        size_t end = block.find('"', start + 1);
        if (end == std::string::npos) return "";
        return block.substr(start + 1, end - start - 1);
    };

    auto extract_float = [&](const std::string& block, const std::string& key, float default_val) -> float {
        std::string pattern = "\"" + key + "\":";
        size_t kp = block.find(pattern);
        if (kp == std::string::npos) return default_val;
        size_t start = block.find_first_of("0123456789.-", kp + pattern.length());
        if (start == std::string::npos) return default_val;
        size_t end = block.find_first_of(", \r\n}", start);
        try {
            return std::stof(block.substr(start, end - start));
        } catch (...) {
            return default_val;
        }
    };

    auto extract_bool = [&](const std::string& block, const std::string& key, bool default_val) -> bool {
        std::string pattern = "\"" + key + "\":";
        size_t kp = block.find(pattern);
        if (kp == std::string::npos) return default_val;
        size_t start = block.find_first_not_of(" \t\r\n", kp + pattern.length());
        if (start == std::string::npos) return default_val;
        if (block.substr(start, 4) == "true") return true;
        if (block.substr(start, 5) == "false") return false;
        return default_val;
    };

    while ((pos = content.find('{', pos)) != std::string::npos) {
        size_t end = content.find('}', pos);
        if (end == std::string::npos) break;

        std::string block = content.substr(pos, end - pos + 1);
        Preset p;
        p.id = extract_string(block, "id");
        p.name = extract_string(block, "name");
        p.description = extract_string(block, "description");
        p.is_builtin = extract_bool(block, "is_builtin", false);

        p.module_width = extract_float(block, "module_width", 5.5f);
        p.bar_height = extract_float(block, "bar_height", 80.0f);
        p.text_size = extract_float(block, "text_size", 30.0f);
        p.quiet_zone_x = extract_float(block, "quiet_zone_x", 10.0f);
        p.margin_y = extract_float(block, "margin_y", 8.0f);
        p.text_gap_y = extract_float(block, "text_gap_y", 6.0f);

        p.roll_width_mm = extract_float(block, "roll_width_mm", 62.0f);
        p.printable_width_mm = extract_float(block, "printable_width_mm", 58.0f);
        p.label_gap_mm = extract_float(block, "label_gap_mm", 4.0f);
        p.repeat_count = (int)extract_float(block, "repeat_count", 12.0f);
        p.rotate_90 = extract_bool(block, "rotate_90", false);
        p.show_cut_lines = extract_bool(block, "show_cut_lines", true);

        if (!p.name.empty()) {
            loaded.push_back(p);
        }
        pos = end + 1;
    }

    if (!loaded.empty()) {
        presets_ = std::move(loaded);
        return true;
    }
    return false;
}
