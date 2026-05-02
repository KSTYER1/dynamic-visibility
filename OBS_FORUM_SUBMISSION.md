# OBS Forum Submission Draft: Dynamic Visibility

## Resource Title

Dynamic Visibility

## Version

1.1.0

## Category

OBS Studio Plugins

## Tags

visibility, scene items, filters, automation, source filter

## Short Tagline

Automatically manage source and filter visibility with simple rules.

## Supported Bit Versions

64-bit

## Supported Platforms

Windows

## Minimum OBS Studio Version

30.0.0

## Source Code URL

https://github.com/KSTYER1/dynamic-visibility

## Download URL

https://github.com/KSTYER1/dynamic-visibility/releases/tag/v1.1.0

## Overview

Dynamic Visibility is an unofficial third-party OBS Studio filter plugin. It
adds two filters: `Source Visibility` for scene/group items and
`Filter Visibility` for filters on the same source.

## Features

- Control source visibility inside scenes and groups.
- Control filter visibility on the same source.
- `Only one active` rule.
- `At most N active` rule.
- Optional "keep at least one controlled item active" behavior.
- Filter include/exclude selection.
- English and German UI text.

## Installation

Download the Windows x64 release archive and extract or copy its contents into
your OBS Studio installation directory.

The final layout should include:

```text
obs-plugins/64bit/dynamic-visibility.dll
data/obs-plugins/dynamic-visibility/locale/en-US.ini
data/obs-plugins/dynamic-visibility/locale/de-DE.ini
```

Restart OBS after installation. The filters appear in the filter menu.

## License

GPL-2.0-or-later.

## Disclaimer

Dynamic Visibility is an unofficial third-party plugin and is not affiliated
with or endorsed by the OBS Project.

AI-assisted tools were used during development and release preparation. The
maintainer is responsible for reviewing, testing, and publishing the released
plugin.

## Pre-Submit Checklist

- [x] Public GitHub repository exists.
- [x] README is visible on GitHub after the next documentation push.
- [x] GPL license is visible on GitHub.
- [x] Source Code URL field points to the repository.
- [x] Release ZIP is attached to GitHub Releases or uploaded to the forum.
- [ ] At least one screenshot/GIF is added to the resource description.
- [x] Description is in English.
- [x] No OBS logo is used as resource icon or marketing artwork.
