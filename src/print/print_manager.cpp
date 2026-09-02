#include "print_manager.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <array>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winspool.h>
#else
#include <unistd.h>
#endif

extern "C" {
#include "../../lib/img/libattopng.h"
}

namespace fs = std::filesystem;

PrintManager::PrintManager() {
    refresh_printers();
}

PrintManager::~PrintManager() = default;

void PrintManager::refresh_printers() {
    printers_.clear();
    default_printer_.clear();

#ifdef _WIN32
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    DWORD bytesNeeded = 0;
    DWORD count = 0;

    EnumPrintersA(flags, NULL, 2, NULL, 0, &bytesNeeded, &count);
    if (bytesNeeded > 0) {
        std::vector<BYTE> buffer(bytesNeeded);
        if (EnumPrintersA(flags, NULL, 2, buffer.data(), bytesNeeded, &bytesNeeded, &count)) {
            PRINTER_INFO_2A* pInfo = reinterpret_cast<PRINTER_INFO_2A*>(buffer.data());
            for (DWORD i = 0; i < count; i++) {
                printers_.push_back(pInfo[i].pPrinterName);
            }
        }
    }

    char default_buf[256] = {0};
    DWORD default_len = sizeof(default_buf);
    if (GetDefaultPrinterA(default_buf, &default_len)) {
        default_printer_ = default_buf;
    }
#else
    // Linux / CUPS via lpstat
    FILE* fp = popen("lpstat -p 2>/dev/null", "r");
    if (fp) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            std::string line(buffer);
            // Format: "printer Brother_QL-1110NWB is idle. enabled since..."
            if (line.rfind("printer ", 0) == 0) {
                size_t start = 8;
                size_t end = line.find(' ', start);
                if (end != std::string::npos) {
                    printers_.push_back(line.substr(start, end - start));
                }
            }
        }
        pclose(fp);
    }

    // Get default printer
    fp = popen("lpstat -d 2>/dev/null", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            std::string line(buffer);
            // Format: "system default destination: Brother_QL-1110NWB"
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string name = line.substr(colon + 1);
                // trim whitespace
                size_t first = name.find_first_not_of(" \t\r\n");
                size_t last = name.find_last_not_of(" \t\r\n");
                if (first != std::string::npos && last != std::string::npos) {
                    default_printer_ = name.substr(first, last - first + 1);
                }
            }
        }
        pclose(fp);
    }
#endif

    // Fallback: If no default printer detected, pick the first one or brother placeholder
    if (default_printer_.empty()) {
        if (!printers_.empty()) {
            default_printer_ = printers_[0];
        } else {
            default_printer_ = "Brother_QL-1110NWB";
        }
    }
    if (printers_.empty()) {
        printers_.push_back(default_printer_);
    }
}

bool PrintManager::print_file(const std::string& file_path, const PrintJobSettings& settings, std::string& out_message) {
    if (!fs::exists(file_path)) {
        out_message = "El archivo no existe: " + file_path;
        return false;
    }

    std::string printer = settings.printer_name.empty() ? default_printer_ : settings.printer_name;

#ifdef _WIN32
    // Windows: ShellExecute with printto action or GDI print
    std::string params = "\"" + printer + "\"";
    HINSTANCE res = ShellExecuteA(NULL, "printto", file_path.c_str(), params.c_str(), NULL, SW_HIDE);
    if ((INT_PTR)res > 32) {
        out_message = "Trabajo enviado a la impresora " + printer;
        return true;
    } else {
        // Fallback to default print action
        res = ShellExecuteA(NULL, "print", file_path.c_str(), NULL, NULL, SW_HIDE);
        if ((INT_PTR)res > 32) {
            out_message = "Trabajo enviado a impresora predeterminada";
            return true;
        }
        out_message = "Error al invocar impresión en Windows (código " + std::to_string((INT_PTR)res) + ")";
        return false;
    }
#else
    // Linux CUPS / lp command
    std::stringstream cmd;
    cmd << "lp -d \"" << printer << "\"";
    if (settings.orientation > 0) {
        cmd << " -o orientation-requested=" << settings.orientation;
    }
    if (settings.copies > 1) {
        cmd << " -n " << settings.copies;
    }

    // Cutter controls
    if (settings.cut_at_end) {
        cmd << " -o BrCutAtEnd=ON";
    } else {
        cmd << " -o BrCutAtEnd=OFF";
    }
    if (settings.cut_each_label) {
        cmd << " -o BrCutLabel=1";
    }

    if (settings.fit_to_page) {
        cmd << " -o fit-to-page";
    }
    cmd << " \"" << file_path << "\" 2>&1";

    std::string cmd_str = cmd.str();
    FILE* pipe = popen(cmd_str.c_str(), "r");
    if (!pipe) {
        out_message = "Error al ejecutar comando de impresión";
        return false;
    }

    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    int ret = pclose(pipe);

    if (ret == 0) {
        out_message = "Trabajo enviado con éxito a '" + printer + "': " + output;
        return true;
    } else {
        out_message = "Error enviando a '" + printer + "' (código " + std::to_string(ret) + "): " + output;
        return false;
    }
#endif
}

#ifdef _WIN32
static bool PrintGdiBuffer(const std::string& printer_name, const std::vector<uint8_t>& rgba, int width, int height,
                           const PrintJobSettings& settings, std::string& out_msg) {
    if (rgba.empty() || width <= 0 || height <= 0) {
        out_msg = "Buffer de imagen inválido";
        return false;
    }

    HANDLE hPrinter = NULL;
    PDEVMODEA pDevMode = NULL;
    std::vector<BYTE> devModeBuf;
    if (OpenPrinterA((LPSTR)printer_name.c_str(), &hPrinter, NULL)) {
        LONG dmSize = DocumentPropertiesA(NULL, hPrinter, (LPSTR)printer_name.c_str(), NULL, NULL, 0);
        if (dmSize > 0) {
            devModeBuf.resize(dmSize);
            pDevMode = (PDEVMODEA)devModeBuf.data();
            if (DocumentPropertiesA(NULL, hPrinter, (LPSTR)printer_name.c_str(), pDevMode, NULL, DM_OUT_BUFFER) == IDOK) {
                pDevMode->dmFields |= DM_COPIES;
                pDevMode->dmCopies = (short)(std::max)(1, settings.copies);
                if (settings.orientation > 0) {
                    pDevMode->dmFields |= DM_ORIENTATION;
                    pDevMode->dmOrientation = (settings.orientation == 4) ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
                }

                // --- SOLUCION PARA EL LARGO DINAMICO EN GDI ---
                // La Brother QL-1110NWB imprime a 300 DPI.
                // Calculamos el largo fisico necesario en base a los pixeles de la imagen (height).
                // dmPaperLength espera el valor expresado en decimas de milimetro (1/10 mm).
                double dpi = 300.0;
                double effective_height_px = (double)height;
                if (settings.fit_to_page && width > 0) {
                    double print_w_mm = settings.printable_width_mm > 0 ? (double)settings.printable_width_mm : 99.0;
                    double target_dots = (print_w_mm / 25.4) * dpi;
                    effective_height_px = ((double)height * target_dots) / (double)width;
                }

                short paperLengthMm10 = (short)std::round((effective_height_px / dpi) * 25.4 * 10.0);
                if (paperLengthMm10 < 127) paperLengthMm10 = 127; // Minimo corte mecanico Brother (~12.7 mm)

                // Forzamos un tamaño de papel personalizado continuo con ANCHO y LARGO
                pDevMode->dmFields |= DM_PAPERSIZE | DM_PAPERWIDTH | DM_PAPERLENGTH;
                pDevMode->dmPaperSize = 0; // DMPAPER_USER: Tamaño continuo personalizado
                pDevMode->dmPaperWidth = (short)std::round((settings.roll_width_mm > 0 ? settings.roll_width_mm : 103.6f) * 10.0f);
                pDevMode->dmPaperLength = paperLengthMm10;

                // MUY IMPORTANTE: Volvemos a llamar a DocumentProperties para que el 
                // driver de Brother valide e integre estos cambios antes de generar el DC
                DocumentPropertiesA(NULL, hPrinter, (LPSTR)printer_name.c_str(), pDevMode, pDevMode, DM_IN_BUFFER | DM_OUT_BUFFER);
            } else {
                pDevMode = NULL;
            }
        }
        ClosePrinter(hPrinter);
    }

    HDC hdc = CreateDCA("WINSPOOL", printer_name.c_str(), NULL, pDevMode);
    if (!hdc) {
        out_msg = "No se pudo abrir la impresora Windows: " + printer_name;
        return false;
    }

    DOCINFOA docInfo;
    ZeroMemory(&docInfo, sizeof(docInfo));
    docInfo.cbSize = sizeof(docInfo);
    docInfo.lpszDocName = "Code128 Studio Print Job";

    if (StartDocA(hdc, &docInfo) <= 0) {
        DeleteDC(hdc);
        out_msg = "Error en StartDocA para " + printer_name;
        return false;
    }

    if (StartPage(hdc) <= 0) {
        AbortDoc(hdc);
        DeleteDC(hdc);
        out_msg = "Error en StartPage para " + printer_name;
        return false;
    }

    int dev_w = GetDeviceCaps(hdc, HORZRES);

    int dest_w = width;
    int dest_h = height;
    int dest_x = 0;
    int dest_y = 0;

    if (settings.fit_to_page && dev_w > 0) {
        dest_w = dev_w;
        dest_h = (int)std::round(((double)height * (double)dev_w) / (double)width);
    }

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down bitmap
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint8_t> bgra(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        bgra[i * 4 + 0] = rgba[i * 4 + 2]; // B
        bgra[i * 4 + 1] = rgba[i * 4 + 1]; // G
        bgra[i * 4 + 2] = rgba[i * 4 + 0]; // R
        bgra[i * 4 + 3] = rgba[i * 4 + 3]; // A
    }

    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);

    int stretch_res = StretchDIBits(
        hdc,
        dest_x, dest_y, dest_w, dest_h,
        0, 0, width, height,
        bgra.data(), &bmi, DIB_RGB_COLORS, SRCCOPY
    );

    EndPage(hdc);
    EndDoc(hdc);
    DeleteDC(hdc);

    if (stretch_res == GDI_ERROR) {
        out_msg = "Error enviando mapa de bits GDI al spooler";
        return false;
    }

    out_msg = "Trabajo impreso correctamente en '" + printer_name + "' (GDI Dinamico)";
    return true;
}

// Spooler RAW Directo para impresoras Brother QL (Comandos ESC/P-Raster nativos)
static bool PrintRawBrotherRaster(const std::string& printer_name, const std::vector<uint8_t>& rgba, int width, int height,
                                  const PrintJobSettings& settings, std::string& out_msg) {
    if (rgba.empty() || width <= 0 || height <= 0) {
        out_msg = "Buffer de imagen inválido";
        return false;
    }

    HANDLE hPrinter = NULL;
    if (!OpenPrinterA((LPSTR)printer_name.c_str(), &hPrinter, NULL)) {
        out_msg = "No se pudo abrir la impresora en modo RAW: " + printer_name;
        return false;
    }

    DOC_INFO_1A docInfo;
    docInfo.pDocName = (LPSTR)"Code128 Raw Brother Raster Job";
    docInfo.pOutputFile = NULL;
    docInfo.pDatatype = (LPSTR)"RAW";

    if (StartDocPrinterA(hPrinter, 1, (LPBYTE)&docInfo) == 0) {
        ClosePrinter(hPrinter);
        out_msg = "Error al iniciar trabajo RAW (StartDocPrinterA)";
        return false;
    }

    if (!StartPagePrinter(hPrinter)) {
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        out_msg = "Error en StartPagePrinter";
        return false;
    }

    std::vector<uint8_t> stream;
    stream.reserve(256 + height * 170);

    // 1. Invalidar estado anterior: 200 bytes de 0x00
    stream.insert(stream.end(), 200, 0x00);

    // 2. ESC @ (Inicializar)
    stream.push_back(0x1B); stream.push_back(0x40);

    // 3. Conmutar a modo Raster: ESC i a 0x01
    stream.push_back(0x1B); stream.push_back(0x69); stream.push_back(0x61); stream.push_back(0x01);

    // 4. Informacion de medio: ESC i z
    uint8_t media_w = (uint8_t)std::clamp((int)std::round(settings.roll_width_mm), 12, 102);
    int bytes_per_line = (media_w > 62) ? 162 : 90;
    int total_pins = bytes_per_line * 8; // 1296 puntos para cabezal 4" o 720 puntos

    // Fit-to-Page: escalar imagen para ocupar el ancho imprimible de la cinta
    int print_w = width;
    int print_h = height;
    double scale = 1.0;

    if (settings.fit_to_page && width > 0) {
        float target_mm = settings.printable_width_mm > 0 ? settings.printable_width_mm : 99.0f;
        int target_dots = (int)std::round((target_mm / 25.4f) * 300.0f);
        if (target_dots > total_pins) target_dots = total_pins;

        scale = (double)target_dots / (double)width;
        print_w = target_dots;
        print_h = (int)std::round((double)height * scale);
    }

    uint32_t raster_lines = (uint32_t)print_h;

    std::vector<uint8_t> stream;
    stream.reserve(256 + print_h * (bytes_per_line + 3));

    // 1. Invalidar estado anterior: 200 bytes de 0x00
    stream.insert(stream.end(), 200, 0x00);

    // 2. ESC @ (Inicializar)
    stream.push_back(0x1B); stream.push_back(0x40);

    // 3. Conmutar a modo Raster: ESC i a 0x01
    stream.push_back(0x1B); stream.push_back(0x69); stream.push_back(0x61); stream.push_back(0x01);

    // 4. Informacion de medio: ESC i z
    stream.push_back(0x1B); stream.push_back(0x69); stream.push_back(0x7A);
    stream.push_back(0x84); // Flags: media type + media width + raster lines validos
    stream.push_back(0x0A); // Tipo de medio: Continuous Tape (Rollo continuo)
    stream.push_back(media_w); // Ancho en mm (102 o 62)
    stream.push_back(0x00); // Largo: 0 (continuo)
    stream.push_back((uint8_t)(raster_lines & 0xFF));
    stream.push_back((uint8_t)((raster_lines >> 8) & 0xFF));
    stream.push_back((uint8_t)((raster_lines >> 16) & 0xFF));
    stream.push_back((uint8_t)((raster_lines >> 24) & 0xFF));
    stream.push_back(0x00); // Numero de pagina
    stream.push_back(0x00); // Reservado

    // 5. Ajuste de cuchilla / corte: ESC i M (Auto-Cut) y ESC i A (Frecuencia de corte)
    // NOTA: Se evita ESC i K 0x08 ya que forzaría un avance y doble corte residual innecesario
    if (settings.cut_at_end) {
        stream.push_back(0x1B); stream.push_back(0x69); stream.push_back(0x4D); stream.push_back(0x40); // Auto-Cut ON
        stream.push_back(0x1B); stream.push_back(0x69); stream.push_back(0x41); stream.push_back(0x01); // Cortar cada etiqueta (1 página)
    } else {
        stream.push_back(0x1B); stream.push_back(0x69); stream.push_back(0x4D); stream.push_back(0x00); // Auto-Cut OFF
    }

    // 6. Margen: ESC i d 0x00 0x00 (margen 0)
    stream.push_back(0x1B); stream.push_back(0x69); stream.push_back(0x64); stream.push_back(0x00); stream.push_back(0x00);

    // Centrado de la imagen en los pines del cabezal
    int offset_pins = 0;
    if (settings.auto_center && print_w < total_pins) {
        offset_pins = (total_pins - print_w) / 2;
    }

    std::vector<uint8_t> line_buf(bytes_per_line, 0x00);

    for (int y = 0; y < print_h; y++) {
        std::fill(line_buf.begin(), line_buf.end(), 0x00);

        int src_y = (int)(y / scale);
        if (src_y >= height) src_y = height - 1;

        for (int x = 0; x < print_w; x++) {
            int target_pin = offset_pins + x;
            if (target_pin < 0 || target_pin >= total_pins) continue;

            // Invertir posición de pin si mirror_x está activado (corrige efecto espejo de cabezal térmico)
            int pin = settings.mirror_x ? (total_pins - 1 - target_pin) : target_pin;

            int src_x = (int)(x / scale);
            if (src_x >= width) src_x = width - 1;

            int idx = (src_y * width + src_x) * 4;
            uint8_t r = rgba[idx + 0];
            uint8_t g = rgba[idx + 1];
            uint8_t b = rgba[idx + 2];
            uint8_t a = rgba[idx + 3];

            bool is_black = (a > 128) && ((r * 299 + g * 587 + b * 114) / 1000 < 128);
            if (is_black) {
                int byte_idx = pin / 8;
                int bit_idx = 7 - (pin % 8);
                if (byte_idx >= 0 && byte_idx < bytes_per_line) {
                    line_buf[byte_idx] |= (1 << bit_idx);
                }
            }
        }

        stream.push_back(0x67);
        stream.push_back(0x00);
        stream.push_back((uint8_t)bytes_per_line);
        stream.insert(stream.end(), line_buf.begin(), line_buf.end());
    }

    // 8. Form Feed / Imprimir y avanzar: 0x0C
    stream.push_back(0x0C);

    DWORD bytesWritten = 0;
    BOOL bSuccess = WritePrinter(hPrinter, stream.data(), (DWORD)stream.size(), &bytesWritten);

    EndPagePrinter(hPrinter);
    EndDocPrinter(hPrinter);
    ClosePrinter(hPrinter);

    if (!bSuccess || bytesWritten == 0) {
        out_msg = "Error al enviar bytes al spooler RAW de Windows";
        return false;
    }

    out_msg = "Trabajo RAW Brother enviado correctamente (" + std::to_string(bytesWritten) + " bytes)";
    return true;
}
#endif

bool PrintManager::print_rgba_buffer(const std::vector<uint8_t>& rgba, int width, int height,
                                    const PrintJobSettings& settings, std::string& out_message) {
    if (rgba.empty() || width <= 0 || height <= 0) {
        out_message = "Buffer de imagen inválido";
        return false;
    }

    std::string printer = settings.printer_name.empty() ? default_printer_ : settings.printer_name;

#ifdef _WIN32
    if (settings.print_method == 1) {
        return PrintRawBrotherRaster(printer, rgba, width, height, settings, out_message);
    }
    return PrintGdiBuffer(printer, rgba, width, height, settings, out_message);
#else
    fs::path temp_path = fs::temp_directory_path() / "code128_print_job.png";

    // Encode to grayscale PNG
    libattopng_t* png = libattopng_new(width, height, PNG_GRAYSCALE);
    if (!png) {
        out_message = "Error creando buffer PNG para impresión";
        return false;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx = (y * width + x) * 4;
            uint8_t r = rgba[idx + 0];
            uint8_t g = rgba[idx + 1];
            uint8_t b = rgba[idx + 2];
            // Convert to grayscale luminance
            uint8_t gray = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
            libattopng_set_pixel(png, x, y, gray);
        }
    }

    libattopng_save(png, temp_path.string().c_str());
    libattopng_destroy(png);

    bool ok = print_file(temp_path.string(), settings, out_message);
    return ok;
#endif
}
