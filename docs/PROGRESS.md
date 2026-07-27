# Theme Loader Implementation Progress

## Current Status

- Started implementation from `C:\Users\Administrator\.doge\plans\cryptic-inventing-hennessy.md`.
- Scope in progress: theme package loading, persistent theme selection, QML theme tokens, wallpaper support, settings UI.

## Progress

- [x] Add `Config::themesPath()`
- [x] Add `Settings::currentThemeId`
- [x] Add theme metadata / loader / manager C++ classes
- [x] Integrate `.tar.zst` extraction through libarchive
- [x] Wire dynamic colors, fonts, and wallpaper into `Theme.qml`
- [x] Add settings UI for installed themes and package installation
- [x] Build verification
- [x] Android async theme package picker signal
- [x] Default `preview.jpg` discovery
- [x] Wallpaper/preview decode-size validation
- [x] Example cute theme package at `examples/themes/cute-clouds.tar.zst`
- [x] Fix Windows false positive in theme archive destination safety check
- [x] Make wallpaper visible behind app pages by using translucent page overlays

## Notes

- Existing QML uses `Theme { id: theme }`, so implementation should preserve existing token names.
- Theme package wallpaper support must include light/dark image paths, mode, and opacity.
- Keep Android writable theme files under `.starryagent/themes/`, not APK assets.
- Android theme package picking now uses `ACTION_OPEN_DOCUMENT`, copies the selected package into app storage, and emits `themePackagePicked`.
- Build passed with `xmake -y` after changing `mode_t` to `auto` in libarchive entry handling for Windows.
- The `cute-clouds` sample theme includes `theme.json`, `preview.png`, and light/dark cloud wallpapers. Package contents were verified with `tar -tf`.
- Theme archive paths are normalized before extraction so `./preview.png` and Windows path separators do not trigger a false "escapes destination" error.
- `Theme.qml` now exposes `hasWallpaper` and `pageOverlay`; Chat, Settings, Scheduled Tasks, and Mode Picker use the overlay so wallpaper is not hidden by opaque page rectangles.
