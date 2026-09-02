#include "updater.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>
#include <stdio.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

static std::string ExecCommand(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

static std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";

    pos += needle.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        pos++;
    }
    if (pos < json.length() && json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end != std::string::npos) {
            return json.substr(pos, end - pos);
        }
    }
    return "";
}

static std::string FindAssetUrl(const std::string& json, const std::string& keyword, const std::string& ext) {
    size_t search_pos = 0;
    while (true) {
        size_t url_pos = json.find("\"browser_download_url\":", search_pos);
        if (url_pos == std::string::npos) break;

        url_pos += 23;
        while (url_pos < json.length() && (json[url_pos] == ' ' || json[url_pos] == '"')) url_pos++;
        size_t end_quote = json.find('"', url_pos);
        if (end_quote == std::string::npos) break;

        std::string found_url = json.substr(url_pos, end_quote - url_pos);
        if (found_url.find(keyword) != std::string::npos && found_url.find(ext) != std::string::npos) {
            return found_url;
        }
        search_pos = end_quote + 1;
    }
    return "";
}

bool AppUpdater::CheckForUpdates(const std::string& repo, UpdateInfo& out_info, std::string& out_err) {
    out_info.current_version = CODE128_APP_VERSION;
    out_info.update_available = false;

    std::string url = "https://api.github.com/repos/" + repo + "/releases/latest";
    std::string cmd = "curl -s -H \"User-Agent: Code128Studio-Updater\" \"" + url + "\" 2>&1";

    std::string json = ExecCommand(cmd);
    if (json.empty() || json.find("\"tag_name\"") == std::string::npos) {
        out_err = "No se pudo conectar con GitHub Releases o el repositorio no tiene releases públicos.";
        return false;
    }

    std::string tag = ExtractJsonString(json, "tag_name");
    std::string clean_tag = tag;
    if (!clean_tag.empty() && (clean_tag[0] == 'v' || clean_tag[0] == 'V')) {
        clean_tag = clean_tag.substr(1);
    }

    out_info.latest_version = tag;
    out_info.release_notes = ExtractJsonString(json, "body");

    // Match platform asset
#ifdef _WIN32
    std::string asset_keyword = "windows";
    std::string ext = ".zip";
#else
    std::string asset_keyword = "linux";
    std::string ext = ".tar.gz";
#endif

    out_info.download_url = FindAssetUrl(json, asset_keyword, ext);

    // Compare versions
    if (!clean_tag.empty() && clean_tag != CODE128_APP_VERSION) {
        out_info.update_available = true;
    }

    return true;
}

bool AppUpdater::PerformHotUpdate(const UpdateInfo& info,
                                 std::function<void(float, const std::string&)> progress_cb,
                                 std::string& out_err) {
    if (info.download_url.empty()) {
        out_err = "URL de descarga no encontrada para esta plataforma.";
        return false;
    }

    // 1. Get destination directory of the application
#ifdef _WIN32
    char exe_buf[MAX_PATH];
    GetModuleFileNameA(NULL, exe_buf, MAX_PATH);
    fs::path running_exe(exe_buf);
    fs::path app_dir = running_exe.parent_path();
#else
    char exe_buf[1024];
    ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
    if (len <= 0) {
        out_err = "No se pudo determinar la ruta del ejecutable actual.";
        return false;
    }
    exe_buf[len] = '\0';
    fs::path running_exe(exe_buf);
    fs::path app_dir = running_exe.parent_path();
#endif

    // Clean up any leftover .old files from a previous update
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(app_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".old") {
            fs::remove(entry.path(), ec);
        }
    }

    fs::path temp_dir = fs::temp_directory_path() / "code128_update";
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

#ifdef _WIN32
    fs::path download_target = temp_dir / "update_package.zip";
#else
    fs::path download_target = temp_dir / "update_package.tar.gz";
#endif

    if (progress_cb) progress_cb(0.2f, "Descargando actualización desde GitHub...");

    std::string dl_cmd = "curl -L -s -o \"" + download_target.string() + "\" \"" + info.download_url + "\"";
    int ret = std::system(dl_cmd.c_str());
    if (ret != 0 || !fs::exists(download_target) || fs::file_size(download_target) < 1000) {
        out_err = "Error al descargar el paquete de actualización desde GitHub.";
        return false;
    }

    if (progress_cb) progress_cb(0.6f, "Extrayendo archivos de la nueva versión...");

#ifdef _WIN32
    std::string extract_cmd = "tar -xf \"" + download_target.string() + "\" -C \"" + temp_dir.string() + "\"";
    int sys_ret = std::system(extract_cmd.c_str());
    if (sys_ret != 0) {
        std::string ps_cmd = "powershell -NoProfile -Command \"Expand-Archive -Force -Path '" + 
                             download_target.string() + "' -DestinationPath '" + temp_dir.string() + "'\"";
        std::system(ps_cmd.c_str());
    }
#else
    std::string extract_cmd = "tar -xzf \"" + download_target.string() + "\" -C \"" + temp_dir.string() + "\" 2>&1";
    int sys_ret = std::system(extract_cmd.c_str());
    (void)sys_ret;
#endif

    // Locate the folder containing the extracted binaries
    fs::path source_dir = temp_dir;
    std::string target_binary_name = 
#ifdef _WIN32
        "code128_studio.exe";
#else
        "code128_studio";
#endif

    bool found = false;
    if (fs::exists(source_dir / target_binary_name)) {
        found = true;
    } else {
        for (const auto& entry : fs::recursive_directory_iterator(temp_dir, ec)) {
            if (entry.is_regular_file() && entry.path().filename().string() == target_binary_name) {
                source_dir = entry.path().parent_path();
                found = true;
                break;
            }
        }
    }

    if (!found) {
        out_err = "No se encontró el ejecutable principal en el paquete descargado.";
        return false;
    }

    if (progress_cb) progress_cb(0.85f, "Instalando archivos en caliente...");

    // Copy all files from source_dir to app_dir
    int updated_files = 0;
    for (const auto& entry : fs::directory_iterator(source_dir, ec)) {
        if (!entry.is_regular_file()) continue;

        fs::path file_name = entry.path().filename();
        fs::path dest_file = app_dir / file_name;

#ifdef _WIN32
        // On Windows, if dest_file is currently running/locked:
        if (fs::exists(dest_file, ec)) {
            fs::path backup_file = app_dir / (file_name.string() + ".old");
            MoveFileExA(dest_file.string().c_str(), backup_file.string().c_str(), MOVEFILE_REPLACE_EXISTING);
        }

        if (CopyFileA(entry.path().string().c_str(), dest_file.string().c_str(), FALSE)) {
            updated_files++;
        }
#else
        fs::copy_file(entry.path(), dest_file, fs::copy_options::overwrite_existing, ec);
        chmod(dest_file.c_str(), 0755);
        updated_files++;
#endif
    }

    // Clean up temporary download directory
    fs::remove_all(temp_dir, ec);

    if (progress_cb) progress_cb(1.0f, "¡Actualización completada exitosamente!");
    return true;
}

void AppUpdater::RestartApplication(const std::string& exe_path) {
#ifdef _WIN32
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    CreateProcessA(exe_path.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    std::exit(0);
#else
    char* args[] = { (char*)exe_path.c_str(), nullptr };
    execv(exe_path.c_str(), args);
    std::exit(0);
#endif
}
