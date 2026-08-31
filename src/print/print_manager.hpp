#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct PrintJobSettings {
    std::string printer_name = "";
    int copies = 1;
    bool fit_to_page = true;
    int orientation = 0; // 0 = Auto / Normal, 3 = Portrait, 4 = Landscape (90 deg)
    bool cut_each_label = false;
    bool cut_at_end = true;
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
