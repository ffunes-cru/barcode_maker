; Installer Attributes
Name "Code128 Studio"
OutFile "Code128Studio_Setup.exe"
InstallDir "$PROGRAMFILES\Code128Studio"
Icon "font.otf"

; Pages
Page directory
Page instfiles

; The actual installation section
Section "Main Application Files"

  SetOutPath "$INSTDIR"

  ; 1. Executable and DLLs
  File "install_package/bin/code128_studio.exe"
  File /nonfatal "install_package/bin/code128_updater.exe"
  File /nonfatal "install_package/bin/freetype.dll"
  File /nonfatal "install_package/bin/glfw3.dll"

  ; 2. Resource files
  File "install_package/bin/code128char.txt"
  File "install_package/bin/code128int.txt"
  File "install_package/bin/font.otf"
  File /nonfatal "install_package/bin/input_rep.txt"

  ; Create shortcuts on the Desktop
  CreateShortCut "$DESKTOP\Code128 Studio.lnk" "$INSTDIR\code128_studio.exe"

SectionEnd

; Uninstaller Section
Section "Uninstall"

  Delete "$INSTDIR\code128_studio.exe"
  Delete "$INSTDIR\code128_updater.exe"
  Delete "$INSTDIR\freetype.dll"
  Delete "$INSTDIR\glfw3.dll"
  Delete "$INSTDIR\code128char.txt"
  Delete "$INSTDIR\code128int.txt"
  Delete "$INSTDIR\font.otf"
  Delete "$INSTDIR\input_rep.txt"

  Delete "$DESKTOP\Code128 Studio.lnk"

  RMDir "$INSTDIR"
  Delete "$INSTDIR\uninstall.exe"

SectionEnd
