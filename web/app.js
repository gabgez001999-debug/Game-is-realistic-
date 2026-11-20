// Game is Realistic - launcher + cloud stream player.
// START -> asks the Modal API to boot a GPU machine which starts the game, then
// connects to the game's WebSocket stream (H.264 via WebCodecs, PCM audio) and
// forwards keyboard, mouse and gamepad (DualSense) input.

const $ = (id) => document.getElementById(id);
const store = {
  get(k, d = "") { try { return localStorage.getItem("gir." + k) ?? d; } catch { return d; } },
  set(k, v) { try { localStorage.setItem("gir." + k, v); } catch {} },
};

// Settings from the URL (?api=...&key=...) are remembered for next time.
const params = new URLSearchParams(location.search);
if (params.get("api")) store.set("api", params.get("api"));
if (params.get("key")) store.set("key", params.get("key"));
if (params.get("api") || params.get("key")) history.replaceState(null, "", location.pathname);

let introReady = false;
function showStart() {
  if (introReady) return;
  introReady = true;
  $("start-wrap").classList.remove("hidden");
  $("start-btn").focus();
}
window.addEventListener("intro-ready", showStart);
setTimeout(showStart, 9000);  // fallback if WebGL / CDN is unavailable

function toast(msg, ms = 4000) {
  const t = $("toast");
  t.textContent = msg;
  t.classList.remove("hidden");
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => t.classList.add("hidden"), ms);
}

// --- Settings -----------------------------------------------------------------
$("settings-btn").addEventListener("click", openSettings);
function openSettings() {
  $("api-url").value = store.get("api");
  $("api-key").value = store.get("key");
  $("res").value = store.get("res", "1280x720");
  $("settings").showModal();
}
$("settings").addEventListener("close", () => {
  store.set("api", $("api-url").value.trim().replace(/\/+$/, ""));
  store.set("key", $("api-key").value.trim());
  store.set("res", $("res").value);
});

// Page served by the game server itself (local or direct tunnel)? Then connect directly.
async function directServer() {
  try {
    const r = await fetch("/health", { cache: "no-store" });
    return r.ok && (await r.text()) === "ok";
  } catch { return false; }
}

// --- Launch flow ----------------------------------------------------------------
let cancelled = false;
let session = null;

function setStep(step, msg, pct) {
  const order = ["request", "gpu", "game", "stream"];
  const idx = order.indexOf(step);
  document.querySelectorAll("#steps li").forEach((li, i) => {
    li.classList.toggle("done", i < idx);
    li.classList.toggle("active", i === idx);
  });
  if (msg !== undefined) $("launch-msg").textContent = msg;
  if (pct !== undefined) $("launch-bar").style.width = pct + "%";
}

$("start-btn").addEventListener("click", async () => {
  cancelled = false;
  try { await document.documentElement.requestFullscreen?.({ navigationUI: "hide" }); } catch {}
  audio.init();
  $("start-wrap").classList.add("hidden");
  $("launch").classList.remove("hidden");
  setStep("request", "", 5);
  try {
    if (await directServer()) {
      setStep("game", "Spiel wird auf dem Cloud-PC gestartet…", 70);
      connect(`${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/ws?token=${encodeURIComponent(params.get("token") || store.get("token"))}`);
      return;
    }
    let api = store.get("api");
    if (!api) {
      // Served by the Modal backend itself? Then this origin is the API.
      try {
        const c = await fetch("gir-config", { cache: "no-store" });
        if (c.ok && (await c.json()).mode === "modal") { api = location.origin; store.set("api", api); }
      } catch {}
    }
    if (!api) {
      $("launch").classList.add("hidden");
      $("start-wrap").classList.remove("hidden");
      toast("Bitte zuerst die Cloud-Server-Adresse eintragen.");
      openSettings();
      return;
    }
    const [w, h] = store.get("res", "1280x720").split("x").map(Number);
    const res = await fetch(`${api}/launch?width=${w}&height=${h}`, { method: "POST", headers: { "x-gir-key": store.get("key") } });
    if (res.status === 401) throw new Error("Zugangsschlüssel falsch (Einstellungen prüfen).");
    if (!res.ok) throw new Error(`Cloud-Server antwortet mit ${res.status}`);
    const { id } = await res.json();
    session = id;
    setStep("gpu", "GPU-Maschine wird zugewiesen (das kann 20–90 Sekunden dauern)…", 20);
    const t0 = performance.now();
    while (!cancelled) {
      await new Promise((r) => setTimeout(r, 1500));
      const st = await (await fetch(`${api}/status/${id}`, { headers: { "x-gir-key": store.get("key") } })).json();
      const secs = Math.round((performance.now() - t0) / 1000);
      if (st.state === "queued") setStep("gpu", `GPU-Maschine wird zugewiesen… ${secs}s`, Math.min(20 + secs, 45));
      else if (st.state === "starting") setStep("game", `Spiel wird gestartet… ${secs}s${st.gpu ? " · " + st.gpu : ""}`, Math.min(50 + secs / 2, 75));
      else if (st.state === "ready") {
        setStep("stream", `Verbinde Stream · ${st.renderer || ""}`, 88);
        const u = new URL(st.url);
        connect(`wss://${u.host}/ws?token=${encodeURIComponent(st.token)}`);
        return;
      } else if (st.state === "failed" || st.state === "ended") throw new Error(st.error || "Der Cloud-PC wurde beendet.");
      if (secs > 300) throw new Error("Zeitüberschreitung beim Starten des Cloud-PCs.");
    }
  } catch (e) {
    $("launch").classList.add("hidden");
    $("start-wrap").classList.remove("hidden");
    toast(e.message || String(e), 7000);
  }
});
$("cancel-btn").addEventListener("click", () => {
  cancelled = true;
  $("launch").classList.add("hidden");
  $("start-wrap").classList.remove("hidden");
});

// --- Audio: PCM ring buffer in an AudioWorklet ------------------------------------
const audio = {
  ctx: null, node: null,
  async init() {
    if (this.ctx) return;
    try {
      this.ctx = new AudioContext({ sampleRate: 48000, latencyHint: "interactive" });
      const src = `class P extends AudioWorkletProcessor{constructor(){super();this.b=new Float32Array(48000*2);this.r=0;this.w=0;this.n=0;
        this.port.onmessage=(e)=>{const d=e.data;for(let i=0;i<d.length;i++){this.b[this.w]=d[i];this.w=(this.w+1)%this.b.length;}
        this.n=Math.min(this.n+d.length,this.b.length);if(this.n>48000*2*0.25){const drop=this.n-48000*2*0.06;this.r=(this.r+drop)%this.b.length;this.n-=drop;}};}
        process(i,o){const L=o[0][0],R=o[0][1];for(let k=0;k<L.length;k++){if(this.n>=2){L[k]=this.b[this.r];R[k]=this.b[(this.r+1)%this.b.length];
        this.r=(this.r+2)%this.b.length;this.n-=2;}else{L[k]=0;R[k]=0;}}return true;}}registerProcessor('pcm',P);`;
      await this.ctx.audioWorklet.addModule(URL.createObjectURL(new Blob([src], { type: "text/javascript" })));
      this.node = new AudioWorkletNode(this.ctx, "pcm", { outputChannelCount: [2] });
      this.node.connect(this.ctx.destination);
    } catch (e) { console.warn("audio unavailable", e); }
  },
  push(int16) {
    if (!this.node) return;
    const f = new Float32Array(int16.length);
    for (let i = 0; i < int16.length; i++) f[i] = int16[i] / 32768;
    this.node.port.postMessage(f, [f.buffer]);
  },
};

// --- Stream player ----------------------------------------------------------------
let ws = null, decoder = null, cfg = null, waitingKey = true;
const screen = $("screen");
const ctx2d = screen.getContext("2d", { alpha: false, desynchronized: true });
const stats = { frames: 0, fps: 0, rtt: 0, mbps: 0, last: performance.now() };

function connect(url) {
  if (!("VideoDecoder" in window)) {
    toast("Dieser Browser kann den Video-Stream nicht dekodieren (WebCodecs fehlt). Bitte Chrome, Edge oder Safari 17+ nutzen.", 9000);
  }
  ws = new WebSocket(url);
  ws.binaryType = "arraybuffer";
  ws.onopen = () => {
    ws.send(JSON.stringify({ t: "hello", pad: padType(), w: screen.width, h: screen.height }));
    setStep("game", "Cloud-PC startet das Spiel… (einige Sekunden)", 80);
  };
  ws.onmessage = (ev) => {
    if (typeof ev.data === "string") return onText(JSON.parse(ev.data));
    const d = new Uint8Array(ev.data);
    if (d[0] === 1) onVideo(d);
    else if (d[0] === 2) audio.push(new Int16Array(ev.data.slice(8)));
  };
  ws.onclose = () => {
    if (!$("player").classList.contains("hidden")) toast("Verbindung zum Cloud-PC beendet.", 6000);
    showMenu();
  };
  ws.onerror = () => toast("Stream-Verbindung fehlgeschlagen.", 6000);
}

function onText(m) {
  if (m.type === "config") {
    setStep("stream", "Erstes Bild kommt…", 95);
    cfg = m;
    screen.width = m.width;
    screen.height = m.height;
    setupDecoder();
  } else if (m.type === "pong") {
    stats.rtt = performance.now() - m.ts;
    stats.mbps = m.mbps;
  } else if (m.type === "busy") {
    toast("Auf diesem Cloud-PC spielt gerade schon jemand.", 7000);
  } else if (m.type === "rumble") {
    for (const gp of navigator.getGamepads?.() || []) {
      gp?.vibrationActuator?.playEffect?.("dual-rumble", { duration: m.ms, strongMagnitude: m.lo, weakMagnitude: m.hi }).catch(() => {});
    }
  }
}

async function setupDecoder() {
  if (!("VideoDecoder" in window) || !cfg) return;
  try { if (decoder && decoder.state !== "closed") decoder.close(); } catch {}
  // Prefer the hardware decoder, fall back to whatever the browser supports.
  let config = { codec: cfg.codec, optimizeForLatency: true, hardwareAcceleration: "prefer-hardware" };
  try {
    if (!(await VideoDecoder.isConfigSupported(config)).supported) config = { codec: cfg.codec, optimizeForLatency: true };
  } catch { config = { codec: cfg.codec }; }
  decoder = new VideoDecoder({
    output: (frame) => {
      ctx2d.drawImage(frame, 0, 0, screen.width, screen.height);
      frame.close();
      stats.frames++;
      if ($("player").classList.contains("hidden")) showPlayer();
    },
    error: (e) => {
      console.warn("decoder", e);
      waitingKey = true;
      if (!setupDecoder.retry) { setupDecoder.retry = true; setTimeout(() => { setupDecoder.retry = false; setupDecoder(); }, 300); }
    },
  });
  decoder.configure(config);
  waitingKey = true;
  send({ t: "kf" });
}

function onVideo(d) {
  if (!decoder || decoder.state !== "configured") return;
  const key = (d[1] & 1) === 1;
  if (waitingKey && !key) return;
  waitingKey = false;
  let ts = 0;
  for (let i = 7; i >= 0; i--) ts = ts * 256 + d[2 + i];
  if (decoder.decodeQueueSize > 6) { waitingKey = true; send({ t: "kf" }); return; }  // catch up after a stall
  decoder.decode(new EncodedVideoChunk({ type: key ? "key" : "delta", timestamp: ts, data: d.subarray(10) }));
}

function send(o) { if (ws && ws.readyState === 1) ws.send(JSON.stringify(o)); }

function showPlayer() {
  $("launch").classList.add("hidden");
  $("player").classList.remove("hidden");
  window.dispatchEvent(new Event("intro-pause"));
  setTimeout(() => ($("hint").style.opacity = 0), 6000);
}
function showMenu() {
  $("player").classList.add("hidden");
  $("launch").classList.add("hidden");
  $("start-wrap").classList.remove("hidden");
  window.dispatchEvent(new Event("intro-resume"));
  try { document.exitPointerLock?.(); } catch {}
}
$("quit-btn").addEventListener("click", () => { ws?.close(); showMenu(); });

setInterval(() => {
  const now = performance.now();
  stats.fps = (stats.frames * 1000) / (now - stats.last);
  stats.frames = 0;
  stats.last = now;
  send({ t: "ping", ts: now });
  if (cfg) $("stats").innerHTML = `${stats.fps.toFixed(0)} FPS · ${stats.rtt.toFixed(0)} ms · ${(stats.mbps || 0).toFixed(1)} Mbit/s<br>${cfg.renderer} · ${cfg.encoder}`;
}, 1000);

// --- Input ----------------------------------------------------------------------
const gameKeys = new Set(["Space", "Tab", "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Backspace"]);
window.addEventListener("keydown", (e) => {
  if ($("player").classList.contains("hidden")) return;
  if (e.repeat) return;
  if (gameKeys.has(e.code) || e.code.startsWith("Key") || e.code.startsWith("Digit")) e.preventDefault();
  send({ t: "kd", c: e.code });
});
window.addEventListener("keyup", (e) => {
  if ($("player").classList.contains("hidden")) return;
  send({ t: "ku", c: e.code });
});
screen.addEventListener("click", () => screen.requestPointerLock?.({ unadjustedMovement: true })?.catch?.(() => screen.requestPointerLock()));
document.addEventListener("mousemove", (e) => {
  if (document.pointerLockElement === screen) send({ t: "mm", x: e.movementX, y: e.movementY });
});
screen.addEventListener("mousedown", (e) => send({ t: "md", b: e.button === 2 ? 1 : e.button === 1 ? 2 : 0 }));
screen.addEventListener("mouseup", (e) => send({ t: "mu", b: e.button === 2 ? 1 : e.button === 1 ? 2 : 0 }));
screen.addEventListener("wheel", (e) => { e.preventDefault(); send({ t: "wh", y: -Math.sign(e.deltaY) }); }, { passive: false });
screen.addEventListener("contextmenu", (e) => e.preventDefault());

// Gamepad: standard mapping -> engine indices (A,B,X,Y,LB,RB,Back,Start,L3,R3,Up,Down,Left,Right).
const BTN_MAP = [0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15];
const lastAxes = new Array(6).fill(0), lastBtn = new Array(14).fill(false);
function padType() {
  const gp = [...(navigator.getGamepads?.() || [])].find(Boolean);
  if (!gp) return "";
  const id = gp.id.toLowerCase();
  if (id.includes("dualsense") || id.includes("0ce6") || id.includes("0df2")) return "ps5";
  if (id.includes("054c") || id.includes("wireless controller")) return "ps4";
  if (id.includes("xbox") || id.includes("045e")) return "xbox";
  return "other";
}
window.addEventListener("gamepadconnected", () => { send({ t: "hello", pad: padType() }); toast(padType() === "ps5" ? "PS5-Controller verbunden" : "Controller verbunden"); });
function pollPad() {
  requestAnimationFrame(pollPad);
  if ($("player").classList.contains("hidden")) return;
  const gp = [...(navigator.getGamepads?.() || [])].find(Boolean);
  if (!gp) return;
  const dz = (v) => (Math.abs(v) < 0.1 ? 0 : v);
  const axes = [dz(gp.axes[0] || 0), dz(gp.axes[1] || 0), dz(gp.axes[2] || 0), dz(gp.axes[3] || 0),
                gp.buttons[6]?.value || 0, gp.buttons[7]?.value || 0];
  axes.forEach((v, i) => { if (Math.abs(v - lastAxes[i]) > 0.01) { lastAxes[i] = v; send({ t: "pa", i, v: +v.toFixed(3) }); } });
  BTN_MAP.forEach((bi, i) => {
    const p = !!gp.buttons[bi]?.pressed;
    if (p !== lastBtn[i]) { lastBtn[i] = p; send({ t: "pb", i, v: p ? 1 : 0 }); }
  });
}
pollPad();
