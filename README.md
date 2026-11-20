# Game is Realistic

Ein Open-World-Spiel mit eigener C++-Engine (**Veritas Engine**). Es läuft im Fenster auf deinem Rechner oder als **Cloud-Gaming** auf einem GPU-Server: Website öffnen, **START** drücken, spielen.

> Stand: Engine, Grafik, Physik, Stadt und der komplette Cloud-Ablauf (Website → GPU-PC → Stream) funktionieren. Spielfigur, Autos mit Schadensmodell, Missionen und HUD sind in Arbeit. Im Moment erkundest du die Stadt mit einer freien Kamera.

---

## Spielen über die Cloud: Lightning AI (ohne Kreditkarte)

[Lightning AI](https://lightning.ai) stellt Cloud-PCs mit Nvidia-GPU bereit. Nach eigenen Angaben gibt es dort **ohne Kreditkarte** 5 Gratis-Credits. Du brauchst ein Konto (E-Mail, laut FAQ eventuell eine Telefonnummer).

1. Bei [lightning.ai](https://lightning.ai) anmelden und ein **Studio** öffnen.
2. Oben rechts die Maschine auf eine **GPU** umstellen (T4 oder L4).
3. Im Studio das **Terminal** öffnen und diesen einen Befehl einfügen:

   ```bash
   curl -sL https://raw.githubusercontent.com/gabgez001999-debug/Game-is-realistic-/claude/busy-cori-rpsiis/cloud/lightning_start.sh | bash
   ```

4. Nach 2–4 Minuten erscheint **DEIN SPIEL-LINK** (`https://….trycloudflare.com/?token=…`). Öffne ihn im Browser. Es kommt das Intro, dann drückst du **START**, und das Spiel startet auf dem Cloud-PC.
5. Nach 5 Minuten ohne Spieler beendet sich das Spiel und stoppt das Studio, damit die Gratis-Credits nicht verbraucht werden.

Der Link läuft über einen kostenlosen [Cloudflare Quick Tunnel](https://try.cloudflare.com), dafür braucht es kein Konto. Der Token im Link sorgt dafür, dass nur du dich verbinden kannst.

## Alternative: Modal (Zahlungsmethode für GPUs nötig)

`cloud/modal_app.py`: Website und API auf Modal, der GPU-PC startet erst beim Klick auf START. Für GPUs verlangt Modal eine hinterlegte Zahlungsmethode, dafür gibt es jeden Monat 30 $ Guthaben. Anleitung steht oben in der Datei.

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
