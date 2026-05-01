Dynamic Visibility — OBS Filter Plugin v1.0.0
================================================

Filter fuer Szenen und Gruppen, der die Sichtbarkeit der enthaltenen
Items automatisch verwaltet — z. B. fuer Radio-Button-Verhalten zwischen
Overlays oder eine Auswahl-Logik fuer mehrere Webcams.


Was es macht
------------

Der Filter wird auf eine Szene oder Gruppe angewendet. Er lauscht auf
Sichtbarkeitsaenderungen aller enthaltenen Items (Augen-Icon) und
toggelt die anderen Items automatisch nach einer wahlbaren Regel:

  Exklusiv (Default)
    Nur eine Quelle darf sichtbar sein. Wer eine andere aktiviert,
    deaktiviert automatisch alle anderen.
    Use-Case: vier Banner ('Just Chatting', 'Gaming', 'BRB', 'Ende'),
    immer nur einer sichtbar.

  Hoechstens N sichtbar
    Bis zu N Quellen gleichzeitig sichtbar. Beim Aktivieren einer
    (N+1)-ten wird die aelteste sichtbare automatisch ausgeblendet.
    Use-Case: Multi-Cam mit max. 2 Cams gleichzeitig auf Bild.

  Mindestens eine Quelle muss sichtbar sein  (Checkbox, optional)
    Wirkt zusaetzlich auf beide Modi. Versucht man die letzte
    sichtbare Quelle auszublenden, springt sie automatisch zurueck.
    Use-Case: Cam-Variante darf nie ganz ausgehen.


Wichtig: nur fuer Szenen und Gruppen
------------------------------------

Wird der Filter auf eine normale Quelle angewendet, zeigt er nur eine
Warnung und bleibt komplett inaktiv. Keine Signal-Verbindungen, keine
Logik. OBS unterscheidet im Filter-Menue nicht zwischen Source-Typen,
deshalb der Schutz im Plugin selbst.


Installation
------------

Empfohlen: Doppelklick auf INSTALL.bat, OBS-Pfad eingeben, fertig.

Manuell: BEIDE Top-Level-Ordner (obs-plugins\, data\) in den
OBS-Hauptordner kopieren.


Verzeichnisstruktur
-------------------

  dynamic-visibility-1.0.0\
    INSTALL.bat
    README.txt
    obs-plugins\
      64bit\
        dynamic-visibility.dll      <- Plugin-Binary (~20 KB)
        dynamic-visibility.pdb      <- Debug-Symbole, optional
    data\
      obs-plugins\
        dynamic-visibility\
          locale\
            en-US.ini
            de-DE.ini


Verwendung
----------

1. OBS starten.
2. Eine Szene oder Gruppe auswaehlen.
3. Rechtsklick auf die Szene/Gruppe -> Filter -> "+" ->
   "Sichtbarkeits-Regeln".
4. Modus waehlen (Exklusiv oder Hoechstens N).
5. Bei "Hoechstens N": N einstellen.
6. Optional: Haekchen bei "Mindestens eine Quelle muss sichtbar sein".
7. Filter aktivieren (Augen-Icon im Filter-Dialog).

Sobald der Filter aktiv ist, reagiert er auf jedes Toggle der
Sichtbarkeit eines Items in dieser Szene/Gruppe.


Mehrere Filter-Instanzen
------------------------

Du kannst auf derselben Szene mehrere Sichtbarkeits-Regel-Filter
anwenden, falls du verschiedene Regelsaetze umschalten willst.
Alle aktiven Filter wirken parallel — pruefe selbst, dass sich die
Regeln nicht widersprechen (z. B. "Exklusiv" + "Hoechstens 3" auf
derselben Szene ist Unsinn, das eine ueberschreibt das andere staendig).


Items verfolgen
---------------

Der Filter referenziert Items intern ueber ihre Scene-Item-ID
(stabile Zahl, ueberlebt Rename). Wer eine Quelle in der Szene
umbenennt, dem laeuft die Logik weiter.


Anforderungen
-------------

  - OBS Studio 30.x / 31.x / 32.x (Windows x64)
  - Reine libobs-Lib, keine Qt- oder Frontend-API-Abhaengigkeit


Versionshistorie
----------------

1.0.0 (2026-04-27)
  * Erstrelease: Port von source-visibility-groups.lua
  * Filter wirkt auf alle Items der Parent-Szene/Gruppe (kein
    Source-Picker noetig wie im Lua)
  * Modi: Exklusiv, Hoechstens N
  * Optional: Mindestens eine sichtbar
  * Schutz: zeigt Warnung wenn auf normaler Quelle angewendet
  * Items via stabile Scene-Item-IDs (Rename-sicher)


Lizenz: GPLv2 oder neuer.
Source: https://github.com/kstyer/dynamic-visibility
