; ==============================================================================
; Inno Setup Script para Code128 Studio con Auto-Descarga desde GitHub
; ==============================================================================

#define MyAppName "Code128 Studio"
#define MyAppVersion "0.5"
#define MyAppPublisher "Cruceros Barcode Team"
#define MyAppExeName "code128_studio.exe"
#define GitHubRepo "ffunes-cru/barcode_maker"

[Setup]
AppId={{D37F62AA-64B1-4FE8-9844-3EC7DFB63E21}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\Code128Studio
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=build
OutputBaseFilename=Code128Studio_Setup_v{#MyAppVersion}
SetupIconFile=resources\icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "downloadlatest"; Description: "Comprobar y descargar la última versión desde GitHub Releases"; GroupDescription: "Actualizaciones:"

[Files]
; 1. Archivos base empaquetados (se instalan de base o como respaldo offline)
Source: "code128_studio.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\code128_studio.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "install_package\bin\code128_studio.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

Source: "code128_updater.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\code128_updater.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "install_package\bin\code128_updater.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

Source: "*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "install_package\bin\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

Source: "font.otf"; DestDir: "{app}"; Flags: ignoreversion
Source: "code128char.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "code128int.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "input_rep.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "presets.json"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "resources\icon.ico"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "resources\icon.png"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\icon.ico"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\icon.ico"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
// Funcion para consultar la API de GitHub y descargar el ultimo release compilado
procedure DownloadLatestFromGitHub(AppDir: string);
var
  ResultCode: Integer;
  PSCommand: string;
  ProgressMsg: string;
begin
  ProgressMsg := 'Conectando con GitHub para obtener la última versión de Code128 Studio...';
  WizardForm.StatusLabel.Caption := ProgressMsg;

  // Comando PowerShell para consultar releases y descargar el zip/exe
  PSCommand := 
    '[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; ' +
    '$repo = "{#GitHubRepo}"; ' +
    '$dest = "' + AppDir + '"; ' +
    '$tmpZip = Join-Path $env:TEMP "code128_github_latest.zip"; ' +
    'try { ' +
    '  $headers = @{ "User-Agent" = "Code128Studio-InnoInstaller" }; ' +
    '  $rel = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -Headers $headers -ErrorAction Stop; ' +
    '  $asset = $rel.assets | Where-Object { $_.name -like "*windows*" -or $_.name -like "*.zip" -or $_.name -like "*.exe" } | Select-Object -First 1; ' +
    '  if ($asset) { ' +
    '    Write-Output "Descargando $($asset.browser_download_url)..."; ' +
    '    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $tmpZip -UseBasicParsing; ' +
    '    if ($asset.name -like "*.zip") { ' +
    '      Expand-Archive -Path $tmpZip -DestinationPath $dest -Force; ' +
    '      Remove-Item -Force $tmpZip; ' +
    '    } else { ' +
    '      Move-Item -Force $tmpZip (Join-Path $dest $asset.name); ' +
    '    } ' +
    '  } ' +
    '} catch { ' +
    '  # Si no hay conexion o no hay release binario publicado, continuar con los archivos base ' +
    '}';

  Exec('powershell.exe', '-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "' + PSCommand + '"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('downloadlatest') then
  begin
    DownloadLatestFromGitHub(ExpandConstant('{app}'));
  end;
end;
