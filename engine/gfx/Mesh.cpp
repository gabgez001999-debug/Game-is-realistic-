#include "gfx/Mesh.h"

#include <cstddef>

namespace vx {

void MeshData::append(const MeshData& o, const mat4& m) {
  uint32_t base = uint32_t(v.size());
  mat3 nm = glm::transpose(glm::inverse(mat3(m)));
  for (const Vertex& s : o.v) {
    Vertex d = s;
    d.pos = vec3(m * vec4(s.pos, 1.f));
    d.normal = glm::normalize(nm * s.normal);
    v.push_back(d);
  }
  for (uint32_t idx : o.i) i.push_back(base + idx);
}

void MeshData::computeNormals() {
  for (auto& vv : v) vv.normal = vec3(0.f);
  for (size_t t = 0; t + 2 < i.size(); t += 3) {
    Vertex &a = v[i[t]], &b = v[i[t + 1]], &c = v[i[t + 2]];
    vec3 n = glm::cross(b.pos - a.pos, c.pos - a.pos);  // area weighted
    a.normal += n; b.normal += n; c.normal += n;
  }
  for (auto& vv : v) {
    float l = glm::length(vv.normal);
    vv.normal = l > 1e-12f ? vv.normal / l : vec3(0, 1, 0);
  }
}

AABB MeshData::bounds() const {
  AABB b;
  for (auto& vv : v) b.expand(vv.pos);
  return b;
}

Mesh::~Mesh() {
  if (ibo_) glDeleteBuffers(1, &ibo_);
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
}

void Mesh::upload(const MeshData& d, bool dynamic) {
  if (!vao_) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
  }
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(d.v.size() * sizeof(Vertex)), d.v.data(), dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(d.i.size() * sizeof(uint32_t)), d.i.data(), GL_STATIC_DRAW);
  const GLsizei st = sizeof(Vertex);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)offsetof(Vertex, pos));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, st, (void*)offsetof(Vertex, normal));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, st, (void*)offsetof(Vertex, uv));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)offsetof(Vertex, color));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_FALSE, st, (void*)offsetof(Vertex, mat));
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(Vertex, extra));
  glBindVertexArray(0);
  count_ = uint32_t(d.i.size());
  vcount_ = d.v.size();
  bounds_ = d.bounds();
}

void Mesh::updateVertices(const std::vector<Vertex>& v) {
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  if (v.size() == vcount_)
    glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(v.size() * sizeof(Vertex)), v.data());
  else
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(v.size() * sizeof(Vertex)), v.data(), GL_DYNAMIC_DRAW);
  vcount_ = v.size();
  bounds_ = AABB();
  for (auto& vv : v) bounds_.expand(vv.pos);
}

void Mesh::draw() const {
  if (!count_) return;
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, GLsizei(count_), GL_UNSIGNED_INT, nullptr);
}

}  // namespace vx
