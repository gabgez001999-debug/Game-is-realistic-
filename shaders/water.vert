#include "common.glsl"

layout(location = 0) in vec3 aPos;

uniform vec4 uWaves[6];  // xy = direction, z = wavelength, w = steepness

out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vClip;

void main() {
  vec3 p = aPos + vec3(uCamPos.x - mod(uCamPos.x, 4.0), 0.0, uCamPos.z - mod(uCamPos.z, 4.0));
  vec3 disp = vec3(0.0);
  vec3 tangent = vec3(1, 0, 0), binormal = vec3(0, 0, 1);
  float fade = 1.0 - smoothstep(600.0, 2500.0, length(p.xz - uCamPos.xz));
  for (int i = 0; i < 6; ++i) {
    vec2 d = normalize(uWaves[i].xy);
    float L = uWaves[i].z;
    float k = 2.0 * PI / L;
    float c = sqrt(9.81 / k);
    float a = uWaves[i].w / k * fade;
    float f = k * (dot(d, p.xz) - c * uTime);
    disp.x += d.x * a * cos(f);
    disp.z += d.y * a * cos(f);
    disp.y += a * sin(f);
    tangent += vec3(-d.x * d.x * uWaves[i].w * sin(f), d.x * uWaves[i].w * cos(f), -d.x * d.y * uWaves[i].w * sin(f)) * fade;
    binormal += vec3(-d.x * d.y * uWaves[i].w * sin(f), d.y * uWaves[i].w * cos(f), -d.y * d.y * uWaves[i].w * sin(f)) * fade;
  }
  vec3 wp = p + disp;
  vWorldPos = wp;
  vNormal = normalize(cross(binormal, tangent));
  vClip = uViewProj * vec4(wp, 1.0);
  gl_Position = vClip;
}
