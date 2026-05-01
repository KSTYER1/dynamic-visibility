# Dynamic Visibility

Dynamic Visibility is a third-party OBS Studio filter plugin with two
visibility controllers:

- `Source Visibility` manages scene-item visibility inside scenes and groups.
- `Filter Visibility` manages enabled filters on the same source.

It is useful for mutually exclusive overlays, multi-camera selection logic,
scene filter presets, and filter stacks where only one look or a limited number
of effects should be active at once.

## Features

- `Source Visibility` for scenes and groups.
- `Filter Visibility` for sources, scenes, and groups.
- Exclusive mode so only one controlled item stays active at a time.
- Maximum-active mode so at most `N` controlled items stay active.
- Optional rule that at least one controlled item must remain active.
- Filter selection mode:
  - control all filters except selected filters
  - control only selected filters
- Visibility controller filters are protected automatically.
- German and English UI text.

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

Restart OBS after installation.

## Basic Usage

### Source Visibility

1. Select a scene or group in OBS.
2. Open the filter dialog.
3. Add `Source Visibility`.
4. Choose either `Only one active` or `At most N active`.
5. Set the maximum count if needed.
6. Optionally require that at least one controlled item remains active.
7. Enable the filter.

Once active, the filter reacts to scene-item visibility toggles in that scene
or group automatically.

### Filter Visibility

1. Select any OBS source, scene, or group.
2. Open the filter dialog.
3. Add `Filter Visibility`.
4. Choose which filters should be controlled:
   - `All filters except selected`
   - `Only selected filters`
5. Choose either `Only one active` or `At most N active`.
6. Optionally require that at least one controlled filter remains active.
7. Enable or disable one of the controlled filters.

`Filter Visibility` never controls `Source Visibility`, other
`Filter Visibility` instances, or itself.

## Version History

### 1.1.0

- Added `Filter Visibility`.
- Renamed the original UI entry to `Source Visibility`.
- Added filter include/exclude style selection.
- Added German and English UI text for both controllers.
- Preserved the original internal source visibility ID for compatibility.

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
