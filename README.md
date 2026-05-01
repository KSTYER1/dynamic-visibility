# Dynamic Visibility

Dynamic Visibility is a third-party OBS Studio filter plugin for scenes and
groups.

It automatically manages the visibility of contained scene items, which makes
it useful for mutually exclusive overlays, multi-camera selection logic, and
other rule-based visibility workflows.

## Features

- Exclusive mode so only one item stays visible at a time.
- Maximum-visible mode so at most `N` items stay visible.
- Optional rule that at least one item must remain visible.
- Works on all scene items in the parent scene or group automatically.
- Uses stable scene-item IDs internally, so item renames do not break tracking.
- Guard behavior when attached to unsupported source types.

## Requirements

- OBS Studio 30.x, 31.x, or 32.x
- Windows x64 for the packaged release
- Pure `libobs` plugin with no Qt or frontend API dependency

## Installation

### Windows

Download the release archive and extract or copy its contents into your OBS
Studio installation directory.

The final layout should include:

```text
obs-plugins/64bit/dynamic-visibility.dll
data/obs-plugins/dynamic-visibility/locale/en-US.ini
data/obs-plugins/dynamic-visibility/locale/de-DE.ini
```

The packaged release also includes `INSTALL.bat`, which can copy the plugin
files into a selected OBS directory.

Restart OBS after installation. The filter appears in the filter menu for
scene and group sources.

## Basic Usage

1. Select a scene or group in OBS.
2. Open the filter dialog.
3. Add `Visibility Rules`.
4. Choose either exclusive mode or maximum-visible mode.
5. Set the maximum count if needed.
6. Optionally require that at least one item remains visible.
7. Enable the filter.

Once active, the filter reacts to scene-item visibility toggles in that scene
or group automatically.

## Version History

### 1.0.0

- Initial release.
- Ported the original `source-visibility-groups.lua` concept.
- Added exclusive and maximum-visible modes.
- Added optional minimum-one-visible protection.
- Added unsupported-source guard behavior.
- Added stable scene-item ID tracking.

## License

Dynamic Visibility is licensed under GPL-2.0-or-later.

## Disclaimer

Dynamic Visibility is an unofficial third-party plugin and is not affiliated
with or endorsed by the OBS Project.
