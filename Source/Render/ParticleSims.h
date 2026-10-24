#pragma once
#include "../Core/Params.h"
#include "../Core/SoundFrame.h"
#include <array>
#include <vector>

namespace au
{
struct Vtx { float x, y, z, r, g, b, size; };   // size: fraction of the short side

void hsv2rgb (float h, float s, float v, float& r, float& g, float& b);

// 3D particle scenes. Morph and Shatter are stateless functions of (time, sound); Curl, Attractor
// and Terrain integrate with fixed steps, so a render started from frame 0 is repeatable.
class Sim3D
{
public:
    void update (const SoundFrame& sf, const Scene& sc);

    std::vector<Vtx> verts;
    float camYaw = 0, camPitch = 0.3f, camDist = 3.2f, camLookY = 0.f;

private:
    struct Particle { float x, y, z; float seed; unsigned respawn; };

    void rebuild (int mode, int attr, int count);
    void updateMorph (const SoundFrame&, const Scene&);
    void updateCurl (const SoundFrame&, const Scene&);
    void updateAttractor (const SoundFrame&, const Scene&);
    void updateTerrain (const SoundFrame&, const Scene&);
    void updateShatter (const SoundFrame&, const Scene&);

    int curMode = -1, curAttr = -1, curCount = -1;
    std::vector<Particle> ps;
    std::vector<unsigned char> shard;
    std::vector<std::array<float, 64>> hist;
    double histAcc = 0;
};

struct TextTarget { float x, y; int word; };

// Particles that spring toward glyph shapes.
class TextSim
{
public:
    // mode: 0 = particles fly in and shimmer, 1 = fade in in place, 2 = appear instantly (static)
    void setTargets (std::vector<TextTarget> t, unsigned seed, float dotSize = 0.0035f, int mode = 0);
    void clear() { tg.clear(); pt.clear(); verts.clear(); }
    void update (const SoundFrame& sf, const Scene& sc, int litWord);

    std::vector<Vtx> verts;
    bool hasTargets() const { return ! tg.empty(); }

private:
    struct Pt { float x, y, vx, vy; };
    std::vector<TextTarget> tg;
    std::vector<Pt> pt;
    unsigned frame = 0;
    float dot = 0.0035f;
    int mode = 0;
    float age = 0.f;
};

float hash1 (unsigned n);                 // 0..1
}
