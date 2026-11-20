#!/usr/bin/env bash
# Fetches realistic human avatars and motion-captured animations from the
# Microsoft Rocketbox library (MIT license) and converts them for the engine:
#   FBX -> GLB (assimp), TGA textures -> 1024px JPG/PNG (ffmpeg).
# Output: assets/characters/<Avatar>/{model.glb,body.jpg,head.jpg,body_n.jpg,head_n.jpg,opacity.png}
#         assets/anims/<clip>.glb
# Requires: git, assimp (assimp-utils), ffmpeg.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${TMPDIR:-/tmp}/rocketbox"
REPO="https://github.com/microsoft/Microsoft-Rocketbox.git"

AVATARS=(
  "Adults/Male_Adult_01" "Adults/Male_Adult_04" "Adults/Male_Adult_08" "Adults/Male_Adult_12"
  "Adults/Female_Adult_01" "Adults/Female_Adult_05" "Adults/Female_Adult_09"
  "Professions/Business_Male_02" "Professions/Business_Female_02"
)
ANIMS=(
  "xy/m_walk_neutral_01" "xy/m_walk_fast_01" "xy/m_run_slow_01" "xy/m_run_neutral" "xy/m_run_fast_01"
  "xy/m_walk_start" "xy/m_walk_stop" "xy/m_run_start" "xy/m_run_stop"
  "xy/f_walk_neutral_01" "xy/f_run_neutral" "xy/f_run_fast_01"
  "xyz/m_turn_left_90" "xyz/m_turn_right_90" "xyz/m_turn_left_180" "xyz/m_turn_right_180"
  "static/m_idle_neutral_01" "static/m_idle_neutral_02" "static/m_idle_look_around_01" "static/m_idle_breathe_01"
  "static/m_idle_angry_01" "static/m_idle_nervous_01" "static/m_idle_waiting_01"
  "static/f_idle_neutral_01" "static/f_idle_breathe_01"
  "static/m_gestic_talk_neutral_01" "static/m_gestic_talk_neutral_02" "static/m_gestic_talk_excited_01"
  "static/m_gestic_talk_nervous_01" "static/m_gestic_talk_relaxed_01" "static/m_gestic_talk_cool"
  "xyz/m_gestic_talk_angry_01" "xyz/m_gestic_talk_angry_02"
  "static/m_gestic_listen_neutral_01" "static/m_gestic_listen_accept_01" "static/m_gestic_listen_deny_01"
  "static/m_gestic_shrug_01" "static/m_gestic_laugh_low"
  "static/f_gestic_talk_neutral_01" "static/f_gestic_talk_excited_01" "static/f_gestic_talk_angry_01"
  "static/f_gestic_listen_neutral_01" "static/f_gestic_listen_accept_01"
  "static/m_cell_phone_talk_01" "static/m_cell_phone_listen_01" "static/m_cell_phone_textmessage"
  "static/m_sit_chair_idle_neutral_01" "static/m_sit_chair_idle_relaxed_01"
  "static/m_try_door_outwards" "static/m_knock_door" "static/m_wave_01"
  "static/m_crouch_idle" "static/m_cheer_01"
)

command -v assimp >/dev/null || { echo "assimp not found (apt install assimp-utils / brew install assimp)"; exit 1; }
command -v ffmpeg >/dev/null || { echo "ffmpeg not found"; exit 1; }

if [ ! -d "$WORK/.git" ]; then
  git clone -q --depth 1 --filter=blob:none --no-checkout "$REPO" "$WORK"
fi
cd "$WORK"
git sparse-checkout init --no-cone >/dev/null 2>&1 || true
PATTERNS=("/LICENSE")
for a in "${AVATARS[@]}"; do PATTERNS+=("/Assets/Avatars/$a/Export/$(basename "$a").fbx" "/Assets/Avatars/$a/Textures/*"); done
for c in "${ANIMS[@]}"; do PATTERNS+=("/Assets/Animations/all_animations_max_motextr_${c%%/*}/${c#*/}.max.fbx"); done
git sparse-checkout set --no-cone "${PATTERNS[@]}"
git checkout -q HEAD

mkdir -p "$ROOT/assets/characters" "$ROOT/assets/anims"
cp LICENSE "$ROOT/assets/characters/LICENSE-Rocketbox.txt"

tex() {  # src dst size
  ffmpeg -loglevel error -y -i "$1" -vf "scale=$3:$3" -q:v 3 "$2"
}

for a in "${AVATARS[@]}"; do
  name="$(basename "$a")"
  out="$ROOT/assets/characters/$name"
  mkdir -p "$out"
  assimp export "Assets/Avatars/$a/Export/$name.fbx" "$out/model.glb" >/dev/null
  T="Assets/Avatars/$a/Textures"
  tex "$(ls "$T"/*_body_color.tga | head -1)" "$out/body.jpg" 1024
  tex "$(ls "$T"/*_head_color.tga | head -1)" "$out/head.jpg" 1024
  tex "$(ls "$T"/*_body_normal.tga | head -1)" "$out/body_n.jpg" 1024
  tex "$(ls "$T"/*_head_normal.tga | head -1)" "$out/head_n.jpg" 1024
  op="$(ls "$T"/*_opacity_color.tga 2>/dev/null | head -1 || true)"
  if [ -n "$op" ]; then
    ffmpeg -loglevel error -y -i "$op" -vf "scale=1024:1024" -pix_fmt rgba "$out/opacity.png"
  fi
  echo "avatar  $name"
done

for c in "${ANIMS[@]}"; do
  clip="${c#*/}"
  assimp export "Assets/Animations/all_animations_max_motextr_${c%%/*}/$clip.max.fbx" "$ROOT/assets/anims/$clip.glb" >/dev/null
  echo "anim    $clip"
done
echo "done: $(ls "$ROOT/assets/characters" | wc -l) avatars, $(ls "$ROOT/assets/anims" | wc -l) clips"
