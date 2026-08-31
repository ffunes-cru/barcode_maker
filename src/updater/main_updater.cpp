#include "updater.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "========================================\n";
    std::cout << " Code128 Studio - Auto Updater CLI\n";
    std::cout << " Current Version: v" << CODE128_APP_VERSION << "\n";
    std::cout << "========================================\n\n";

    std::cout << "Comprobando actualizaciones en GitHub (" << CODE128_GITHUB_REPO << ")...\n";

    UpdateInfo info;
    std::string err;
    if (!AppUpdater::CheckForUpdates(CODE128_GITHUB_REPO, info, err)) {
        std::cerr << "Aviso: " << err << "\n";
        return 1;
    }

    if (!info.update_available) {
        std::cout << "Tu aplicación está actualizada (v" << info.current_version << ").\n";
        return 0;
    }

    std::cout << "¡Nueva versión disponible: " << info.latest_version << "!\n";
    std::cout << "Iniciando descarga y actualización en caliente...\n";

    if (!AppUpdater::PerformHotUpdate(info, [](float p, const std::string& status) {
        std::cout << "[" << (int)(p * 100) << "%] " << status << "\n";
    }, err)) {
        std::cerr << "Error actualizando: " << err << "\n";
        return 1;
    }

    std::cout << "\n¡Actualización completada con éxito a " << info.latest_version << "!\n";
    return 0;
}
