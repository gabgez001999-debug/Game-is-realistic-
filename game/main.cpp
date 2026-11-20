// Graphics test harness: generates the city and renders screenshots (replaced by the full game).
#include "City.h"
#include "core/FileSystem.h"
#include "gfx/Renderer.h"
#include "platform/Platform.h"

using namespace vx;

int main(int argc, char** argv) {
  setExecutablePath(argv[0]);
  std::string out = argc > 1 ? argv[1] : "shot";
  float tod = argc > 2 ? std::stof(argv[2]) : 15.f;
  int view = argc > 3 ? std::stoi(argv[3]) : 0;
  PlatformConfig pc;
  pc.width = 1280; pc.height = 720;
  auto platform = createHeadlessPlatform();
  if (!platform->init(pc)) return 1;
  Renderer r;
  if (!r.init(pc.width, pc.height)) return 1;
  PhysicsWorld physics;
  game::City city;
  city.generate(1337, physics);
  physics.statics = &city.statics;
  r.setCity(city.shaderParams());
  r.setHeightMap(city.heightMap, city.heightMapRes, city.heightMapRes, city.heightMapOrigin, city.heightMapSize);
  r.setWater(true, 0.f);
  Environment env;
  env.timeOfDay = tod;
  env.update(0.f);
  Camera cam;
  cam.setPerspective(glm::radians(55.f), float(pc.width) / float(pc.height));
  struct V { vec3 eye, at; };
  const V views[] = {
      {{9.f, 3.8f, 50.f}, {0.f, 6.f, -40.f}},          // street level looking north along x=0
      {{-200.f, 120.f, 520.f}, {0.f, 40.f, 0.f}},      // aerial from the south-west
      {{30.f, 2.4f, 498.f}, {200.f, 2.f, 600.f}},      // beach boulevard
      {{120.f, 3.f, 7.f}, {300.f, 10.f, -5.f}},        // street looking east
  };
  V v = views[std::clamp(view, 0, 3)];
  cam.lookAt(v.eye, v.at);
  for (int f = 0; f < 4; ++f) {
    r.beginFrame(cam, env, 10.f + float(f) / 60.f, 1.f / 60.f);
    city.submit(r, cam.pos, env.night, 1.f);
    r.render(platform->outputFramebuffer(), pc.width, pc.height, nullptr);
  }
  logInfo("draws %d shadow %d lights %d", r.stats.drawCalls, r.stats.shadowCalls, r.stats.lights);
  r.screenshot(platform->outputFramebuffer(), pc.width, pc.height, out + ".png");
  return 0;
}
