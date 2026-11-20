// Cinematic 3D intro: sunset skyline over a mirrored sea, chrome title letters that
// assemble one by one, a light sweep, drifting embers and bloom.
import * as THREE from "three";
import { FontLoader } from "three/addons/loaders/FontLoader.js";
import { TextGeometry } from "three/addons/geometries/TextGeometry.js";
import { EffectComposer } from "three/addons/postprocessing/EffectComposer.js";
import { RenderPass } from "three/addons/postprocessing/RenderPass.js";
import { UnrealBloomPass } from "three/addons/postprocessing/UnrealBloomPass.js";
import { OutputPass } from "three/addons/postprocessing/OutputPass.js";

const canvas = document.getElementById("intro");
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: "high-performance" });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.5));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 0.0;  // fades in
renderer.outputColorSpace = THREE.SRGBColorSpace;

const scene = new THREE.Scene();
scene.fog = new THREE.FogExp2(0x2a1a22, 0.0032);
const camera = new THREE.PerspectiveCamera(38, window.innerWidth / window.innerHeight, 0.1, 3000);

// --- Sunset sky -------------------------------------------------------------
const skyMat = new THREE.ShaderMaterial({
  side: THREE.BackSide, depthWrite: false, fog: false,
  uniforms: { uTime: { value: 0 } },
  vertexShader: `varying vec3 vDir; void main(){ vDir = normalize(position); gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }`,
  fragmentShader: `
    varying vec3 vDir; uniform float uTime;
    float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898,78.233))) * 43758.5453); }
    float noise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
      return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y); }
    float fbm(vec2 p){ float s=0.0,a=0.5; for(int i=0;i<5;i++){ s+=a*noise(p); p*=2.03; a*=0.5;} return s; }
    void main(){
      vec3 d = normalize(vDir);
      float h = d.y;
      vec3 zenith = vec3(0.03,0.05,0.16), mid = vec3(0.32,0.16,0.32), horizon = vec3(1.25,0.48,0.2);
      vec3 col = mix(horizon, mid, smoothstep(0.0, 0.18, h));
      col = mix(col, zenith, smoothstep(0.15, 0.7, h));
      vec3 sunDir = normalize(vec3(0.85, 0.05, -1.0));
      float mu = max(dot(d, sunDir), 0.0);
      col += vec3(1.8,0.75,0.3) * pow(mu, 32.0) * 0.35 + vec3(1.6,1.0,0.6) * smoothstep(0.99955, 0.9998, mu);
      // Wispy clouds lit from below.
      vec2 uv = d.xz / max(d.y + 0.06, 0.02) * 1.4 + vec2(uTime * 0.01, 0.0);
      float c = smoothstep(0.52, 0.85, fbm(uv));
      col = mix(col, vec3(1.1,0.45,0.35) * (0.4 + pow(mu,4.0)), c * smoothstep(0.0, 0.08, h) * 0.75);
      if (h < 0.0) col = horizon * 0.25;
      gl_FragColor = vec4(col, 1.0);
    }`,
});
scene.add(new THREE.Mesh(new THREE.SphereGeometry(1500, 48, 24), skyMat));

// --- Skyline (instanced towers with lit windows) -----------------------------
function windowTexture() {
  const c = document.createElement("canvas");
  c.width = 64; c.height = 256;
  const g = c.getContext("2d");
  g.fillStyle = "#07080c"; g.fillRect(0, 0, 64, 256);
  for (let y = 4; y < 256; y += 6)
    for (let x = 3; x < 64; x += 6) {
      const r = Math.random();
      g.fillStyle = r > 0.72 ? `rgba(255,${180 + (Math.random() * 60) | 0},${110 + (Math.random() * 60) | 0},1)` : r > 0.6 ? "#1b2a3a" : "#0b0d12";
      g.fillRect(x, y, 3, 3);
    }
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.magFilter = THREE.NearestFilter;
  return t;
}
const towerTex = windowTexture();
const towerMat = new THREE.MeshStandardMaterial({ color: 0x0c0d12, roughness: 0.6, metalness: 0.2, emissive: 0xffffff, emissiveMap: towerTex, emissiveIntensity: 1.6 });
const towerCount = 220;
const towers = new THREE.InstancedMesh(new THREE.BoxGeometry(1, 1, 1), towerMat, towerCount);
const m4 = new THREE.Matrix4();
for (let i = 0; i < towerCount; i++) {
  const x = (Math.random() - 0.5) * 900;
  const center = Math.exp(-(x * x) / (2 * 180 * 180));
  const h = 12 + Math.random() * 40 + center * (60 + Math.random() * 160);
  const w = 8 + Math.random() * 18;
  const z = -380 - Math.random() * 220;
  m4.compose(new THREE.Vector3(x, h / 2 - 2, z), new THREE.Quaternion(), new THREE.Vector3(w, h, w * (0.7 + Math.random() * 0.6)));
  towers.setMatrixAt(i, m4);
}
scene.add(towers);

// --- Glossy sea (reflects the sunset environment) ---------------------------------
const seaMat = new THREE.MeshStandardMaterial({ color: 0x07070c, roughness: 0.16, metalness: 0.6, envMapIntensity: 0.7 });
seaMat.onBeforeCompile = (sh) => {
  sh.uniforms.uTime = { value: 0 };
  seaMat.userData.shader = sh;
  sh.vertexShader = "varying vec3 vWorldP;\n" + sh.vertexShader.replace("#include <worldpos_vertex>", "#include <worldpos_vertex>\n vWorldP = (modelMatrix * vec4(transformed,1.0)).xyz;");
  sh.fragmentShader = "uniform float uTime; varying vec3 vWorldP;\n" + sh.fragmentShader.replace("#include <normal_fragment_maps>",
    "#include <normal_fragment_maps>\n vec2 wp = vWorldP.xz * 0.35; normal = normalize(normal + vec3(sin(wp.x * 2.0 + uTime) * 0.04 + sin(wp.y * 3.1 - uTime * 1.3) * 0.03, 0.0, cos(wp.y * 2.4 + uTime * 0.8) * 0.04));");
};
const sea = new THREE.Mesh(new THREE.PlaneGeometry(4000, 4000), seaMat);
sea.rotation.x = -Math.PI / 2;
sea.position.y = -2;
scene.add(sea);

// --- Lights -----------------------------------------------------------------
scene.add(new THREE.HemisphereLight(0xff9a6a, 0x101020, 0.35));
const key = new THREE.DirectionalLight(0xffb070, 2.2);
key.position.set(30, 8, -60);
scene.add(key);
const sweep = new THREE.PointLight(0xffffff, 0, 30, 1.6);
scene.add(sweep);
const rim = new THREE.PointLight(0xff4f6d, 6, 60, 2);
rim.position.set(-14, 6, 6);
scene.add(rim);

// Environment for chrome reflections: PMREM of the sky + skyline.
const pmrem = new THREE.PMREMGenerator(renderer);
const envScene = new THREE.Scene();
envScene.add(new THREE.Mesh(new THREE.SphereGeometry(100, 32, 16), skyMat.clone()));
scene.environment = pmrem.fromScene(envScene, 0.02).texture;

// --- Embers -------------------------------------------------------------------
const emberCount = 900;
const emberGeo = new THREE.BufferGeometry();
const emberPos = new Float32Array(emberCount * 3);
const emberSeed = new Float32Array(emberCount);
for (let i = 0; i < emberCount; i++) {
  emberPos[i * 3] = (Math.random() - 0.5) * 120;
  emberPos[i * 3 + 1] = Math.random() * 40 - 2;
  emberPos[i * 3 + 2] = (Math.random() - 0.5) * 120 - 20;
  emberSeed[i] = Math.random();
}
emberGeo.setAttribute("position", new THREE.BufferAttribute(emberPos, 3));
emberGeo.setAttribute("seed", new THREE.BufferAttribute(emberSeed, 1));
const emberMat = new THREE.ShaderMaterial({
  transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
  uniforms: { uTime: { value: 0 }, uFade: { value: 0 } },
  vertexShader: `attribute float seed; uniform float uTime; varying float vA;
    void main(){ vec3 p = position; p.y += mod(uTime * (0.6 + seed) + seed * 40.0, 42.0) - 2.0;
      p.x += sin(uTime * 0.4 + seed * 30.0) * 1.5;
      vec4 mv = modelViewMatrix * vec4(p,1.0); gl_Position = projectionMatrix * mv;
      gl_PointSize = (1.5 + seed * 3.0) * (60.0 / -mv.z); vA = 0.4 + 0.6 * sin(uTime * 3.0 + seed * 20.0); }`,
  fragmentShader: `uniform float uFade; varying float vA;
    void main(){ float d = length(gl_PointCoord - 0.5); float a = smoothstep(0.5, 0.0, d) * vA * uFade;
      gl_FragColor = vec4(vec3(1.0,0.6,0.3) * a, a); }`,
});
scene.add(new THREE.Points(emberGeo, emberMat));

// --- Title ------------------------------------------------------------------
const letters = [];
const titleGroup = new THREE.Group();
scene.add(titleGroup);
const chrome = new THREE.MeshPhysicalMaterial({ color: 0xe8e4dc, metalness: 1.0, roughness: 0.2, clearcoat: 1.0, clearcoatRoughness: 0.06, envMapIntensity: 1.0 });
const glowEdge = new THREE.MeshBasicMaterial({ color: 0xffb347 });

new FontLoader().load("https://cdn.jsdelivr.net/npm/three@0.160.0/examples/fonts/helvetiker_bold.typeface.json", (font) => {
  const lines = ["GAME IS", "REALISTIC"];
  const size = [2.2, 3.4];
  const lineY = [4.3, -0.4];
  lines.forEach((text, li) => {
    const scale = size[li] / font.data.resolution;
    let width = 0;
    for (const ch of text) width += (font.data.glyphs[ch] ? font.data.glyphs[ch].ha : font.data.resolution * 0.5) * scale * 1.06;
    let x = -width / 2;
    for (const ch of text) {
      const adv = (font.data.glyphs[ch] ? font.data.glyphs[ch].ha : font.data.resolution * 0.5) * scale * 1.06;
      if (ch !== " ") {
        const geo = new TextGeometry(ch, { font, size: size[li], height: size[li] * 0.28, depth: size[li] * 0.28, curveSegments: 6,
          bevelEnabled: true, bevelThickness: size[li] * 0.04, bevelSize: size[li] * 0.03, bevelSegments: 3 });
        geo.computeBoundingBox();
        const mesh = new THREE.Mesh(geo, [chrome, chrome]);
        const edge = new THREE.Mesh(new THREE.BoxGeometry(adv * 0.8, 0.05, 0.05), glowEdge);
        edge.position.set(adv * 0.4, -0.18, size[li] * 0.14);
        mesh.add(edge);
        mesh.userData = { target: new THREE.Vector3(x, lineY[li], 0), delay: 0.4 + letters.length * 0.11 };
        mesh.position.set(x, lineY[li] - 6, -4);
        mesh.rotation.set(-1.2, 0.4, 0);
        mesh.visible = false;
        titleGroup.add(mesh);
        letters.push(mesh);
      }
      x += adv;
    }
  });
});

// --- Post processing ---------------------------------------------------------
const composer = new EffectComposer(renderer);
composer.addPass(new RenderPass(scene, camera));
const bloom = new UnrealBloomPass(new THREE.Vector2(window.innerWidth, window.innerHeight), 0.55, 0.5, 0.9);
composer.addPass(bloom);
composer.addPass(new OutputPass());

// --- Timeline -----------------------------------------------------------------
const clock = new THREE.Clock();
let readySent = false;
let paused = false;
const ease = (t) => 1 - Math.pow(1 - Math.min(Math.max(t, 0), 1), 3);

function frame() {
  if (paused) return;
  requestAnimationFrame(frame);
  const t = clock.getElapsedTime();
  skyMat.uniforms.uTime.value = t;
  if (seaMat.userData.shader) seaMat.userData.shader.uniforms.uTime.value = t;
  emberMat.uniforms.uTime.value = t;
  emberMat.uniforms.uFade.value = ease(t / 3);
  renderer.toneMappingExposure = 0.85 * ease(t / 2.2);

  // Camera: low dolly over the water towards the title, then a slow handheld drift.
  const k = ease(t / 7);
  camera.position.set(Math.sin(t * 0.13) * 2.5 * k + (1 - k) * -6, 1.0 + k * 1.4 + Math.sin(t * 0.7) * 0.05, 46 - k * 16);
  camera.lookAt(Math.sin(t * 0.1) * 0.6, 1.4, -2);

  for (const m of letters) {
    const lt = (t - m.userData.delay) / 1.3;
    if (lt <= 0) continue;
    m.visible = true;
    const e = ease(lt);
    m.position.lerpVectors(new THREE.Vector3(m.userData.target.x, m.userData.target.y - 6, -4), m.userData.target, e);
    m.rotation.set(-1.2 * (1 - e), 0.4 * (1 - e), 0);
  }
  // Light sweep across the letters after they land.
  const st = (t - 3.4) / 2.2;
  if (st > 0 && st < 1.2) {
    sweep.intensity = 22 * Math.sin(Math.min(st, 1) * Math.PI);
    sweep.position.set(-14 + st * 28, 1.6, 4);
  } else sweep.intensity = 0;
  titleGroup.position.y = Math.sin(t * 0.6) * 0.06;

  if (!readySent && t > 5.2) {
    readySent = true;
    document.getElementById("title-overlay").classList.remove("hidden");
    window.dispatchEvent(new Event("intro-ready"));
  }
  composer.render();
}
frame();

window.addEventListener("resize", () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
  composer.setSize(window.innerWidth, window.innerHeight);
});
// The intro pauses while the game stream runs (saves the GPU of the viewer's device).
window.addEventListener("intro-pause", () => { paused = true; });
window.addEventListener("intro-resume", () => { if (paused) { paused = false; frame(); } });
