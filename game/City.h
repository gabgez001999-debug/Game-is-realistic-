#pragma once
#include "gfx/Mesh.h"
#include "gfx/Renderer.h"
#include "physics/PhysicsWorld.h"

namespace game {
using namespace vx;

// City layout constants (shared with shaders through uCity).
struct CityLayout {
  float pitch = 120.f;       // block pitch (road centre to road centre)
  float roadHalf = 7.5f;     // half road width, curb to curb (4 lanes x 3.5 m + gutters)
  float sidewalk = 4.5f;
  int roadsPerSide = 4;      // roads at k*pitch for k in [-4, 4]
  float groundY = 2.0f;      // street level
  float curbH = 0.15f;
  float extent() const { return float(roadsPerSide) * pitch; }
  float laneOffset(int lane) const { return lane == 0 ? 1.75f : 5.25f; }
};

enum class PropType : uint8_t { StreetLight, TrafficLight, Hydrant, Bin, Bench, Palm, Bollard };

struct Prop {
  PropType type;
  RigidBody body;
  const Mesh* mesh = nullptr;
  vec3 meshOffset{0.f};     // mesh origin relative to body centre (local)
  float health = 1.f;
  bool broken = false;      // knocked over (light off)
  int intersection = -1;    // traffic lights
  int facing = 0;           // traffic lights: which approach they face (0 N, 1 E, 2 S, 3 W)
  mat4 prevModel{1.f};
};

struct Intersection {
  vec2 pos;
  float phaseOffset;
};

class City {
 public:
  CityLayout L;
  StaticWorld statics;
  std::vector<std::unique_ptr<Mesh>> chunks;      // static batched geometry
  std::vector<bool> chunkShadows;
  std::vector<std::unique_ptr<Mesh>> propMeshes;  // shared prop meshes
  std::vector<std::unique_ptr<Prop>> props;
  std::vector<Intersection> intersections;
  std::vector<vec3> parkingSpots;                 // xyz + yaw packed in parkingYaw
  std::vector<float> parkingYaw;
  std::vector<float> heightMap;
  int heightMapRes = 0;
  vec2 heightMapOrigin{0.f}, heightMapSize{0.f};
  vec3 spawnPoint{0.f};
  float spawnYaw = 0.f;

  void generate(uint32_t seed, PhysicsWorld& physics);
  void submit(Renderer& r, const vec3& camPos, float night, float alpha);
  void updateProps(float dt);
  vec4 shaderParams() const { return vec4(L.pitch, L.roadHalf, L.sidewalk, L.extent()); }

  // Traffic signal state for an approach direction: 0 = green, 1 = yellow, 2 = red.
  int signalState(int intersection, bool northSouth, float time) const;
  int nearestIntersection(const vec2& p) const;
  bool onRoad(const vec2& p) const;
  float terrainHeight(float x, float z) const;
  void setTime(float t) { time_ = t; }

  struct PalmInstance { vec3 pos; float yaw; int variant; };
  std::vector<PalmInstance> palms_;

 private:
  void buildTerrain(uint32_t seed);
  void buildBlocks(uint32_t seed, PhysicsWorld& physics);
  void buildPropMeshes();
  void addProp(PropType t, const vec3& base, float yaw, PhysicsWorld& physics, int inter = -1, int facing = 0);
  void buildHeightMap();
  Mesh* propMesh(PropType t, int variant = 0);
  std::vector<int> propMeshIndex_;
  std::vector<float> palmHeights_;
  int lensMeshes_[3]{};
  float time_ = 0.f;
};

}  // namespace game
