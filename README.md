# X21 CHEATS 🪽

## License and customer support

- X21TILKI (WhatsApp): +1 (437) 666-6044
- X21 KURT (WhatsApp): +359 892 358 538
- The license sign-in screen includes contact buttons for customers who want to purchase a license. The About page lists both WhatsApp support contacts.

## Free Fire FPS guidance

The Emulator tab's FPS guide summarizes vendor-supported BlueStacks settings. BlueStacks documents up to 90 FPS for Free Fire and a 120 FPS setting for Free Fire MAX on supported configurations. These are configuration options, not guaranteed results; the game profile, emulator, display refresh rate, and hardware must all support and sustain the target. The application does not overclock hardware or modify/inject into game files.

Sources: [Free Fire on BlueStacks](https://support.bluestacks.com/hc/en-us/articles/360059239591-Free-Fire-on-PC-with-BlueStacks-5), [Free Fire MAX at 120 FPS on BlueStacks](https://support.bluestacks.com/hc/en-us/articles/43864024050829-Free-Fire-MAX-on-PC-with-BlueStacks-5), [BlueStacks performance settings](https://support.bluestacks.com/hc/en-us/articles/360053245871-How-to-use-the-BlueStacks-5-Settings-menu), [Microsoft Windows power mode](https://support.microsoft.com/en-us/windows/experience/change-the-power-mode-for-your-windows-pc).

C++17 Windows desktop prototype built with Visual Studio 2026 (x64).

## Features in this prototype

- KeyAuth 1.3 startup initialization and license-key gate. The PC modules remain unavailable until KeyAuth accepts the entered license.
- BlueStacks / MSI App Player / LDPlayer process-name scan; it displays the process and PID only and does not inspect or modify game memory.
- A wider black, gold, and steel-blue X21 CHEATS interface with the supplied X21 artwork, embossed panels, larger readable text, optimizer, emulator, activity, and about navigation.
- V1 and V2 presets, individual modules, and a local activity panel.
- Windows modules: Game DVR capture settings, DiagTrack service state, NTFS last-access behavior, DNS cache flush, High Performance power plan, current-user TEMP cleanup, mouse acceleration settings, emulator process priority, BlueStacks GPU preference, Windows Game Mode, and an emulator configuration guide.

The registry and Windows settings are changed only after the user clicks a module. The temporary-file cleaner asks before deleting. Some restore information is kept in memory for the current run; keep the app open while applying and restoring settings. DNS flush and temporary-file cleanup are one-way actions. The DNS option only clears name-lookup cache; it does not promise lower game ping. Mouse latency and emulator crash items show guidance instead of applying unverified tweaks. The app asks for the license key directly and does not search AppData for saved keys.

This project is a PC optimizer UI. It does not implement game cheats, aim assistance, injection, or game-memory modification. The emulator stability module only shows configuration suggestions.

## Build

Open this folder in Visual Studio, or run CMake with the Visual Studio 18 2026 generator for x64. The repository includes the KeyAuth C++ 1.3 SDK and its dependencies under `third_party/keyauth-1.3`.

The app name, owner ID, version, API endpoint, and path are set in `main.cpp` from the values provided by the owner. A live KeyAuth license could not be tested in this build; enter a valid key created in the matching KeyAuth application to unlock the main window.

The bundled KeyAuth SDK retains its upstream `LICENSE` file. See `third_party/keyauth-1.3-vendor/LICENSE` before distributing the SDK or this project.

## GitHub Releases

The `.github/workflows/release.yml` workflow builds a Windows x64 package when a version tag such as `v1.0.1` is pushed. It publishes a ZIP containing the executable and `assets` folder, plus a SHA-256 checksum. After a valid license login, the app checks the public `orhan-31/x21-cheats-updates` Releases endpoint. If a newer tag exists, it offers the update, verifies the ZIP checksum, and replaces the running executable and assets after the app closes. To publish a release, push a new `vX.Y.Z` tag; GitHub Actions builds that exact app version and creates the release.
