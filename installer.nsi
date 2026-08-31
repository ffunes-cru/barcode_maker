; Installer Attributes
Name "Code128 Maker"
OutFile "Code128Maker_Setup.exe"  ; The name of the final installer EXE
InstallDir "$PROGRAMFILES\Code128Maker" ; Default installation path
Icon "font.otf" ; Use a resource file as the installer icon

; Pages
Page directory
Page instfiles

; The actual installation section
Section "Main Application Files"

  SetOutPath "$INSTDIR" ; Set the output directory to the installation folder

  ; --- Files to Bundle ---
  ; Note: 'install_package/bin/' is relative to the directory where you run 'makensis'

  ; 1. Executable and DLLs
  File /nonfatal "install_package/bin/code128_gui.exe"
  File "install_package/bin/code128_maker.exe"
  File "install_package/bin/freetype.dll" 
  ; Add other necessary MinGW/GCC runtime DLLs here if using GCC (e.g., libstdc++-6.dll)

  ; 2. Resource files
  File "install_package/bin/code128char.txt"
  File "install_package/bin/code128int.txt"
  File "install_package/bin/font.otf"

  ; Create shortcuts on the Desktop
  CreateShortCut "$DESKTOP\Code128 Studio.lnk" "$INSTDIR\code128_gui.exe"
  CreateShortCut "$DESKTOP\Code128 CLI Maker.lnk" "$INSTDIR\code128_maker.exe"

SectionEnd

; Uninstaller Section (Recommended for cleanuninstallation)
Section "Uninstall"

  ; Remove the main application files
  Delete "$INSTDIR\code128_gui.exe"
  Delete "$INSTDIR\code128_maker.exe"
  Delete "$INSTDIR\freetype.dll"
  Delete "$INSTDIR\code128char.txt"
  Delete "$INSTDIR\code128int.txt"
  Delete "$INSTDIR\font.otf"

  ; Remove the shortcut
  Delete "$DESKTOP\Code128 Studio.lnk"
  Delete "$DESKTOP\Code128 CLI Maker.lnk"

  ; Remove the install directory if it's empty
  RMDir "$INSTDIR"

  ; Remove the uninstaller itself
  Delete "$INSTDIR\uninstall.exe"

SectionEnd
