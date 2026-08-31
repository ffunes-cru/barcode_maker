#pragma once

#include <string>
#include <functional>

#define CODE128_APP_VERSION "1.1.0"
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
    // Checks GitHub API for the latest release
    static bool CheckForUpdates(const std::string& repo, UpdateInfo& out_info, std::string& out_err);

    // Downloads and applies the update in-place (hot update)
    static bool PerformHotUpdate(const UpdateInfo& info,
                                 std::function<void(float progress, const std::string& status)> progress_cb,
                                 std::string& out_err);

    // Helper to restart the application after update
    static void RestartApplication(const std::string& exe_path);
};
