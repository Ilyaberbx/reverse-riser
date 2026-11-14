; Windows installer for Glacier Riser (Inno Setup 6).
;   iscc /DAppVersion=1.0.1 /DArtefacts=<...\Release> /O<out-dir> packaging\windows\GlacierRiser.iss
; Unsigned for now. signing: add a SignTool= line (e.g. Azure Trusted Signing) and SignedUninstaller=yes.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef Artefacts
  #define Artefacts "..\..\Builds\GlacierRiser_artefacts\Release"
#endif

[Setup]
; Fixed for the product's life: Windows matches upgrades and the uninstaller on it.
AppId={{6A1E3C52-9B7D-4C1A-8E2F-3D5B7A9C1E44}
AppName=Glacier Riser
AppVersion={#AppVersion}
AppVerName=Glacier Riser {#AppVersion}
AppPublisher=Ice Cold Sound
AppPublisherURL=https://icecoldsound.com
AppSupportURL=mailto:hello@icecoldsound.com
DefaultDirName={autopf}\Ice Cold Sound\Glacier Riser
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputBaseFilename=GlacierRiser-Windows-Setup
UninstallDisplayName=Glacier Riser
UninstallDisplayIcon={app}\Glacier Riser.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Messages]
WelcomeLabel2=This installs the Glacier Riser reverse-reverb riser from Ice Cold Sound.%n%nOn the next page you pick the formats: VST3 for your DAW, and the Standalone app if you want to use it without one.%n%nThe first time you open Glacier Riser it asks for your license key. You'll find it in your purchase email and on your account page.
FinishedHeadingLabel=You're all set
FinishedLabelNoIcons=Open your DAW and rescan plugins. Glacier Riser is under Ice Cold Sound.%n%nPaste your license key when it asks. Your key is at icecoldsound.com/en/account.%n%nSomething not working? Email hello@icecoldsound.com.
FinishedLabel=Open your DAW and rescan plugins. Glacier Riser is under Ice Cold Sound.%n%nPaste your license key when it asks. Your key is at icecoldsound.com/en/account.%n%nSomething not working? Email hello@icecoldsound.com.

[Types]
Name: "full"; Description: "Everything"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton Live, FL Studio, Cubase, Reaper, Bitwig, Studio One...)"; Types: full custom
Name: "standalone"; Description: "Standalone app, no DAW needed"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Desktop shortcut for the Standalone app"; Components: standalone; Flags: unchecked

[Files]
Source: "{#Artefacts}\VST3\Glacier Riser.vst3\*"; DestDir: "{commoncf64}\VST3\Glacier Riser.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\Glacier Riser.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Glacier Riser"; Filename: "{app}\Glacier Riser.exe"; Components: standalone
Name: "{autodesktop}\Glacier Riser"; Filename: "{app}\Glacier Riser.exe"; Tasks: desktopicon

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Glacier Riser.vst3"

[Run]
Filename: "{app}\Glacier Riser.exe"; Description: "Open Glacier Riser now"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked
