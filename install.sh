#!/usr/bin/env bash
# ==============================================================================
# Code128 Studio - Instalador para Linux (Entorno de Usuario ~/.local)
# ==============================================================================
set -e

APP_NAME="Code128 Studio"
APP_ID="code128_studio"
INSTALL_DIR="${HOME}/.local/share/${APP_ID}"
BIN_DIR="${HOME}/.local/bin"
DESKTOP_DIR="${HOME}/.local/share/applications"
ICON_DIR="${HOME}/.local/share/icons/hicolor/256x256/apps"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- Modo Desinstalación ---
if [[ "$1" == "--uninstall" || "$1" == "-u" ]]; then
    echo "Desinstalando ${APP_NAME}..."
    rm -rf "${INSTALL_DIR}"
    rm -f "${BIN_DIR}/code128_studio"
    rm -f "${BIN_DIR}/code128_updater"
    rm -f "${DESKTOP_DIR}/${APP_ID}.desktop"
    rm -f "${ICON_DIR}/${APP_ID}.png"
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${DESKTOP_DIR}" 2>/dev/null || true
    fi
    echo "¡${APP_NAME} ha sido desinstalado correctamente!"
    exit 0
fi

echo "========================================================"
echo "    Instalador de ${APP_NAME} para Linux"
echo "========================================================"
echo "Destino de datos: ${INSTALL_DIR}"
echo "Destino binario: ${BIN_DIR}/code128_studio"
echo ""

# 1. Localizar binarios compilados
STUDIO_BIN=""
UPDATER_BIN=""

if [[ -f "${SCRIPT_DIR}/build/code128_studio" ]]; then
    STUDIO_BIN="${SCRIPT_DIR}/build/code128_studio"
elif [[ -f "${SCRIPT_DIR}/code128_studio" ]]; then
    STUDIO_BIN="${SCRIPT_DIR}/code128_studio"
fi

if [[ -f "${SCRIPT_DIR}/build/code128_updater" ]]; then
    UPDATER_BIN="${SCRIPT_DIR}/build/code128_updater"
elif [[ -f "${SCRIPT_DIR}/code128_updater" ]]; then
    UPDATER_BIN="${SCRIPT_DIR}/code128_updater"
fi

if [[ -z "${STUDIO_BIN}" ]]; then
    echo "Compilando aplicación antes de instalar..."
    mkdir -p "${SCRIPT_DIR}/build"
    cd "${SCRIPT_DIR}/build"
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    cd "${SCRIPT_DIR}"
    STUDIO_BIN="${SCRIPT_DIR}/build/code128_studio"
    UPDATER_BIN="${SCRIPT_DIR}/build/code128_updater"
fi

# 2. Crear directorios
mkdir -p "${INSTALL_DIR}"
mkdir -p "${BIN_DIR}"
mkdir -p "${DESKTOP_DIR}"
mkdir -p "${ICON_DIR}"

# 3. Copiar binarios y recursos
echo "[1/4] Copiando binarios..."
cp -f "${STUDIO_BIN}" "${INSTALL_DIR}/code128_studio"
chmod +x "${INSTALL_DIR}/code128_studio"

if [[ -f "${UPDATER_BIN}" ]]; then
    cp -f "${UPDATER_BIN}" "${INSTALL_DIR}/code128_updater"
    chmod +x "${INSTALL_DIR}/code128_updater"
fi

echo "[2/4] Copiando recursos del motor y presets..."
for res in font.otf code128char.txt code128int.txt input_rep.txt presets.json; do
    if [[ -f "${SCRIPT_DIR}/${res}" ]]; then
        cp -f "${SCRIPT_DIR}/${res}" "${INSTALL_DIR}/${res}"
    fi
done

if [[ -f "${SCRIPT_DIR}/resources/icon.png" ]]; then
    cp -f "${SCRIPT_DIR}/resources/icon.png" "${ICON_DIR}/${APP_ID}.png"
    cp -f "${SCRIPT_DIR}/resources/icon.png" "${INSTALL_DIR}/icon.png"
fi

echo "[3/4] Creando accesos directos y comandos..."
cat << 'EOF' > "${BIN_DIR}/code128_studio"
#!/usr/bin/env bash
INSTALL_PATH="${HOME}/.local/share/code128_studio"
cd "${INSTALL_PATH}"
exec "${INSTALL_PATH}/code128_studio" "$@"
EOF
chmod +x "${BIN_DIR}/code128_studio"

if [[ -f "${INSTALL_DIR}/code128_updater" ]]; then
cat << 'EOF' > "${BIN_DIR}/code128_updater"
#!/usr/bin/env bash
INSTALL_PATH="${HOME}/.local/share/code128_studio"
cd "${INSTALL_PATH}"
exec "${INSTALL_PATH}/code128_updater" "$@"
EOF
chmod +x "${BIN_DIR}/code128_updater"
fi

echo "[4/4] Creando acceso en el Menú de Aplicaciones..."
cat << EOF > "${DESKTOP_DIR}/${APP_ID}.desktop"
[Desktop Entry]
Type=Application
Version=1.0
Name=Code128 Studio
GenericName=Diseñador de Etiquetas Code128
Comment=Generador e Impresor de Códigos de Barras Code 128 para Brother QL
Exec=${BIN_DIR}/code128_studio
Icon=${ICON_DIR}/${APP_ID}.png
Path=${INSTALL_DIR}
Terminal=false
Categories=Graphics;Office;Utility;Printing;
StartupNotify=true
StartupWMClass=Code128 Studio
EOF
chmod +x "${DESKTOP_DIR}/${APP_ID}.desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${DESKTOP_DIR}" 2>/dev/null || true
fi

echo ""
echo "========================================================"
echo "    ¡Instalación completada con éxito!"
echo "========================================================"
echo "• Podés abrirlo desde el menú de aplicaciones buscando 'Code128 Studio'"
echo "• O ejecutar en terminal: code128_studio"
if [[ ":$PATH:" != *":${BIN_DIR}:"* ]]; then
    echo "• NOTA: Asegurate de tener '${BIN_DIR}' en tu variable PATH:"
    echo "  echo 'export PATH=\"\$HOME/.local/bin:\$PATH\"' >> ~/.bashrc (o ~/.zshrc)"
fi
echo "• Para desinstalar en cualquier momento: ./install.sh --uninstall"
echo "========================================================"
