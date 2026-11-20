// Game-is-realistic - entry point.
//
//   realistic                         play in a window (SDL2, OpenGL 4.1)
//   realistic --shot out.png [opts]   render one headless screenshot and exit
//   realistic --stream [opts]         cloud mode: headless GPU rendering + H.264 stream
//                                     to the browser over WebSocket (see web/ and cloud/)
//
// Options: --width N --height N --time HOURS --view N --port N --fps N --bitrate KBPS
//          --token STR --idle-exit SECONDS --web DIR --encoder NAME --fullscreen
//          --lazy-start  (stream mode: serve the website, launch the game when a player presses START)
#include <chrono>
#include <cstring>
#include <thread>

#include "Game.h"
#include "core/FileSystem.h"
#if defined(VERITAS_HAS_STREAMING)
#include "stream/StreamSession.h"
#endif

using namespace vx;

struct Args {
  bool stream = false, fullscreen = false, lazyStart = false;
  std::string shot;
  int width = 0, height = 0, view = -1, port = 8080, fps = 60, bitrate = 0, frames = 3;
  float time = 16.5f, idleExit = 0.f;
  std::string token, web, encoder;
};

static Args parseArgs(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    std::string s = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (s == "--stream") a.stream = true;
    else if (s == "--lazy-start") a.lazyStart = true;
    else if (s == "--fullscreen") a.fullscreen = true;
    else if (s == "--shot") a.shot = next();
    else if (s == "--width") a.width = std::stoi(next());
    else if (s == "--height") a.height = std::stoi(next());
    else if (s == "--time") a.time = std::stof(next());
    else if (s == "--view") a.view = std::stoi(next());
    else if (s == "--frames") a.frames = std::stoi(next());
    else if (s == "--port") a.port = std::stoi(next());
    else if (s == "--fps") a.fps = std::stoi(next());
    else if (s == "--bitrate") a.bitrate = std::stoi(next());
    else if (s == "--token") a.token = next();
    else if (s == "--idle-exit") a.idleExit = std::stof(next());
    else if (s == "--web") a.web = next();
    else if (s == "--encoder") a.encoder = next();
  }
  return a;
}

static void applyView(game::Game& g, int view) {
  struct V { vec3 pos; float yaw, pitch; };
  const V views[] = {
      {{9.f, 3.8f, 50.f}, 0.1f, 0.02f},
      {{-200.f, 120.f, 520.f}, -0.38f, -0.2f},
      {{30.f, 2.4f, 498.f}, -1.2f, 0.0f},
      {{120.f, 3.f, 7.f}, -1.57f, 0.03f},
  };
  if (view >= 0 && view < 4) g.setCameraPose(views[view].pos, views[view].yaw, views[view].pitch);
}

int main(int argc, char** argv) {
  setExecutablePath(argv[0]);
  Args args = parseArgs(argc, argv);
  bool headless = args.stream || !args.shot.empty();

  PlatformConfig pc;
  pc.width = args.width ? args.width : (headless ? 1280 : 1600);
  pc.height = args.height ? args.height : (headless ? 720 : 900);
  pc.fullscreen = args.fullscreen;
  std::unique_ptr<Platform> platform;
#if defined(VERITAS_HAS_STREAMING)
  if (headless) platform = createHeadlessPlatform();
#endif
  if (!platform) {
    if (headless) { logError("Headless mode needs the Linux streaming build"); return 1; }
    platform = createSdlPlatform();
  }
  if (!platform->init(pc)) return 1;

  game::GameOptions opt;
  opt.timeOfDay = args.time;
  std::unique_ptr<game::Game> g;
  auto launchGame = [&]() {
    g = std::make_unique<game::Game>();
    if (!g->init(*platform, opt)) return false;
    applyView(*g, args.view);
    return true;
  };
  bool lazy = args.lazyStart && args.stream;
  if (!lazy && !launchGame()) return 1;

  // --- Screenshot mode -------------------------------------------------------
  if (!args.shot.empty()) {
    Input in;
    for (int f = 0; f < args.frames; ++f) {
      g->update(in, 1.f / 60.f);
      g->render(platform->outputFramebuffer(), platform->width(), platform->height());
    }
    g->renderer().screenshot(platform->outputFramebuffer(), platform->width(), platform->height(), args.shot);
    return 0;
  }

  // --- Cloud streaming mode --------------------------------------------------
#if defined(VERITAS_HAS_STREAMING)
  std::unique_ptr<StreamSession> stream;
  if (args.stream) {
    StreamConfig sc;
    sc.port = args.port;
    sc.width = platform->width();
    sc.height = platform->height();
    sc.fps = args.fps;
    sc.bitrateKbps = args.bitrate ? args.bitrate : (sc.width * sc.height >= 1920 * 1080 ? 16000 : 9000);
    sc.token = args.token;
    sc.idleExitSeconds = args.idleExit;
    sc.encoder = args.encoder;
    sc.webRoot = args.web.empty() ? dataPath("web") : args.web;
    stream = std::make_unique<StreamSession>();
    if (!stream->start(sc, *platform, nullptr)) return 1;
  }
#endif

  // --- Main loop ----------------------------------------------------------------
  using clock = std::chrono::steady_clock;
  auto last = clock::now();
  Input in;
  const double frameBudget = 1.0 / double(std::max(args.fps, 10));
  while (true) {
    auto start = clock::now();
    float dt = std::chrono::duration<float>(start - last).count();
    last = start;
    dt = clampf(dt, 1e-4f, 0.1f);
    platform->pollEvents(in);
    if (in.quit) break;
#if defined(VERITAS_HAS_STREAMING)
    if (stream) {
      if (stream->shouldExit()) { logInfo("No player connected - shutting down"); break; }
      if (!g) {
        // Lobby: only the website is served until a player presses START.
        if (!stream->clientConnected()) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); last = clock::now(); continue; }
        logInfo("Player pressed START - launching the game");
        if (!launchGame()) return 1;
        last = clock::now();
        continue;
      }
      char info[96];
      std::snprintf(info, sizeof(info), "CLOUD %.1f MBIT/S %s", double(stream->bitrateMbps()), stream->clientConnected() ? "VERBUNDEN" : "WARTET");
      g->setStreamInfo(info);
    }
#endif
    g->update(in, dt);
    g->render(platform->outputFramebuffer(), platform->width(), platform->height());
#if defined(VERITAS_HAS_STREAMING)
    if (stream) stream->captureFrame(platform->outputTexture());
#endif
    platform->present();
    if (platform->isHeadless()) {
      double spent = std::chrono::duration<double>(clock::now() - start).count();
      if (spent < frameBudget) std::this_thread::sleep_for(std::chrono::duration<double>(frameBudget - spent));
    }
  }
  return 0;
}
