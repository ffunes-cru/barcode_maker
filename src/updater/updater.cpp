#include "updater.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>

#ifdef _WIN32
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

    // Search for asset URL matching platform
    size_t asset_pos = json.find(asset_keyword);
    if (asset_pos != std::string::npos) {
        size_t url_key = json.rfind("\"browser_download_url\":", asset_pos + 200);
        if (url_key != std::string::npos) {
            out_info.download_url = ExtractJsonString(json.substr(url_key), "browser_download_url");
        }
    }

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

    fs::path temp_dir = fs::temp_directory_path() / "code128_update";
    fs::create_directories(temp_dir);

    fs::path download_target = temp_dir / "update_package";
    if (progress_cb) progress_cb(0.2f, "Descargando actualización desde GitHub...");

    std::string dl_cmd = "curl -L -s -o \"" + download_target.string() + "\" \"" + info.download_url + "\"";
    int ret = std::system(dl_cmd.c_str());
    if (ret != 0 || !fs::exists(download_target)) {
        out_err = "Error al descargar el paquete de actualización.";
        return false;
    }

    if (progress_cb) progress_cb(0.7f, "Extrayendo y reemplazando archivos...");

#ifdef _WIN32
    // Windows: Extract zip with powershell or tar
    std::string extract_cmd = "tar -xf \"" + download_target.string() + "\" -C \"" + temp_dir.string() + "\"";
    int sys_ret = std::system(extract_cmd.c_str());
    (void)sys_ret;

    // Replace current exe
    char exe_buf[MAX_PATH];
    GetModuleFileNameA(NULL, exe_buf, MAX_PATH);
    std::string current_exe = exe_buf;
    std::string backup_exe = current_exe + ".old";

    MoveFileExA(current_exe.c_str(), backup_exe.c_str(), MOVEFILE_REPLACE_EXISTING);

    fs::path new_exe = temp_dir / "code128_studio.exe";
    if (fs::exists(new_exe)) {
        CopyFileA(new_exe.string().c_str(), current_exe.c_str(), FALSE);
    }
#else
    // Linux: Extract tar.gz and replace current binary
    std::string extract_cmd = "tar -xzf \"" + download_target.string() + "\" -C \"" + temp_dir.string() + "\" 2>&1";
    int sys_ret = std::system(extract_cmd.c_str());
    (void)sys_ret;

    char exe_buf[1024];
    ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
    if (len > 0) {
        exe_buf[len] = '\0';
        std::string current_exe = exe_buf;
        fs::path new_bin = temp_dir / "code128_studio";
        if (fs::exists(new_bin)) {
            fs::copy_file(new_bin, current_exe, fs::copy_options::overwrite_existing);
            chmod(current_exe.c_str(), 0755);
        }
    }
#endif

    if (progress_cb) progress_cb(1.0f, "¡Actualización completada!");
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
