#pragma once

#include <string>
#include <functional>

#ifndef CODE128_APP_VERSION
#define CODE128_APP_VERSION "0.5.8"
#endif
#define CODE128_GITHUB_REPO "ffunes-cru/barcode_maker"

struct UpdateInfo {
    bool update_available = false;
    std::string current_version = CODE128_APP_VERSION;
    std::string latest_version;
    std::string release_notes;
    std::string download_url;
    std::string asset_name;
};

class AppUpdater {
public:
    static bool CheckForUpdates(const std::string& repo, UpdateInfo& out_info, std::string& out_err);
    static bool PerformHotUpdate(const UpdateInfo& info,
                                 std::function<void(float progress, const std::string& status)> progress_cb,
                                 std::string& out_err);
    static void RestartApplication(const std::string& exe_path);
};
