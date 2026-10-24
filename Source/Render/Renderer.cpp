#include "Renderer.h"
#include "Shaders.h"
#include "Ornaments.h"
#include <cmath>
#include <cstring>

using namespace juce::gl;

namespace au
{
// ============================================================================ GlProgram
static unsigned compile (GLenum type, const char* src, juce::String& err)
{
    const GLuint s = glCreateShader (type);
    glShaderSource (s, 1, &src, nullptr);
    glCompileShader (s);
    GLint ok = 0; glGetShaderiv (s, GL_COMPILE_STATUS, &ok);
    if (! ok)
    {
        char log[4096] {}; glGetShaderInfoLog (s, sizeof log, nullptr, log);
        err << "Shader compile error: " << log << "\n";
        glDeleteShader (s);
        return 0;
    }
    return s;
}

bool GlProgram::build (const char* vs, const std::string& fs, const std::vector<std::pair<int, const char*>>& attribs, juce::String& err)
{
    const unsigned v = compile (GL_VERTEX_SHADER, vs, err);
    const unsigned f = compile (GL_FRAGMENT_SHADER, fs.c_str(), err);
    if (! v || ! f) return false;
    id = glCreateProgram();
    glAttachShader (id, v); glAttachShader (id, f);
    for (auto& a : attribs) glBindAttribLocation (id, (GLuint) a.first, a.second);
    glBindFragDataLocation (id, 0, "frag");
    glLinkProgram (id);
    glDeleteShader (v); glDeleteShader (f);
    GLint ok = 0; glGetProgramiv (id, GL_LINK_STATUS, &ok);
    if (! ok)
    {
        char log[4096] {}; glGetProgramInfoLog (id, sizeof log, nullptr, log);
        err << "Program link error: " << log << "\n";
        return false;
    }
    return true;
}
void GlProgram::use() const { glUseProgram (id); }
int GlProgram::loc (const char* n)
{
    auto it = cache.find (n);
    if (it != cache.end()) return it->second;
    const int l = glGetUniformLocation (id, n);
    cache[n] = l; return l;
}
void GlProgram::set (const char* n, float v) { glUniform1f (loc (n), v); }
void GlProgram::set (const char* n, float a, float b) { glUniform2f (loc (n), a, b); }
void GlProgram::set (const char* n, float a, float b, float c, float d) { glUniform4f (loc (n), a, b, c, d); }
void GlProgram::set (const char* n, int v) { glUniform1i (loc (n), v); }
void GlProgram::setArray (const char* n, const float* v, int c) { glUniform1fv (loc (n), c, v); }
void GlProgram::setMat4 (const char* n, const float* m) { glUniformMatrix4fv (loc (n), 1, GL_FALSE, m); }
void GlProgram::release() { if (id) glDeleteProgram (id); id = 0; cache.clear(); }

// ============================================================================ Fbo
void Fbo::ensure (int width, int height, bool floatFormat)
{
    if (fbo && w == width && h == height && hdr == floatFormat) return;
    release();
    w = width; h = height; hdr = floatFormat;
    glGenTextures (1, &tex);
    glBindTexture (GL_TEXTURE_2D, tex);
    glTexImage2D (GL_TEXTURE_2D, 0, hdr ? GL_RGBA16F : GL_RGBA8, w, h, 0, GL_RGBA, hdr ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers (1, &fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
}
void Fbo::release()
{
    if (fbo) glDeleteFramebuffers (1, &fbo);
    if (tex) glDeleteTextures (1, &tex);
    fbo = tex = 0; w = h = 0;
}

// ============================================================================ Renderer
Renderer::Renderer() = default;
Renderer::~Renderer() = default;

bool Renderer::init (juce::String& err)
{
    using namespace shaders;
    const char* bgs[5] = { bgLake, bgNebula, bgFlight, bgAurora, bgRetro };
    const char* sps[3] = { spPolar, spLine, spHex };
    bool ok = true;
    for (int i = 0; i < 5; ++i) ok &= bg[i].build (fullscreenVS, std::string (prelude) + bgs[i], {}, err);
    for (int i = 0; i < 3; ++i) ok &= sp[i].build (fullscreenVS, std::string (prelude) + sps[i], {}, err);
    ok &= ornament.build (fullscreenVS, ornamentFS, {}, err);
    ok &= particles.build (particleVS, particleFS, { { 0, "iPos" }, { 1, "iCol" } }, err);
    ok &= video.build (fullscreenVS, videoFS, {}, err);
    ok &= panel.build (fullscreenVS, panelFS, {}, err);
    ok &= bright.build (fullscreenVS, brightFS, {}, err);
    ok &= down.build (fullscreenVS, downFS, {}, err);
    ok &= up.build (fullscreenVS, upFS, {}, err);
    ok &= composite.build (fullscreenVS, compositeFS, {}, err);
    ok &= blit.build (fullscreenVS, blitFS, {}, err);
    if (! ok) return false;

    glGenVertexArrays (1, &emptyVao);
    glGenVertexArrays (1, &particleVao);
    glGenBuffers (1, &particleVbo);
    glBindVertexArray (particleVao);
    glBindBuffer (GL_ARRAY_BUFFER, particleVbo);
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 4, GL_FLOAT, GL_FALSE, sizeof (float) * 8, (void*) 0);
    glVertexAttribDivisor (0, 1);
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, sizeof (float) * 8, (void*) (sizeof (float) * 4));
    glVertexAttribDivisor (1, 1);
    glBindVertexArray (0);
    ready = true;
    return true;
}

void Renderer::release()
{
    for (auto& p : bg) p.release();
    for (auto& p : sp) p.release();
    for (auto* p : { &ornament, &particles, &video, &panel, &bright, &down, &up, &composite, &blit }) p->release();
    scene.release(); lum.release(); out.release();
    for (auto& b : bloom) b.release();
    if (emptyVao) glDeleteVertexArrays (1, &emptyVao);
    if (particleVao) glDeleteVertexArrays (1, &particleVao);
    if (particleVbo) glDeleteBuffers (1, &particleVbo);
    for (auto& t : ornTex) if (t) { glDeleteTextures (1, &t); t = 0; }
    if (videoTex) glDeleteTextures (1, &videoTex);
    emptyVao = particleVao = particleVbo = videoTex = 0; particleCapacity = 0;
    ready = false;
}

void Renderer::drawFullscreen()
{
    glBindVertexArray (emptyVao);
    glDrawArrays (GL_TRIANGLES, 0, 3);
}

void Renderer::setSound (GlProgram& p, const SoundFrame& sf, int w, int h)
{
    p.set ("uRes", (float) w, (float) h);
    p.set ("uTime", (float) std::fmod (sf.animTime, 3600.0));
    p.set ("uBass", sf.bass); p.set ("uMid", sf.mid); p.set ("uTreble", sf.treble);
    p.set ("uBeat", sf.beat); p.set ("uLevel", sf.level); p.set ("uBeatPhase", sf.beatPhase);
    p.setArray ("uBands", sf.bands, SoundFrame::kBands);
}

unsigned Renderer::ornamentTexture (int motif)
{
    motif = juce::jlimit (0, 2, motif);
    if (ornTex[motif]) return ornTex[motif];
    const int size = 1024;
    std::vector<uint8_t> px ((size_t) size * size);
    makeOrnamentMask (motif, size, px.data());
    glGenTextures (1, &ornTex[motif]);
    glBindTexture (GL_TEXTURE_2D, ornTex[motif]);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap (GL_TEXTURE_2D);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return ornTex[motif];
}

void Renderer::setVideoFrame (const uint8_t* bgra, int w, int h)
{
    if (! videoTex) glGenTextures (1, &videoTex);
    glBindTexture (GL_TEXTURE_2D, videoTex);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
    if (w != videoW || h != videoH)
    {
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, bgra);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        videoW = w; videoH = h;
    }
    else glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, bgra);
    hasVideo = true;
}

void Renderer::drawParticles (const std::vector<Vtx>& v, bool twoD, const float* view, float camDist, float sizeMul, bool soft, int w, int h)
{
    if (v.empty()) return;
    // pack into 8-float records (pos.xyz,size, rgb, pad)
    static thread_local std::vector<float> buf;
    buf.resize (v.size() * 8);
    for (size_t i = 0; i < v.size(); ++i)
    {
        float* d = &buf[i * 8];
        d[0] = v[i].x; d[1] = v[i].y; d[2] = v[i].z; d[3] = v[i].size;
        d[4] = v[i].r; d[5] = v[i].g; d[6] = v[i].b; d[7] = 0.f;
    }
    glBindBuffer (GL_ARRAY_BUFFER, particleVbo);
    if (v.size() > particleCapacity)
    {
        particleCapacity = v.size() + v.size() / 2;
        glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (particleCapacity * 8 * sizeof (float)), nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData (GL_ARRAY_BUFFER, 0, (GLsizeiptr) (buf.size() * sizeof (float)), buf.data());

    particles.use();
    particles.set ("uRes", (float) w, (float) h);
    particles.set ("u2D", twoD ? 1.f : 0.f);
    particles.set ("uCamDist", camDist);
    particles.set ("uSizeMul", sizeMul);
    particles.set ("uSoft", soft ? 1.f : 0.f);
    static const float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    particles.setMat4 ("uView", view ? view : ident);
    glBindVertexArray (particleVao);
    glDrawArraysInstanced (GL_TRIANGLE_STRIP, 0, 4, (GLsizei) v.size());
    glBindVertexArray (0);
}

static void lookAt (float yaw, float pitch, float dist, float lookY, float* m)
{
    const float ex = std::sin (yaw) * std::cos (pitch) * dist, ey = std::sin (pitch) * dist + lookY, ez = std::cos (yaw) * std::cos (pitch) * dist;
    float fx = -ex, fy = lookY - ey, fz = -ez;
    float l = std::sqrt (fx * fx + fy * fy + fz * fz); fx /= l; fy /= l; fz /= l;
    // s = f x up(0,1,0)
    float sx = -fz, sy = 0, sz = fx; l = std::sqrt (sx * sx + sz * sz); sx /= l; sz /= l;
    const float ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;
    const float M[16] = { sx, ux, -fx, 0,  sy, uy, -fy, 0,  sz, uz, -fz, 0,
                          -(sx * ex + sy * ey + sz * ez), -(ux * ex + uy * ey + uz * ez), (fx * ex + fy * ey + fz * ez), 1 };
    std::memcpy (m, M, sizeof M);
}

void Renderer::buildBokeh (const SoundFrame& sf, const Scene& sc, int w, int h)
{
    bokeh.clear();
    const float amt = sc[P::opBokeh];
    if (amt <= 0.001f) return;
    const float mn = (float) std::min (w, h);
    const float hx = 0.5f * (float) w / mn, hy = 0.5f * (float) h / mn;
    const float t = (float) sf.animTime;
    const float hue = sc.on (P::spOn) ? sc[P::spHue] : (sc.on (P::d3On) ? sc[P::d3Hue] : sc[P::bgHue]);
    for (int i = 0; i < 30; ++i)
    {
        const float a = hash1 ((unsigned) i * 3u + 1), b = hash1 ((unsigned) i * 7u + 2), c = hash1 ((unsigned) i * 11u + 3), d = hash1 ((unsigned) i * 13u + 4);
        const float x = (a * 2.f - 1.f) * hx * 1.05f + 0.06f * std::sin (t * (0.05f + 0.1f * c) + 6.28f * d);
        const float y = std::fmod ((b * 2.f - 1.f) * hy + t * 0.012f * (0.3f + c) + 10.f * hy, 2.f * hy * 1.1f) - hy * 1.1f;
        const float bv = sf.bands[(int) (c * 50.f)];
        const float size = (0.015f + 0.05f * d * d) * (1.f + 0.35f * bv);
        float r, g, bl; hsv2rgb (hue + (a - 0.5f) * 0.18f, 0.55f + 0.3f * b, 1.f, r, g, bl);
        const float k = amt * (0.03f + 0.14f * bv + 0.05f * sf.beat) * (0.6f + 0.8f * (1.f - d));
        bokeh.push_back ({ x, y, 0.f, r * k, g * k, bl * k, size });
    }
}

void Renderer::render (const SoundFrame& sf, const Scene& sc, int W, int H, const TextState& ts)
{
    if (! ready) return;
    W = std::max (16, W); H = std::max (16, H);
    scene.ensure (W, H, true);
    out.ensure (W, H, false);
    lum.ensure (std::max (8, W / 20), std::max (8, H / 20), true);
    {
        int bw = W, bh = H;
        for (auto& b : bloom) { bw = std::max (2, bw / 2); bh = std::max (2, bh / 2); b.ensure (bw, bh, true); }
    }

    glDisable (GL_DEPTH_TEST); glDisable (GL_CULL_FACE); glDisable (GL_SCISSOR_TEST);
    glBindFramebuffer (GL_FRAMEBUFFER, scene.fbo);
    glViewport (0, 0, W, H);
    glDisable (GL_BLEND);
    glClearColor (0, 0, 0, 1); glClear (GL_COLOR_BUFFER_BIT);

    // --- background
    if (sc.on (P::bgOn))
    {
        auto& p = bg[juce::jlimit (0, 4, sc.choice (P::bgMode))];
        p.use(); setSound (p, sf, W, H);
        p.set ("uHue", sc[P::bgHue]); p.set ("uSat", sc[P::bgSat]); p.set ("uInt", sc[P::bgIntensity]);
        p.set ("uSpeed", sc[P::bgSpeed]); p.set ("uReact", sc[P::bgReact]);
        drawFullscreen();
    }

    auto drawVideo = [&] (bool framed)
    {
        if (! sc.on (P::vdOn) || ! hasVideo || (sc.choice (P::vdMode) == 1) != framed) return;
        glEnable (GL_BLEND); glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        video.use();
        glActiveTexture (GL_TEXTURE0); glBindTexture (GL_TEXTURE_2D, videoTex);
        video.set ("uTex", 0);
        video.set ("uRes", (float) W, (float) H); video.set ("uVidSize", (float) videoW, (float) videoH);
        video.set ("uFramed", framed ? 1.f : 0.f); video.set ("uOpacity", sc[P::vdOpacity]);
        video.set ("uBright", sc[P::vdBright]); video.set ("uContrast", sc[P::vdContrast]); video.set ("uSat", sc[P::vdSat]);
        video.set ("uGrain", sc[P::vdGrain]); video.set ("uScan", sc[P::vdScan]); video.set ("uAberr", sc[P::vdAberr] * (1.f + 2.f * sf.beat));
        video.set ("uTime", (float) std::fmod (sf.animTime, 1000.0)); video.set ("uBeat", sf.beat);
        drawFullscreen();
    };
    drawVideo (false);

    glEnable (GL_BLEND); glBlendFunc (GL_ONE, GL_ONE);

    // --- ornament
    if (sc.on (P::orOn))
    {
        const unsigned tex = ornamentTexture (sc.choice (P::orMode));
        ornament.use();
        glActiveTexture (GL_TEXTURE0); glBindTexture (GL_TEXTURE_2D, tex);
        ornament.set ("uMask", 0);
        ornament.set ("uRes", (float) W, (float) H);
        ornament.set ("uAngle", (float) std::fmod (sf.animTime * sc[P::orSpin] * 0.5, 6.283185307179586));
        ornament.set ("uScale", 0.42f * sc[P::orSize] * (1.f + 0.08f * sf.bass * sc[P::orReact] + 0.05f * sf.beat * sc[P::orReact]));
        ornament.set ("uHue", sc[P::orHue]); ornament.set ("uSat", sc[P::orSat]);
        ornament.set ("uGlow", 0.35f + 1.1f * sf.level * sc[P::orReact]);
        ornament.set ("uBright", 0.8f + 0.8f * sf.mid * sc[P::orReact]);
        drawFullscreen();
    }

    // --- spectrum
    if (sc.on (P::spOn))
    {
        auto& p = sp[juce::jlimit (0, 2, sc.choice (P::spMode))];
        p.use(); setSound (p, sf, W, H);
        p.set ("uHue", sc[P::spHue]); p.set ("uSat", sc[P::spSat]); p.set ("uGain", sc[P::spGain]);
        p.set ("uSize", sc[P::spSize]); p.set ("uThick", sc[P::spThick]);
        drawFullscreen();
    }

    // --- 3D
    if (sc.on (P::d3On))
    {
        sim.update (sf, sc);
        float view[16]; lookAt (sim.camYaw, sim.camPitch, sim.camDist, sim.camLookY, view);
        drawParticles (sim.verts, false, view, sim.camDist, 1.f, false, W, H);
    }

    // --- bokeh orbs
    buildBokeh (sf, sc, W, H);
    drawParticles (bokeh, true, nullptr, 1.f, 1.f, true, W, H);

    drawVideo (true);

    // --- text
    if (sc.on (P::txOn) && ts.visible)
    {
        // backing panel follows scene brightness
        if (sc[P::txPanel] > 0.001f)
        {
            glBindFramebuffer (GL_READ_FRAMEBUFFER, scene.fbo);
            glBindFramebuffer (GL_DRAW_FRAMEBUFFER, lum.fbo);
            glBlitFramebuffer (0, 0, W, H, 0, 0, lum.w, lum.h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBindFramebuffer (GL_FRAMEBUFFER, scene.fbo);
            glViewport (0, 0, W, H);
            glEnable (GL_BLEND); glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
            panel.use();
            glActiveTexture (GL_TEXTURE0); glBindTexture (GL_TEXTURE_2D, lum.tex);
            panel.set ("uLum", 0); panel.set ("uRes", (float) W, (float) H);
            panel.set ("uBox", ts.box[0], ts.box[1], ts.box[2], ts.box[3]);
            panel.set ("uAmount", sc[P::txPanel]);
            drawFullscreen();
        }
        glEnable (GL_BLEND); glBlendFunc (GL_ONE, GL_ONE);
        text.update (sf, sc, ts.litWord);
        drawParticles (text.verts, true, nullptr, 1.f, 1.f, false, W, H);
    }

    // --- bloom
    glDisable (GL_BLEND);
    {
        glBindFramebuffer (GL_FRAMEBUFFER, bloom[0].fbo);
        glViewport (0, 0, bloom[0].w, bloom[0].h);
        bright.use(); glActiveTexture (GL_TEXTURE0); glBindTexture (GL_TEXTURE_2D, scene.tex);
        bright.set ("uTex", 0); bright.set ("uTexel", 0.5f / (float) W, 0.5f / (float) H);
        drawFullscreen();
        down.use(); down.set ("uTex", 0);
        for (int i = 1; i < 6; ++i)
        {
            glBindFramebuffer (GL_FRAMEBUFFER, bloom[i].fbo);
            glViewport (0, 0, bloom[i].w, bloom[i].h);
            glBindTexture (GL_TEXTURE_2D, bloom[i - 1].tex);
            down.set ("uTexel", 1.f / (float) bloom[i - 1].w, 1.f / (float) bloom[i - 1].h);
            drawFullscreen();
        }
        glEnable (GL_BLEND); glBlendFunc (GL_ONE, GL_ONE);
        up.use(); up.set ("uTex", 0); up.set ("uWeight", 1.0f);
        for (int i = 5; i > 0; --i)
        {
            glBindFramebuffer (GL_FRAMEBUFFER, bloom[i - 1].fbo);
            glViewport (0, 0, bloom[i - 1].w, bloom[i - 1].h);
            glBindTexture (GL_TEXTURE_2D, bloom[i].tex);
            up.set ("uTexel", 1.f / (float) bloom[i].w, 1.f / (float) bloom[i].h);
            drawFullscreen();
        }
        glDisable (GL_BLEND);
    }

    // --- composite
    glBindFramebuffer (GL_FRAMEBUFFER, out.fbo);
    glViewport (0, 0, W, H);
    composite.use();
    glActiveTexture (GL_TEXTURE0); glBindTexture (GL_TEXTURE_2D, scene.tex);
    glActiveTexture (GL_TEXTURE1); glBindTexture (GL_TEXTURE_2D, bloom[0].tex);
    composite.set ("uScene", 0); composite.set ("uBloom", 1);
    composite.set ("uRes", (float) W, (float) H);
    composite.set ("uBloomAmt", sc[P::opBloom] * 0.5f);
    composite.set ("uChroma", sc[P::opChroma] * (1.f + 0.8f * sf.beat));
    composite.set ("uVignette", sc[P::opVignette]);
    composite.set ("uExposure", sc[P::opExposure]);
    composite.set ("uTime", (float) std::fmod (sf.animTime, 1000.0));
    drawFullscreen();
    glActiveTexture (GL_TEXTURE0);
}

void Renderer::drawToScreen (int viewW, int viewH, int x, int y, int w, int h)
{
    if (! ready || ! out.tex) return;
    glViewport (0, 0, viewW, viewH);
    glDisable (GL_BLEND);
    blit.use();
    glActiveTexture (GL_TEXTURE0); glBindTexture (GL_TEXTURE_2D, out.tex);
    blit.set ("uTex", 0);
    blit.set ("uRect", (float) x, (float) y, (float) w, (float) h);
    drawFullscreen();
}

void Renderer::readPixelsBGRA (uint8_t* dst)
{
    if (! out.fbo) return;
    glBindFramebuffer (GL_FRAMEBUFFER, out.fbo);
    glPixelStorei (GL_PACK_ALIGNMENT, 4);
    const size_t row = (size_t) out.w * 4;
    flipBuf.resize (row * (size_t) out.h);
    glReadPixels (0, 0, out.w, out.h, GL_BGRA, GL_UNSIGNED_BYTE, flipBuf.data());
    for (int yy = 0; yy < out.h; ++yy)
        std::memcpy (dst + (size_t) yy * row, flipBuf.data() + (size_t) (out.h - 1 - yy) * row, row);
}
}
