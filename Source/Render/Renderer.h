#pragma once
#include <juce_opengl/juce_opengl.h>
#include "ParticleSims.h"
#include "../Core/Params.h"
#include "../Core/SoundFrame.h"
#include <map>
#include <memory>

namespace au
{
class GlProgram
{
public:
    bool build (const char* vs, const std::string& fs, const std::vector<std::pair<int, const char*>>& attribs, juce::String& err);
    void use() const;
    int loc (const char* name);
    void set (const char* n, float v);
    void set (const char* n, float a, float b);
    void set (const char* n, float a, float b, float c, float d);
    void set (const char* n, int v);
    void setArray (const char* n, const float* v, int count);
    void setMat4 (const char* n, const float* m);
    void release();
    ~GlProgram() = default;
    unsigned id = 0;
private:
    std::map<std::string, int> cache;
};

struct Fbo
{
    unsigned fbo = 0, tex = 0; int w = 0, h = 0; bool hdr = true;
    void ensure (int width, int height, bool floatFormat);
    void release();
};

struct TextState
{
    bool visible = false;
    int litWord = -1;
    float box[4] { 0, 0, 0, 0 };   // minX, minY, maxX, maxY in short-side units
};

// Renders one frame of the scene at any resolution into an offscreen buffer.
// Must be used on a thread with a current GL context (OpenGLContext render callback).
class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool init (juce::String& error);
    void release();
    bool isReady() const { return ready; }

    void render (const SoundFrame& sf, const Scene& sc, int width, int height, const TextState& ts);

    // Draws the last rendered frame into the currently bound framebuffer (viewport pixels, GL origin bottom-left).
    void drawToScreen (int viewW, int viewH, int x, int y, int w, int h);
    // Copies the last frame as top-down BGRA. dst must hold w*h*4 bytes.
    void readPixelsBGRA (uint8_t* dst);

    void setVideoFrame (const uint8_t* bgraTopDown, int w, int h);
    void clearVideo() { hasVideo = false; }

    Sim3D sim;
    TextSim text;

    int outWidth() const { return out.w; }
    int outHeight() const { return out.h; }

private:
    void drawFullscreen();
    void setSound (GlProgram& p, const SoundFrame& sf, int w, int h);
    void drawParticles (const std::vector<Vtx>& v, bool twoD, const float* view, float camDist, float sizeMul, bool soft, int w, int h);
    unsigned ornamentTexture (int motif);
    void buildBokeh (const SoundFrame& sf, const Scene& sc, int w, int h);

    bool ready = false;
    GlProgram bg[5], sp[3], ornament, particles, video, panel, bright, down, up, composite, blit;
    Fbo scene, lum, out;
    Fbo bloom[6];
    unsigned emptyVao = 0, particleVao = 0, particleVbo = 0;
    size_t particleCapacity = 0;
    unsigned ornTex[3] { 0, 0, 0 };
    unsigned videoTex = 0; int videoW = 0, videoH = 0; bool hasVideo = false;
    std::vector<Vtx> bokeh;
    std::vector<uint8_t> flipBuf;
};
}
