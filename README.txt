Dynamic Visibility - OBS Filter Plugin v1.0.0
=============================================

Dynamic Visibility is a filter for OBS scenes and groups that manages the
visibility state of contained scene items automatically.


Package Contents
----------------

  dynamic-visibility-1.0.0\
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

Add the filter to an OBS scene or group:

  Right-click scene/group -> Filters -> "+" -> Visibility Rules

Available rule modes:

  - Exclusive: only one item remains visible at a time
  - Maximum visible count: at most `N` items remain visible
  - Optional safety rule: at least one item must stay visible


Requirements
------------

  - OBS Studio 30.x / 31.x / 32.x (Windows x64)
  - Pure libobs plugin, no Qt dependency


License
-------

GPLv2 or later.
