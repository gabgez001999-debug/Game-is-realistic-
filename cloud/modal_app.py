"""
Game is Realistic - cloud gaming backend on Modal (https://modal.com).

What it does
  * Serves the website (intro + START button) from a tiny, cheap CPU container.
  * START  -> POST /launch  -> boots a GPU container, which starts the game in
              headless streaming mode and opens a public tunnel to it.
  * The page polls /status/<id> and connects to the game's WebSocket stream.
  * When the player leaves, the game shuts itself down after 90 s idle, so the
    GPU stops billing.

Deploy (once, from a terminal):
    pip install modal
    modal setup                      # log in (browser, e.g. "Continue with GitHub")
    modal deploy cloud/modal_app.py  # prints the website address

Options (environment variables at deploy time):
    GIR_GPU=L4|T4|A10|L40S|RTX-PRO-6000   GPU type (default L4)
    GIR_REPO / GIR_BRANCH                  which source to build
    GIR_KEY                                access key (generated and saved in ~/.gir_key if unset)
"""
import os
import pathlib
import secrets

import modal

REPO = os.environ.get("GIR_REPO", "https://github.com/gabgez001999-debug/Game-is-realistic-.git")
BRANCH = os.environ.get("GIR_BRANCH", "claude/busy-cori-rpsiis")
GPU = os.environ.get("GIR_GPU", "L4")


def _access_key() -> str:
    """Access key protecting /launch, so strangers cannot spend your GPU credits."""
    if os.environ.get("GIR_KEY"):
        return os.environ["GIR_KEY"]
    path = pathlib.Path.home() / ".gir_key"
    try:
        if path.exists():
            return path.read_text().strip()
        key = secrets.token_urlsafe(12)
        path.write_text(key)
        return key
    except OSError:
        return secrets.token_urlsafe(12)


ACCESS_KEY = _access_key() if modal.is_local() else os.environ.get("GIR_KEY", "")

# --- Container image: Ubuntu 24.04 + build tools + the game, built from source --------
game_image = (
    modal.Image.from_registry("ubuntu:24.04", add_python="3.12")
    .env({"DEBIAN_FRONTEND": "noninteractive", "NVIDIA_DRIVER_CAPABILITIES": "all", "NVIDIA_VISIBLE_DEVICES": "all"})
    .apt_install(
        "git", "build-essential", "cmake", "ninja-build", "pkg-config",
        "libsdl2-dev", "libglm-dev", "libegl-dev", "libgl-dev", "libgles-dev", "libglvnd-dev",
        "libegl1", "libegl-mesa0", "libgl1-mesa-dri", "libavcodec-dev", "libavutil-dev", "libx264-dev",
    )
    .run_commands(
        f"git clone --depth 1 -b {BRANCH} {REPO} /opt/gir",
        "cmake -S /opt/gir -B /opt/gir/build -G Ninja -DCMAKE_BUILD_TYPE=Release",
        "ninja -C /opt/gir/build",
    )
    .pip_install("fastapi[standard]==0.115.6")
)

app = modal.App("game-is-realistic")
sessions = modal.Dict.from_name("gir-sessions", create_if_missing=True)
key_secret = modal.Secret.from_dict({"GIR_KEY": ACCESS_KEY})


def _prepare_gpu_graphics() -> dict:
    """Expose the NVIDIA EGL driver to libglvnd (if the platform mounted it) and report the GPU."""
    import glob
    import json
    import subprocess

    info = {"gpu": "unknown", "egl_nvidia": False}
    try:
        out = subprocess.run(["nvidia-smi", "--query-gpu=name,driver_version", "--format=csv,noheader"],
                             capture_output=True, text=True, timeout=15).stdout.strip()
        if out:
            info["gpu"] = out
    except Exception:
        pass
    candidates = []
    for d in ["/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/usr/local/nvidia/lib64", "/usr/lib"]:
        candidates += glob.glob(f"{d}/libEGL_nvidia.so*")
    if candidates:
        info["egl_nvidia"] = True
        vendor_dir = pathlib.Path("/usr/share/glvnd/egl_vendor.d")
        vendor_dir.mkdir(parents=True, exist_ok=True)
        icd = vendor_dir / "10_nvidia.json"
        if not icd.exists():
            icd.write_text(json.dumps({"file_format_version": "1.0.0", "ICD": {"library_path": candidates[0]}}))
        lib_dir = os.path.dirname(candidates[0])
        os.environ["LD_LIBRARY_PATH"] = lib_dir + ":" + os.environ.get("LD_LIBRARY_PATH", "")
    return info


@app.function(image=game_image, gpu=GPU, cpu=8.0, memory=16384, timeout=4 * 3600, max_containers=2)
def game_server(session_id: str, token: str, width: int, height: int):
    """Runs on the GPU machine: starts the game and streams it through a public tunnel."""
    import subprocess

    info = _prepare_gpu_graphics()
    with modal.forward(8080) as tunnel:
        sessions[session_id] = {"state": "starting", "url": tunnel.url, "token": token, "gpu": info["gpu"]}
        cmd = ["/opt/gir/build/realistic", "--stream", "--port", "8080", "--width", str(width), "--height", str(height),
               "--fps", "60", "--token", token, "--idle-exit", "90", "--web", "/opt/gir/web"]
        proc = subprocess.Popen(cmd, cwd="/opt/gir", stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=os.environ.copy())
        renderer, encoder = "", ""
        for line in proc.stdout:
            print(line, end="", flush=True)
            if "Streaming ready" in line:
                # e.g. Streaming ready: renderer 'NVIDIA L4/PCIe/SSE2', encoder h264_nvenc
                renderer = line.split("renderer '")[-1].split("'")[0]
                encoder = line.strip().split("encoder ")[-1]
                sessions[session_id] = {"state": "ready", "url": tunnel.url, "token": token, "gpu": info["gpu"],
                                        "renderer": renderer, "encoder": encoder,
                                        "gpu_graphics": "llvmpipe" not in renderer.lower()}
        code = proc.wait()
        sessions[session_id] = {"state": "ended" if code == 0 else "failed", "error": f"Spiel beendet (Code {code})"}


@app.function(image=game_image, secrets=[key_secret], min_containers=0)
@modal.asgi_app()
def api():
    from fastapi import FastAPI, Header, HTTPException
    from fastapi.middleware.cors import CORSMiddleware
    from fastapi.staticfiles import StaticFiles

    web = FastAPI(title="Game is Realistic")
    web.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])
    expected = os.environ.get("GIR_KEY", "")

    def check(key: str):
        if expected and not secrets.compare_digest(key or "", expected):
            raise HTTPException(status_code=401, detail="bad key")

    @web.post("/launch")
    def launch(width: int = 1280, height: int = 720, x_gir_key: str = Header(default="")):
        check(x_gir_key)
        width = max(640, min(width, 1920)) & ~7
        height = max(360, min(height, 1080)) & ~7
        sid = secrets.token_hex(8)
        token = secrets.token_urlsafe(16)
        sessions[sid] = {"state": "queued"}
        game_server.spawn(sid, token, width, height)
        return {"id": sid}

    @web.get("/status/{sid}")
    def status(sid: str, x_gir_key: str = Header(default="")):
        check(x_gir_key)
        return sessions.get(sid, {"state": "unknown"})

    @web.get("/gir-config")
    def config():
        return {"mode": "modal", "gpu": GPU}

    # The website itself (intro + player); it detects /gir-config and uses this address as API.
    web.mount("/", StaticFiles(directory="/opt/gir/web", html=True), name="site")
    return web


@app.local_entrypoint()
def main():
    print("Deploy with:  modal deploy cloud/modal_app.py")


if modal.is_local():
    print("\n=== Game is Realistic ===")
    print(f"GPU: {GPU}   Branch: {BRANCH}")
    print(f"Zugangsschluessel: {ACCESS_KEY}")
    print("Nach 'modal deploy' die angezeigte api-Adresse oeffnen und so verwenden:")
    print(f"  <api-adresse>/?api=<api-adresse>&key={ACCESS_KEY}\n")
