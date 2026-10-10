# DLSSClear

A Windows image-sharpening prototype for comparing and reducing perceived softness in game images, including images produced by DLSS Performance mode.

DLSSClear uses C++20, DirectX 11/12 and HLSL. It includes a standalone image viewer and its own optional game connection with a small in-game comparison panel. The native connection does not require ReShade or DLSSTweaks and does not replace NVIDIA DLSS files.

**Interface languages: English, Türkçe and 简体中文 (Simplified Chinese).** Choose a language in the main window. The choice is saved and shared with the updated in-game panel. Turkish and Chinese Windows installations default to their respective languages; other systems default to English.

This is an experimental spatial enhancement filter. It sharpens existing detail; it cannot reconstruct missing detail, eliminate every kind of blur, generate frames, or replace DLSS. Performance and compatibility depend on the game, resolution and GPU. A specific FPS cost is not guaranteed.

## Build and launch

Requirements:

- Windows 10/11, x64.
- Visual Studio with **Desktop development with C++**, the **v145** toolset and a Windows SDK (the project targets `10.0`).
- A DirectX 11 feature-level 11.0 device for the image viewer. The native DX12 path also requires DX12 support.
- The matching Microsoft Visual C++ runtime when running a build on another machine.

Open `DLSSClear.slnx`, select **Release / x64**, then build and run `DLSSClear`. The solution also builds the native DLL and its test application. No third-party package download is required by the build.

The application is written to `bin/x64/Release/`. Keep this layout when moving it:

```text
DLSSClear.exe
shaders/
  Clear.hlsl
native/
  dxgi.dll
  DLSSClearNativeTest.exe
```

The validated target is x64. Existing Win32 project configurations are retained, but the native game connection is x64 only.

## Image comparison

Start with the built-in test image, or use **Open image** for PNG, JPEG, BMP or TIFF screenshots. Select **100% view** to inspect individual pixels and drag with the right mouse button to pan.

| Action | Control |
| --- | --- |
| Open image | O |
| Save the processed image as PNG | S |
| Original / processed / split / difference | 1 / 2 / 3 / 4 |
| Move the split line | Left drag |
| Fit / 100% view | Z |
| Pan at 100% | Right drag |
| Reset | R |
| Recompile the image-viewer shader | F5 |

Controls adjust sharpness, fine-detail contrast, a noise-protection threshold and anti-ringing. The default **Balanced** preset uses 0.65 sharpness, 0.20 detail and 0.012 noise threshold. **Soft** and **Strong** provide alternative starting points. Two photo presets use wider sample spacing; photo controls do not automatically change game settings.

The noise threshold reduces sharpening in weak local differences; it is not a denoiser. Anti-ringing clamps the result to sampled neighborhood bounds. **Difference ×8** highlights changed pixels and is not a quality score. PNG export always writes the full-resolution processed image, independently of the preview mode. The source file cannot be overwritten through the export dialog.

The image path assumes SDR, 8-bit, sRGB-encoded values. HDR tone mapping, ICC profile management, EXIF orientation and animation are not implemented. Higher-bit-depth inputs are converted to 8-bit, and only the first frame is read. Inputs are limited to 40 megapixels and 16384 pixels per side.

## Connect a game

1. Close the game.
2. Click **Install to game** and select the actual 64-bit game executable, rather than its launcher.
3. Start the game and check DLSSClear's connection status. A detected game name alone does not mean the filter is active.
4. Use **Game filter**, its preset selector and **Split view**, or press **Home** inside the game.

Installation copies our `dxgi.dll` next to the selected game executable and writes an ownership/hash marker. An unrelated graphics DLL is not overwritten. **Uninstall**, with the game closed, removes only a verified DLSSClear installation. Updates also require the game to be closed.

Steam discovery is read-only, requires no Steam sign-in and does not install the connection automatically. The current game name is shown dynamically; the application is not specific to Cyberpunk 2077.

**Upgrading from an older build:** replace the application and its accompanying folders, then use **Install to game** again while the game is closed. Older game DLLs do not contain the new panel translations.

The native connection supports selected x64 DX11/DX12 SDR swap-chain paths. Vulkan, OpenGL, HDR, protected/anti-cheat games and every possible overlay or graphics-hook combination are not supported or validated. Unsupported image formats are passed through without filtering. Use the connection only where the game permits graphics modifications; no protection bypass is provided.

## In-game panel and shortcut

**Home** opens or closes the small panel on the left. **Up/Down** select a row; **Left/Right** or **Enter** change it. The panel offers filter on/off, Soft/Balanced/Strong presets, split comparison, reset and close. In split view, the left half is original and the right half is processed.

The panel is keyboard-operated and works even when sharpening is off. It can run without the main application. Alt+Tab closes the panel; press Home again after returning. Raw-input games may also receive these keys; key conflicts and exclusive-fullscreen paths need game-specific testing.

**F8** toggles the connected game's filter while the main application is running. To change it, focus **Game toggle key**, enter a function key or Ctrl/Alt combination, then click **Save key**. Backspace followed by Save key disables the shortcut. F12 is reserved. An unavailable shortcut is reported in the status line.

Language changes in the main application are also picked up by the updated game connection while the game is focused. Preference checks run on the input thread, not the rendering thread. The panel texture is rebuilt only when its contents, language or size change. A closed panel submits no panel draw or texture upload.

## Save per-game settings

With a game or **Connection demo** connected, choose its live settings and click **Save for this game**. **Load saved** restores the last saved setup. The saved values include enable/disable, sharpness, detail, noise threshold, anti-ringing and split comparison. Live changes made through Home can be saved from the main application; the Home panel does not yet have its own Save button.

Saved settings are automatically applied once when the main application first connects to a new game process. Temporary comparisons are not saved automatically. Automatic profile loading requires the main application to remain running; the game DLL does not load profiles by itself.

Profiles are stored under `%LOCALAPPDATA%/DLSSClear/profiles`, keyed by the full executable path. Moving a game requires saving a profile for its new location. Invalid profiles are not applied, and a failed save preserves the previous file. Language and shortcut preferences are stored separately in `%LOCALAPPDATA%/DLSSClear/settings.ini`.

## Background operation

The window's **X** and **Alt+F4** hide the application to the system tray. It leaves the taskbar and Alt+Tab list while discovery, profile loading and the toggle shortcut continue. Hidden image previews do not render.

Click the tray icon, or launch DLSSClear again, to restore the window. Use **Exit application** in the main window or tray menu to quit completely. Quitting stops the application's shortcut and profile service; the installed game connection and its Home panel remain active until the game exits.

## Validation

From the repository folder in PowerShell after building:

```powershell
$exe = '.\bin\x64\Release\DLSSClear.exe'
$p = Start-Process $exe -ArgumentList '--smoke-test', '--language=en' -Wait -PassThru
$p.ExitCode
$p = Start-Process $exe -ArgumentList '--smoke-test', '--language=zh-CN' -Wait -PassThru
$p.ExitCode
$p = Start-Process $exe -ArgumentList '--localization-test', 'validation-language' -Wait -PassThru
$p.ExitCode
$p = Start-Process $exe -ArgumentList '--profile-test', 'validation-profiles' -Wait -PassThru
$p.ExitCode
$p = Start-Process $exe -ArgumentList '--self-test', 'validation-release' -Wait -PassThru
$p.ExitCode
$p = Start-Process '.\bin\x64\Release\native\DLSSClearNativeTest.exe' -Wait -PassThru
$p.ExitCode
```

Zero means success. `--language=tr`, `en` or `zh-CN` overrides the main app's language for that launch without saving it. Localization validation uses an isolated settings file and checks preference persistence, shortcut preservation and failure handling. Profile validation checks the per-game persistence behavior. UI smoke tests exercise views, presets, resizing and shader reload while checking rendering-cache counts.

The image self-test reads back real GPU output to check identity, alpha, anti-ringing, photo radii, PNG round trips and caching. It also writes filter-pass benchmarks at 1080p, 1440p and 4K. Add `--warp` after its output folder to use software rendering. These timings exclude game rendering, I/O, presentation and the panel; they are not in-game FPS-loss measurements.

Native validation exercises DX11 and DX12, enable/disable, zero gain, split view, resizing, unsupported formats, GPU state preservation, panel bounds/alpha/caching and debug-layer diagnostics. It requires the Windows Direct3D debug layers. Its log is written beside the test executable. **Connection demo** is an interactive preview, not a performance benchmark.

## Source layout

- `src/`: Win32 interface, DX11 image viewer, Steam discovery, installation, hotkeys, tray behavior, profiles and validation.
- `src/Localization.h` and `src/Translations.h`: language preferences and the Turkish/English/Chinese catalog.
- `shaders/Clear.hlsl`: standalone image enhancement and comparison shaders.
- `native/`: DXGI connection, DX11/DX12 filters, in-game menu and test host.
- `game/`: optional legacy ReShade shader/preset retained as reference; not required by the native path.

The enhancement pass uses nine spatial samples with adaptive gain and optional neighborhood clamping. Static image previews cache unchanged results; games still require processing each new frame. Native filtering works on the final game image and can also affect HUD/text. Temporal reconstruction and a higher-cost reconstruction mode are not implemented.

DLSSClear is an independent project and is not affiliated with or endorsed by NVIDIA or any game developer. DLSS and other product names belong to their respective owners.
