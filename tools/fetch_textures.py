#!/usr/bin/env python3
"""Downloads the photoscanned CC0 PBR materials used by the engine from Poly Haven.

All textures are CC0 (public domain) - https://polyhaven.com/license
Writes assets/textures/<name>/{diff,nor,arm}.jpg and assets/textures/manifest.txt
(one line per texture-array layer: "<layer> <name> <width_m> <height_m>").

Usage: python3 tools/fetch_textures.py [--res 1k|2k]
"""
import hashlib
import json
import os
import sys
import urllib.request

# Layer order must match the MATL_* constants in shaders/gbuffer.frag.
MATERIALS = [
    "asphalt_02",              # 0  roads
    "concrete_pavement",       # 1  sidewalks
    "concrete_tile_facade",    # 2  office stone facades
    "brick_wall_02",           # 3  brick facades
    "white_stucco",            # 4  stucco / plaster (tinted pastel)
    "coast_sand_01",           # 5  dry beach sand
    "damp_beach_sand",         # 6  wet sand at the waterline
    "leafy_grass",             # 7  lawns / terrain grass
    "aerial_rocks_02",         # 8  hills / cliffs
    "palm_tree_bark",          # 9  palm trunks
    "wood_floor_deck",         # 10 boardwalks, docks
    "box_profile_metal_sheet", # 11 containers, sheds
    "tarred_gravel",           # 12 flat roofs
    "dry_ground_01",           # 13 dirt
    "granite_tile",            # 14 curbs, plinths
    "rusty_metal_02",          # 15 port / industrial
]

API = "https://api.polyhaven.com"
UA = {"User-Agent": "veritas-engine-asset-fetch/1.0"}


def get_json(url):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)


def download(url, path, md5):
    if os.path.exists(path) and hashlib.md5(open(path, "rb").read()).hexdigest() == md5:
        return False
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=120) as r:
        data = r.read()
    if hashlib.md5(data).hexdigest() != md5:
        raise RuntimeError(f"checksum mismatch for {url}")
    with open(path, "wb") as f:
        f.write(data)
    return True


def main():
    res = "1k"
    if "--res" in sys.argv:
        res = sys.argv[sys.argv.index("--res") + 1]
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "textures")
    os.makedirs(root, exist_ok=True)
    lines = []
    for layer, name in enumerate(MATERIALS):
        files = get_json(f"{API}/files/{name}")
        info = get_json(f"{API}/info/{name}")
        dims = info.get("dimensions", [2000, 2000])  # millimetres
        out = os.path.join(root, name)
        os.makedirs(out, exist_ok=True)
        maps = {"diff": files["Diffuse"], "nor": files["nor_gl"], "arm": files["arm"]}
        for key, entry in maps.items():
            f = entry[res]["jpg"]
            changed = download(f["url"], os.path.join(out, key + ".jpg"), f["md5"])
            print(f"[{layer:2d}] {name:24s} {key:4s} {'downloaded' if changed else 'cached'}")
        lines.append(f"{layer} {name} {dims[0] / 1000.0:.3f} {dims[1] / 1000.0:.3f}")
    with open(os.path.join(root, "manifest.txt"), "w") as f:
        f.write("# layer name width_m height_m  (Poly Haven, CC0)\n")
        f.write("\n".join(lines) + "\n")
    print("ok:", len(lines), "materials")


if __name__ == "__main__":
    main()
