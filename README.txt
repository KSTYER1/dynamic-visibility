Dynamic Visibility - OBS Filter Plugin v1.1.0
=============================================

Dynamic Visibility provides two OBS filter controllers:

  - Source Visibility: controls scene items inside scenes and groups
  - Filter Visibility: controls filters on the same source


Package Contents
----------------

  dynamic-visibility-1.1.0\
    INSTALL.bat
    README.txt
    obs-plugins\
      64bit\
        dynamic-visibility.dll
        dynamic-visibility.pdb
    data\
      obs-plugins\
        dynamic-visibility\
          locale\
            en-US.ini
            de-DE.ini


Installation
------------

Recommended: double-click `INSTALL.bat`, choose your OBS folder, and let the
script copy the files for you.

Manual installation: copy BOTH top-level folders, `obs-plugins\` and `data\`,
into your OBS Studio installation directory.


Usage
-----

Source Visibility:

  Add this filter to a scene or group to control contained scene items.

  Right-click scene/group -> Filters -> "+" -> Source Visibility

Filter Visibility:

  Add this filter to any source, scene, or group to control the other filters
  on that same source.

  Right-click source -> Filters -> "+" -> Filter Visibility

Available rule modes:

  - Only one active: only one controlled item remains active at a time
  - At most N active: at most `N` controlled items remain active
  - Optional safety rule: at least one controlled item must stay active

Filter Visibility selection:

  - All filters except selected
  - Only selected filters

Visibility controller filters are always protected.


Requirements
------------

  - OBS Studio 30.x / 31.x / 32.x (Windows x64)
  - Pure libobs plugin, no Qt dependency


License
-------

GPLv2 or later.
