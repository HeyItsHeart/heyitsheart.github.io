#pragma once
// ─────────────────────────────────────────────
//  STARSHINE ENGINE — WebGL2 Renderer
//
//  Replaces Nintendo's GX (GameCube) and GX2 (Wii)
//  graphics pipeline.
//
//  Architecture:
//    GX TEV stages     → GLSL fragment shader uniforms
//    GX display lists  → VAO/VBO static batches
//    GX texture env    → PBR-lite material system
//    GX indirect tex   → multi-pass render graph
//
//  The renderer is called from the WASM module via
//  Emscripten's OpenGL ES 3.0 bindings, which map
//  directly to WebGL2 in the browser.
//
//  Shader pipeline:
//    BMD/BRRES geometry → Mesh → RenderBatch
//    Material (TEV approx) → ShaderProgram → draw call
// ─────────────────────────────────────────────

#include "../core_types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// These are OpenGL ES 3.0 types (= WebGL2 via Emscripten)
using GLuint   = unsigned int;
using GLint    = int;
using GLenum   = unsigned int;
using GLsizei  = int;

namespace Starshine {
namespace Renderer {

// ── Vertex layout ──────────────────────────────
struct Vertex {
    Vec3 pos;
    Vec3 normal;
    Vec2 uv0;
    Vec2 uv1;         // lightmap / secondary UV
    u8   color[4];    // vertex color RGBA
};

// ── GPU mesh ────────────────────────────────────
struct GpuMesh {
    GLuint vao    = 0;
    GLuint vbo    = 0;
    GLuint ebo    = 0;
    u32    indexCount = 0;
    GLenum primitive  = 0x0004; // GL_TRIANGLES
    AABB   bounds;

    // Upload to GPU — call once after CPU-side build
    void upload(const std::vector<Vertex>& verts,
                const std::vector<u32>&    indices);
    void draw() const;
    void destroy();
};

// ── Texture ─────────────────────────────────────
struct GpuTexture {
    GLuint id     = 0;
    u32    width  = 0;
    u32    height = 0;
    std::string name;

    // Upload RGBA8 pixel data
    void upload(const u8* pixels, u32 w, u32 h, bool mipmap = true);
    void bind(u32 slot = 0) const;
    void destroy();
};

// ── Material (approximates GX TEV) ──────────────
// Full TEV emulation in GLSL would be massive.
// We use a structured approximation that covers 95%
// of Sunshine/Galaxy visual cases.
enum class MaterialMode {
    Opaque,
    AlphaTest,
    Translucent,
};

struct Material {
    std::string    name;
    MaterialMode   mode        = MaterialMode::Opaque;
    GpuTexture*    texture0    = nullptr;  // diffuse / color
    GpuTexture*    texture1    = nullptr;  // environment map / lightmap
    Vec4           baseColor   = {1,1,1,1};
    f32            alphaRef    = 0.5f;     // for alpha test
    bool           doubleSided = false;
    bool           useVertexColor = true;

    // TEV color combiner approximation parameters
    // These are set from the original material data
    f32  tevColorScale = 1.f;
    f32  tevAlphaScale = 1.f;
    bool hasEnvMap     = false;
    f32  envMapStrength = 0.3f;

    // GX fog params
    bool  fogEnabled  = false;
    f32   fogStart    = 500.f;
    f32   fogEnd      = 2000.f;
    Vec3  fogColor    = {0.5f,0.7f,1.f};
};

// ── Shader program ──────────────────────────────
class ShaderProgram {
public:
    GLuint id = 0;

    bool compile(const char* vertSrc, const char* fragSrc);
    void use() const;
    void destroy();

    // Uniform setters
    void setMat4(const char* name, const Mat4& m) const;
    void setVec3(const char* name, const Vec3& v) const;
    void setVec4(const char* name, const Vec4& v) const;
    void setFloat(const char* name, f32 v) const;
    void setInt(const char* name, s32 v) const;

private:
    std::unordered_map<std::string, GLint> m_uniformCache;
    GLint getLoc(const char* name) const;
};

// ── Render command ──────────────────────────────
struct RenderCmd {
    GpuMesh*      mesh;
    Material*     material;
    Mat4          modelMatrix;
    f32           depth;       // for sorting transparent geometry
};

// ── Main renderer ───────────────────────────────
class Renderer {
public:
    bool init(u32 viewportW, u32 viewportH);
    void shutdown();
    void resize(u32 w, u32 h);

    // ── Frame lifecycle ─────────────────────────
    void beginFrame();
    void endFrame();

    // ── Camera ─────────────────────────────────
    void setCamera(const Vec3& eye, const Vec3& center, const Vec3& up,
                   f32 fovY = 60.f * kDegToRad);

    // ── Submit draw calls ───────────────────────
    void submit(GpuMesh* mesh, Material* mat, const Mat4& model);

    // ── Direct fullscreen quad ──────────────────
    // Used for skybox, post-process, UI
    void drawFullscreenQuad(GpuTexture* tex);

    // ── Skybox ──────────────────────────────────
    void drawSkybox(GpuTexture* cubemap);

    // ── Debug wireframe ─────────────────────────
    void drawAABB(const AABB& box, const Vec3& color = {1,1,0});
    void drawSphere(const Vec3& center, f32 radius, const Vec3& color = {1,1,0});
    void drawLine(const Vec3& a, const Vec3& b, const Vec3& color = {1,1,1});

    // ── Getters ─────────────────────────────────
    u32 viewportW() const { return m_vpW; }
    u32 viewportH() const { return m_vpH; }
    u64 drawCallCount() const { return m_drawCalls; }

    // ── Resource creation ───────────────────────
    std::shared_ptr<GpuTexture> createTexture(const char* name);
    std::shared_ptr<GpuMesh>    createMesh(const char* name);
    std::shared_ptr<Material>   createMaterial(const char* name);

private:
    u32  m_vpW = 0, m_vpH = 0;
    Mat4 m_viewMatrix;
    Mat4 m_projMatrix;
    Vec3 m_cameraPos;

    // Opaque + transparent command lists (sorted separately)
    std::vector<RenderCmd> m_opaqueList;
    std::vector<RenderCmd> m_transparentList;

    // Built-in shaders
    std::unique_ptr<ShaderProgram> m_opaqueShader;
    std::unique_ptr<ShaderProgram> m_alphaTestShader;
    std::unique_ptr<ShaderProgram> m_translucentShader;
    std::unique_ptr<ShaderProgram> m_debugShader;
    std::unique_ptr<ShaderProgram> m_skyboxShader;

    // Debug mesh (lazy-built sphere/cube for debug rendering)
    std::unique_ptr<GpuMesh> m_debugCube;
    std::unique_ptr<GpuMesh> m_fullscreenQuad;

    u64 m_drawCalls = 0;

    void flushOpaque();
    void flushTransparent();
    void buildBuiltinShaders();
    void buildDebugMeshes();
};

// ── Built-in GLSL sources ─────────────────────
// Approximates GX TEV combiner for Sunshine/Galaxy materials
namespace Shaders {

constexpr const char* kOpaqueVert = R"(#version 300 es
precision highp float;

layout(location=0) in vec3 a_pos;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec2 a_uv0;
layout(location=3) in vec2 a_uv1;
layout(location=4) in vec4 a_color;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;
uniform mat3 u_normalMat;

out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_uv0;
out vec2 v_uv1;
out vec4 v_color;

void main() {
    vec4 worldPos = u_model * vec4(a_pos, 1.0);
    v_worldPos = worldPos.xyz;
    v_normal   = normalize(u_normalMat * a_normal);
    v_uv0      = a_uv0;
    v_uv1      = a_uv1;
    v_color    = a_color;
    gl_Position = u_proj * u_view * worldPos;
}
)";

constexpr const char* kOpaqueFrag = R"(#version 300 es
precision highp float;

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_uv0;
in vec2 v_uv1;
in vec4 v_color;

uniform sampler2D u_tex0;
uniform sampler2D u_tex1;
uniform vec4  u_baseColor;
uniform float u_tevColorScale;
uniform float u_tevAlphaScale;
uniform bool  u_useVertexColor;
uniform bool  u_hasEnvMap;
uniform float u_envMapStrength;
uniform vec3  u_camPos;
// Lighting
uniform vec3  u_sunDir;
uniform vec3  u_sunColor;
uniform vec3  u_ambientColor;
// Fog
uniform bool  u_fogEnabled;
uniform float u_fogStart;
uniform float u_fogEnd;
uniform vec3  u_fogColor;

out vec4 fragColor;

void main() {
    // Texture sample
    vec4 texColor = texture(u_tex0, v_uv0);

    // Vertex color blend
    vec4 baseCol = u_baseColor;
    if(u_useVertexColor) baseCol *= v_color;

    // TEV approximation: multiply texture with base color
    vec4 col = texColor * baseCol * u_tevColorScale;

    // Environment map (sphere map)
    if(u_hasEnvMap) {
        vec3 viewDir   = normalize(v_worldPos - u_camPos);
        vec3 reflected = reflect(viewDir, normalize(v_normal));
        // Sphere map UV from reflected vector
        vec2 envUV = reflected.xy * 0.5 + 0.5;
        vec4 envCol = texture(u_tex1, envUV);
        col.rgb = mix(col.rgb, envCol.rgb, u_envMapStrength);
    }

    // Diffuse lighting (simple directional)
    float NdotL = max(dot(normalize(v_normal), -u_sunDir), 0.0);
    vec3  light  = u_ambientColor + u_sunColor * NdotL;
    col.rgb *= light;

    // Fog
    if(u_fogEnabled) {
        float dist = length(v_worldPos - u_camPos);
        float fogFactor = clamp((dist - u_fogStart)/(u_fogEnd - u_fogStart), 0.0, 1.0);
        col.rgb = mix(col.rgb, u_fogColor, fogFactor);
    }

    col.a *= u_tevAlphaScale;
    fragColor = col;
}
)";

constexpr const char* kAlphaTestFrag = R"(#version 300 es
precision highp float;
// Same as opaque but with alpha discard
in vec2 v_uv0;
in vec4 v_color;
uniform sampler2D u_tex0;
uniform vec4  u_baseColor;
uniform float u_alphaRef;
uniform bool  u_useVertexColor;
out vec4 fragColor;
void main() {
    vec4 col = texture(u_tex0, v_uv0) * u_baseColor;
    if(u_useVertexColor) col *= v_color;
    if(col.a < u_alphaRef) discard;
    fragColor = col;
}
)";

} // namespace Shaders

} // namespace Renderer
} // namespace Starshine
