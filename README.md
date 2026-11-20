# Game is Realistic

Ein Open-World-Spiel mit eigener C++-Engine (**Veritas Engine**). Es läuft im Fenster auf deinem Rechner oder als **Cloud-Gaming** auf einem GPU-Server: Website öffnen, **START** drücken, spielen.

> Stand: Engine, Grafik, Physik, Stadt und der komplette Cloud-Ablauf (Website → GPU-PC → Stream) funktionieren. Spielfigur, Autos mit Schadensmodell, Missionen und HUD sind in Arbeit. Im Moment erkundest du die Stadt mit einer freien Kamera.

---

## Spielen über die Cloud (Modal)

Der GPU-Server läuft bei [Modal](https://modal.com). Für GPUs musst du dort eine Zahlungsmethode hinterlegen, jeden Monat sind aber **30 $ Guthaben gratis** dabei. Mit einer L4-GPU reicht das für etwa 37 Stunden. Nach 90 Sekunden ohne Spieler fährt der Server automatisch herunter.

**Einmalig einrichten** (Terminal auf dem Mac):

```bash
pip3 install modal
python3 -m modal setup            # im Browser einloggen, z. B. "Continue with GitHub"
curl -O https://raw.githubusercontent.com/gabgez001999-debug/Game-is-realistic-/claude/busy-cori-rpsiis/cloud/modal_app.py
python3 -m modal deploy modal_app.py
```

Der letzte Befehl zeigt deinen **Zugangsschlüssel** und eine Adresse wie
`https://DEINNAME--game-is-realistic-api.modal.run`.

**Spielen:** Öffne `https://DEINNAME--game-is-realistic-api.modal.run/?key=DEIN-SCHLÜSSEL`. Es folgen das 3D-Intro und der START-Knopf. Nach einem Klick fährt der GPU-PC hoch und startet das Spiel automatisch (20–90 Sekunden), dann läuft es im Vollbild im Browser.

* GPU wählen: `GIR_GPU=L40S python3 -m modal deploy modal_app.py` (`T4`, `L4`, `A10`, `L40S`, `RTX-PRO-6000`)
* Browser: Chrome, Edge oder Safari 17+ (für den Video-Decoder WebCodecs)
* Controller: PS5-DualSense per Bluetooth oder USB mit dem Gerät koppeln, er wird erkannt.

## Selbst bauen (macOS / Linux)

```bash
# macOS
brew install cmake ninja sdl2 glm
# Ubuntu
sudo apt install cmake ninja-build libsdl2-dev libglm-dev libegl-dev libavcodec-dev libavutil-dev libx264-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/realistic                 # im Fenster spielen
./build/realistic --shot bild.png --view 1 --time 18   # Screenshot ohne Fenster (Linux)
./build/realistic --stream --port 8080                 # eigener Cloud-Server (Linux + GPU)
```

## Steuerung (aktuell: freie Kamera)

| Aktion | Tastatur/Maus | PS5-Controller |
|---|---|---|
| Bewegen | W A S D | linker Stick |
| Umschauen | Maus | rechter Stick |
| Hoch / runter | Leertaste / Strg | R2 / L2 |
| Schneller | Shift | L3 |
| Tageszeit +1 h | T | Steuerkreuz hoch |
| Regen an/aus | R | Steuerkreuz rechts |

## Technik

| Bereich | Umsetzung |
|---|---|
| Renderer | Deferred PBR (GGX), physikalischer Himmel (Nishita-Streuung), Wolken mit Schatten, 4 kaskadierte Schattenkarten, SSAO, Straßenschlucht-Verschattung, Screen-Space-Reflexionen, Wasser mit Wellen, Bloom, Auto-Belichtung, Bewegungsunschärfe, ACES-Tonemapping, FXAA |
| Materialien | Prozedural: Asphalt mit Fahrbahnmarkierungen und Rissen, Gehwegplatten, Fassaden mit Innenräumen hinter den Fenstern (Interior Mapping), Glastürme, Nässe und Pfützen |
| Physik | Eigene Engine: Rigid Bodies, Sequential-Impulse-Solver, Grid-Broadphase, Charakter-Kapsel ohne Durchclippen, Verlet-Ragdoll, abbrechbare Laternen und Ampeln |
| Stadt | Prozedurales Raster mit Hochhäusern, Ampeln, Palmen, Küste und Hügeln |
| Cloud | Headless-GPU-Rendering (EGL), H.264 mit NVENC oder x264, WebSocket-Stream, WebCodecs-Player, Eingaben inklusive DualSense, Abschaltung im Leerlauf |
| Controller | DualSense: Rumble, adaptive Trigger, Lightbar (lokal über SDL2) |

## Lizenzen von Fremd-Assets

* Microsoft Rocketbox Avatare und Animationen: MIT (`tools/fetch_characters.sh`)
* Poly Haven Texturen: CC0 (`tools/fetch_textures.py`)
* three.js (Intro der Website): MIT, über jsDelivr geladen
