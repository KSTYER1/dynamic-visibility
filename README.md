# Dynamic Visibility

Dynamic Visibility is a third-party OBS Studio filter plugin for automatically
managing source visibility inside scenes and groups, plus filter visibility on
individual sources.

It is useful for setups where only one item in a group should be visible, where
only a limited number of items should remain visible, or where multiple filters
on the same source need simple visibility rules.

## Filters

### Source Visibility

`Source Visibility` is applied to a scene or group. It controls the visibility
of scene items inside that scene or group.

### Filter Visibility

`Filter Visibility` is applied to any source. It controls other filters on the
same source. Visibility filters are protected and are never disabled by this
filter.

## Features

- `Only one active` rule.
- `At most N active` rule.
- Optional "keep at least one controlled item active" behavior.
- Source-item control for scenes and groups.
- Filter control for filters on the same source.
- Filter selection modes for controlling all normal filters except selected
  ones, or only selected filters.
- German and English UI text.

## Requirements

- OBS Studio 30.x, 31.x, or 32.x
- Windows 64-bit release build

## Installation

Download the release archive and extract or copy its contents into your OBS
Studio installation directory.

The final layout should include:

```text
obs-plugins/64bit/dynamic-visibility.dll
data/obs-plugins/dynamic-visibility/locale/en-US.ini
data/obs-plugins/dynamic-visibility/locale/de-DE.ini
```

Restart OBS after installation. The filters appear in the filter menu.

## Basic Usage

1. Add `Source Visibility` to a scene or group to control contained scene
   items.
2. Add `Filter Visibility` to a source to control other filters on that source.
3. Choose the visibility rule.
4. For `Filter Visibility`, choose whether selected filters are included or
   excluded.

## Building from Source

Requires CMake 3.28 or newer, OBS development headers, and a supported compiler
toolchain.

```bash
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

## Source Code

Source code is available at:

```text
https://github.com/KSTYER1/dynamic-visibility
```

## Version History

### 1.1.0

- Renamed the original filter to `Source Visibility`.
- Added the new `Filter Visibility` filter.
- Added filter selection controls.
- Added German and English UI text for both filters.

### 1.0.0

- Initial release.
- Added scene and group source visibility rules.

## License

Dynamic Visibility is licensed under GPL-2.0-or-later.

## Disclaimer

Dynamic Visibility is an unofficial third-party plugin and is not affiliated
with or endorsed by the OBS Project.

AI-assisted tools were used during development and release preparation. The
maintainer is responsible for reviewing, testing, and publishing the released
plugin.
