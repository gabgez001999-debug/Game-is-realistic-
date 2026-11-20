#!/usr/bin/env bash
# Game is Realistic - one-command cloud gaming setup for a Lightning AI Studio.
#
# In the Studio (switch the machine to a GPU first, e.g. T4 or L4), open the terminal and run:
#   curl -sL https://raw.githubusercontent.com/gabgez001999-debug/Game-is-realistic-/claude/busy-cori-rpsiis/cloud/lightning_start.sh | bash
#
# It installs the build tools, builds the game, opens a free Cloudflare quick tunnel
# (no account needed) and prints a link. Open it, watch the intro, press START:
# the game launches on this GPU machine and streams into your browser.
# When nobody has played for 5 minutes the game exits and the Studio is stopped,
# so your free credits are not wasted.
#
# Options (environment variables): GIR_WIDTH, GIR_HEIGHT, GIR_FPS, GIR_PORT,
# GIR_BRANCH, GIR_IDLE (seconds), GIR_NO_TUNNEL=1 (local test), GIR_KEEP_STUDIO=1.
set -euo pipefail

REPO="${GIR_REPO:-https://github.com/gabgez001999-debug/Game-is-realistic-.git}"
BRANCH="${GIR_BRANCH:-claude/busy-cori-rpsiis}"
DIR="${GIR_DIR:-$HOME/game-is-realistic}"
PORT="${GIR_PORT:-8080}"
IDLE="${GIR_IDLE:-300}"

say() { printf '\n\033[1;33m== %s\033[0m\n' "$*"; }

say "Game is Realistic: Cloud-PC wird eingerichtet"
if command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi --query-gpu=name --format=csv,noheader >/dev/null 2>&1; then
  echo "GPU: $(nvidia-smi --query-gpu=name,driver_version --format=csv,noheader | head -1)"
else
  echo "WARNUNG: Keine NVIDIA-GPU gefunden. Oben rechts im Studio die Maschine auf eine GPU (T4/L4) umstellen,"
  echo "         sonst rendert der Prozessor und das Spiel ruckelt stark."
fi

SUDO=""
if [ "$(id -u)" != "0" ] && command -v sudo >/dev/null 2>&1; then SUDO="sudo"; fi

say "Pakete installieren (einmalig, ca. 1-2 Minuten)"
PKGS="git build-essential cmake ninja-build pkg-config libsdl2-dev libglm-dev libegl-dev libgl-dev libglvnd-dev libegl1 libegl-mesa0 libavcodec-dev libavutil-dev libx264-dev curl"
missing=""
for p in $PKGS; do dpkg -s "$p" >/dev/null 2>&1 || missing="$missing $p"; done
if [ -n "$missing" ]; then
  $SUDO apt-get update -qq
  $SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq $missing >/dev/null
fi

# Headless GPU rendering needs the NVIDIA EGL vendor file for libglvnd.
NV_EGL="$(ls /usr/lib/x86_64-linux-gnu/libEGL_nvidia.so.* 2>/dev/null | head -1 || true)"
if [ -n "$NV_EGL" ] && [ ! -f /usr/share/glvnd/egl_vendor.d/10_nvidia.json ]; then
  $SUDO mkdir -p /usr/share/glvnd/egl_vendor.d
  echo '{"file_format_version":"1.0.0","ICD":{"library_path":"libEGL_nvidia.so.0"}}' | $SUDO tee /usr/share/glvnd/egl_vendor.d/10_nvidia.json >/dev/null
fi

say "Spiel herunterladen und bauen"
if [ -d "$DIR/.git" ]; then
  git -C "$DIR" fetch -q --depth 1 origin "$BRANCH" && git -C "$DIR" reset -q --hard FETCH_HEAD
else
  git clone -q --depth 1 -b "$BRANCH" "$REPO" "$DIR"
fi
cmake -S "$DIR" -B "$DIR/build" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
ninja -C "$DIR/build" | tail -1

TOKEN="$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n' | head -c 20)"
URL="http://localhost:$PORT"
TUNNEL_PID=""
if [ "${GIR_NO_TUNNEL:-0}" != "1" ]; then
  say "Oeffentlichen Link erstellen (Cloudflare Quick Tunnel, kein Konto noetig)"
  CF="$HOME/.local/bin/cloudflared"
  if [ ! -x "$CF" ]; then
    mkdir -p "$HOME/.local/bin"
    curl -sL -o "$CF" https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64
    chmod +x "$CF"
  fi
  "$CF" tunnel --no-autoupdate --url "http://localhost:$PORT" > /tmp/gir-tunnel.log 2>&1 &
  TUNNEL_PID=$!
  for _ in $(seq 1 60); do
    URL="$(grep -o 'https://[a-z0-9-]*\.trycloudflare\.com' /tmp/gir-tunnel.log | head -1 || true)"
    [ -n "$URL" ] && break
    sleep 1
  done
  if [ -z "$URL" ]; then echo "Tunnel konnte nicht gestartet werden:"; tail -5 /tmp/gir-tunnel.log; exit 1; fi
fi
cleanup() { [ -n "$TUNNEL_PID" ] && kill "$TUNNEL_PID" 2>/dev/null || true; }
trap cleanup EXIT

printf '\n\033[1;32m================================================================\033[0m\n'
printf '\033[1;32m  DEIN SPIEL-LINK:\033[0m\n\n  %s/?token=%s\n\n' "$URL" "$TOKEN"
printf '  Im Browser oeffnen (Chrome, Edge oder Safari), Intro ansehen, START druecken.\n'
printf '  Nach %s Sekunden ohne Spieler wird alles beendet und das Studio gestoppt.\n' "$IDLE"
printf '\033[1;32m================================================================\033[0m\n\n'

cd "$DIR"
./build/realistic --stream --lazy-start --port "$PORT" \
  --width "${GIR_WIDTH:-1280}" --height "${GIR_HEIGHT:-720}" --fps "${GIR_FPS:-60}" \
  --token "$TOKEN" --idle-exit "$IDLE" --web "$DIR/web" || true

cleanup
if [ "${GIR_KEEP_STUDIO:-0}" != "1" ] && [ "${GIR_NO_TUNNEL:-0}" != "1" ]; then
  say "Spiel beendet - Studio wird gestoppt, damit keine Credits verbraucht werden"
  python3 -c "from lightning_sdk import Studio; Studio().stop()" 2>/dev/null \
    || echo "Bitte das Studio oben rechts manuell stoppen, damit keine Credits verbraucht werden."
fi
