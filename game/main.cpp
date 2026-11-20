// Temporary renderer smoke test (replaced by the full game entry point).
#include "core/FileSystem.h"
#include "core/Noise.h"
#include "gfx/MeshBuilder.h"
#include "gfx/Renderer.h"
#include "platform/Platform.h"

using namespace vx;

int main(int argc, char** argv) {
  setExecutablePath(argv[0]);
  PlatformConfig pc;
  pc.width = 960; pc.height = 540;
  auto platform = createHeadlessPlatform();
  if (!platform->init(pc)) return 1;
  Renderer r;
  if (!r.init(pc.width, pc.height)) return 1;
  Hud hud; hud.init();
  MeshData md;
  addHeightGrid(md, vec2(-400), vec2(800), 200, 200, [](float x, float z) { return fbm2(x * 0.004f, z * 0.004f, 5) * 30.f + 6.f; },
                surf(MAT_TERRAIN, 0x808080));
  md.computeNormals();
  Mesh terrain; terrain.upload(md);
  MeshData b;
  Surface fs = surf(MAT_FACADE, 0xffffff);
  for (int i = 0; i < 6; ++i) {
    fs.extra = vec4(float(i) * 3.7f, float(i % 5), 3.4f, 2.4f);
    vec3 org(-10.f, -20.f - float(i) * 6.f, -10.f);
    addBox(b, vec3(float(i) * 26.f - 60.f, 20.f + float(i) * 6.f, -60.f), vec3(10.f, 20.f + float(i) * 6.f, 10.f), fs, mat3(1.f));
  }
  Mesh bm; bm.upload(b);
  MeshData a;
  addBox(a, vec3(0, 0, 0), vec3(200, 0.1f, 200), surf(MAT_ASPHALT, 0x202020));
  Mesh am; am.upload(a);
  Environment env; env.timeOfDay = 17.0f; env.update(0.f);
  Camera cam; cam.setPerspective(glm::radians(60.f), float(pc.width) / float(pc.height));
  cam.lookAt(vec3(10, 8, 40), vec3(-20, 15, -60));
  r.setCity(vec4(100.f, 7.5f, 4.5f, 500.f));
  r.setWater(true, 0.f);
  for (int f = 0; f < 3; ++f) {
    r.beginFrame(cam, env, 1.f + f * 0.016f, 0.016f);
    DrawItem t; t.mesh = &terrain; r.submit(t);
    DrawItem bi; bi.mesh = &bm; r.submit(bi);
    DrawItem ai; ai.mesh = &am; r.submit(ai);
    hud.begin(pc.width, pc.height);
    hud.text(20, 20, 28, "GAME IS REALISTIC 123", vec4(1));
    r.render(platform->outputFramebuffer(), pc.width, pc.height, &hud);
  }
  r.screenshot(platform->outputFramebuffer(), pc.width, pc.height, argc > 1 ? argv[1] : "shot.png");
  return 0;
}
