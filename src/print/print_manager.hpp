#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct PrintJobSettings {
    std::string printer_name = "Brother_QL-1110NWB";
    int copies = 1;
    bool fit_to_page = true; // Default to true for dynamic roll stretch
    int orientation = 3; // 3 = Portrait (-o orientation-requested=3), 4 = Landscape, 0 = system default
    bool cut_each_label = false;
    bool cut_at_end = true;
    int print_method = 0; // 0 = GDI Driver (Largo Dinámico DEVMODE), 1 = Spooler RAW (Brother ESC/P-Raster)
    float roll_width_mm = 103.6f;
    float printable_width_mm = 99.0f;
};

class PrintManager {
public:
    PrintManager();
    ~PrintManager();

    // Refresh list of installed system printers
    void refresh_printers();
    const std::vector<std::string>& get_available_printers() const { return printers_; }
    const std::string& get_default_printer() const { return default_printer_; }

    // Direct print an existing image file (e.g. PNG)
    bool print_file(const std::string& file_path, const PrintJobSettings& settings, std::string& out_message);

    // Direct print an in-memory RGBA buffer by writing temporary file or spooling
    bool print_rgba_buffer(const std::vector<uint8_t>& rgba, int width, int height,
                           const PrintJobSettings& settings, std::string& out_message);

private:
    std::vector<std::string> printers_;
    std::string default_printer_;
};
