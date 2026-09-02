@echo off
setlocal EnableDelayedExpansion
title Instalador de Code128 Studio para Windows

echo ========================================================
echo     Instalador de Code128 Studio para Windows
echo ========================================================
echo.

set "TARGET_DIR=%LOCALAPPDATA%\Programs\Code128Studio"
set "SOURCE_DIR=%~dp0"
set "REPO=ffunes-cru/barcode_maker"

echo Instalando en: !TARGET_DIR!
echo.

:: 1. Crear carpetas de destino
if not exist "!TARGET_DIR!" (
    mkdir "!TARGET_DIR!"
)

:: 2. Intentar descargar la ultima version desde GitHub
echo [1/5] Verificando ultima version en GitHub (!REPO!)...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; " ^
  "$repo = '!REPO!'; " ^
  "$dest = '!TARGET_DIR!'; " ^
  "$tmpZip = Join-Path $env:TEMP 'code128_github_latest.zip'; " ^
  "try { " ^
  "  $headers = @{ 'User-Agent' = 'Code128Studio-Installer' }; " ^
  "  $rel = Invoke-RestMethod -Uri \"https://api.github.com/repos/$repo/releases/latest\" -Headers $headers -ErrorAction Stop; " ^
  "  Write-Host ('Version mas reciente encontrada: ' + $rel.tag_name); " ^
  "  $asset = $rel.assets | Where-Object { $_.name -like '*windows*' -or $_.name -like '*.zip' -or $_.name -like '*.exe' } | Select-Object -First 1; " ^
  "  if ($asset) { " ^
  "    Write-Host ('Descargando ' + $asset.name + '...'); " ^
  "    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $tmpZip -UseBasicParsing; " ^
  "    if ($asset.name -like '*.zip') { " ^
  "      Expand-Archive -Path $tmpZip -DestinationPath $dest -Force; " ^
  "      Remove-Item -Force $tmpZip; " ^
  "    } else { " ^
  "      Move-Item -Force $tmpZip (Join-Path $dest $asset.name); " ^
  "    } " ^
  "    Write-Host 'Descarga e instalacion desde GitHub completada exitosamente.'; " ^
  "  } else { " ^
  "    Write-Host 'No se encontraron binarios adjuntos en la ultima release de GitHub. Usando archivos locales.'; " ^
  "  } " ^
  "} catch { " ^
  "  Write-Host 'Sin conexion a GitHub Releases o no disponible. Usando archivos locales.'; " ^
  "}"

:: 3. Copiar ejecutables locales (como respaldo)
echo [2/5] Verificando ejecutables...
if exist "!SOURCE_DIR!code128_studio.exe" copy /Y "!SOURCE_DIR!code128_studio.exe" "!TARGET_DIR!\" >nul
if exist "!SOURCE_DIR!build\code128_studio.exe" copy /Y "!SOURCE_DIR!build\code128_studio.exe" "!TARGET_DIR!\" >nul
if exist "!SOURCE_DIR!install_package\bin\code128_studio.exe" copy /Y "!SOURCE_DIR!install_package\bin\code128_studio.exe" "!TARGET_DIR!\" >nul

if exist "!SOURCE_DIR!code128_updater.exe" copy /Y "!SOURCE_DIR!code128_updater.exe" "!TARGET_DIR!\" >nul
if exist "!SOURCE_DIR!build\code128_updater.exe" copy /Y "!SOURCE_DIR!build\code128_updater.exe" "!TARGET_DIR!\" >nul
if exist "!SOURCE_DIR!install_package\bin\code128_updater.exe" copy /Y "!SOURCE_DIR!install_package\bin\code128_updater.exe" "!TARGET_DIR!\" >nul

:: 4. Copiar librerias DLL si existen
if exist "!SOURCE_DIR!*.dll" copy /Y "!SOURCE_DIR!*.dll" "!TARGET_DIR!\" >nul
if exist "!SOURCE_DIR!install_package\bin\*.dll" copy /Y "!SOURCE_DIR!install_package\bin\*.dll" "!TARGET_DIR!\" >nul

:: 5. Copiar recursos del motor y presets
echo [3/5] Verificando fuentes, diccionarios y presets...
for %%F in (font.otf code128char.txt code128int.txt input_rep.txt presets.json) do (
    if not exist "!TARGET_DIR!\%%F" (
        if exist "!SOURCE_DIR!%%F" copy /Y "!SOURCE_DIR!%%F" "!TARGET_DIR!\" >nul
        if exist "!SOURCE_DIR!install_package\bin\%%F" copy /Y "!SOURCE_DIR!install_package\bin\%%F" "!TARGET_DIR!\" >nul
    )
)

if exist "!SOURCE_DIR!resources\icon.ico" (
    copy /Y "!SOURCE_DIR!resources\icon.ico" "!TARGET_DIR!\icon.ico" >nul
)
if exist "!SOURCE_DIR!resources\icon.png" (
    copy /Y "!SOURCE_DIR!resources\icon.png" "!TARGET_DIR!\icon.png" >nul
)

:: 6. Crear accesos directos (Escritorio y Menu Inicio)
echo [4/5] Creando accesos directos...
powershell -NoProfile -Command ^
  "$ws = New-Object -ComObject WScript.Shell; " ^
  "$desk = [Environment]::GetFolderPath('Desktop'); " ^
  "$scDesk = $ws.CreateShortcut((Join-Path $desk 'Code128 Studio.lnk')); " ^
  "$scDesk.TargetPath = '!TARGET_DIR!\code128_studio.exe'; " ^
  "$scDesk.WorkingDirectory = '!TARGET_DIR!'; " ^
  "if (Test-Path '!TARGET_DIR!\icon.ico') { $scDesk.IconLocation = '!TARGET_DIR!\icon.ico'; }; " ^
  "$scDesk.Save(); " ^
  "$smDir = Join-Path ([Environment]::GetFolderPath('Programs')) 'Code128 Studio'; " ^
  "New-Item -ItemType Directory -Force -Path $smDir | Out-Null; " ^
  "$scSm = $ws.CreateShortcut((Join-Path $smDir 'Code128 Studio.lnk')); " ^
  "$scSm.TargetPath = '!TARGET_DIR!\code128_studio.exe'; " ^
  "$scSm.WorkingDirectory = '!TARGET_DIR!'; " ^
  "if (Test-Path '!TARGET_DIR!\icon.ico') { $scSm.IconLocation = '!TARGET_DIR!\icon.ico'; }; " ^
  "$scSm.Save();"

:: 7. Crear script de desinstalacion
echo [5/5] Creando desinstalador...
(
echo @echo off
echo echo Desinstalando Code128 Studio...
echo powershell -NoProfile -Command ^
echo   "$desk = [Environment]::GetFolderPath('Desktop'); " ^
echo   "$link1 = Join-Path $desk 'Code128 Studio.lnk'; " ^
echo   "if (Test-Path $link1) { Remove-Item -Force $link1 }; " ^
echo   "$sm = Join-Path ([Environment]::GetFolderPath('Programs')) 'Code128 Studio'; " ^
echo   "if (Test-Path $sm) { Remove-Item -Recurse -Force $sm };"
echo cd ..
echo rmdir /S /Q "%LOCALAPPDATA%\Programs\Code128Studio"
echo echo Desinstalacion completada.
echo pause
) > "!TARGET_DIR!\uninstall.bat"

echo.
echo ========================================================
echo     Instalacion completada con exito en Windows
echo ========================================================
echo Acceso directo creado en el Escritorio y Menu Inicio.
echo.
pause
