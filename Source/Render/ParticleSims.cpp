#include "ParticleSims.h"
#include <algorithm>
#include <cmath>

namespace au
{
namespace
{
constexpr float PI = 3.14159265358979f;
inline float clamp01 (float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

struct V3 { float x, y, z; };
inline V3 fibonacci (int i, int n)
{
    const float y = 1.f - 2.f * ((float) i + 0.5f) / (float) n;
    const float r = std::sqrt (std::max (0.f, 1.f - y * y));
    const float phi = (float) i * 2.39996323f;
    return { std::cos (phi) * r, y, std::sin (phi) * r };
}
inline V3 rotateAxis (V3 p, V3 k, float a)
{
    const float c = std::cos (a), s = std::sin (a);
    const float d = k.x * p.x + k.y * p.y + k.z * p.z;
    return { p.x * c + (k.y * p.z - k.z * p.y) * s + k.x * d * (1 - c),
             p.y * c + (k.z * p.x - k.x * p.z) * s + k.y * d * (1 - c),
             p.z * c + (k.x * p.y - k.y * p.x) * s + k.z * d * (1 - c) };
}
inline float fract (float x) { return x - std::floor (x); }
}

float hash1 (unsigned n)
{
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return (float) (n & 0x7fffffffu) / (float) 0x7fffffff;
}

void hsv2rgb (float h, float s, float v, float& r, float& g, float& b)
{
    h = fract (h);
    const float kx = std::fabs (fract (h + 0.f) * 6.f - 3.f), ky = std::fabs (fract (h + 2.f / 3.f) * 6.f - 3.f), kz = std::fabs (fract (h + 1.f / 3.f) * 6.f - 3.f);
    r = v * (1.f + s * (clamp01 (kx - 1.f) - 1.f));
    g = v * (1.f + s * (clamp01 (ky - 1.f) - 1.f));
    b = v * (1.f + s * (clamp01 (kz - 1.f) - 1.f));
}

// ============================================================================ Sim3D
void Sim3D::rebuild (int mode, int attr, int count)
{
    curMode = mode; curAttr = attr; curCount = count;
    ps.assign ((size_t) count, {});
    hist.clear(); histAcc = 0; shard.clear();

    if (mode == 1) // curl: random inside a sphere
    {
        for (int i = 0; i < count; ++i)
        {
            const V3 d = fibonacci (i, count); const float r = 1.4f * std::cbrt (hash1 ((unsigned) i * 7u + 1));
            ps[(size_t) i] = { d.x * r, d.y * r, d.z * r, hash1 ((unsigned) i * 13u + 5), 0 };
        }
    }
    else if (mode == 2) // attractor: seed near the basin and burn in
    {
        for (int i = 0; i < count; ++i)
        {
            Particle p { 0.1f + 0.6f * hash1 ((unsigned) i * 3u + 1), 0.1f + 0.6f * hash1 ((unsigned) i * 5u + 2),
                         0.1f + 0.6f * hash1 ((unsigned) i * 11u + 3), hash1 ((unsigned) i * 17u + 4), 0 };
            const int burn = 300 + (int) (hash1 ((unsigned) i * 19u + 7) * 4000.f);
            for (int s = 0; s < burn; ++s)
            {
                float dx, dy, dz, dt;
                if (attr == 0) { dx = 10.f * (p.y - p.x); dy = p.x * (28.f - p.z) - p.y; dz = p.x * p.y - 8.f / 3.f * p.z; dt = 0.006f; }
                else if (attr == 1)
                {
                    const float a = .95f, b = .7f, c = .6f, d = 3.5f, e = .25f, f = .1f;
                    dx = (p.z - b) * p.x - d * p.y; dy = d * p.x + (p.z - b) * p.y;
                    dz = c + a * p.z - p.z * p.z * p.z / 3.f - (p.x * p.x + p.y * p.y) * (1.f + e * p.z) + f * p.z * p.x * p.x * p.x; dt = 0.012f;
                }
                else { const float b = .208186f; dx = std::sin (p.y) - b * p.x; dy = std::sin (p.z) - b * p.y; dz = std::sin (p.x) - b * p.z; dt = 0.12f; }
                p.x += dx * dt; p.y += dy * dt; p.z += dz * dt;
            }
            ps[(size_t) i] = p;
        }
    }
    else if (mode == 4) // shatter: shard id = nearest of 24 centres
    {
        shard.resize ((size_t) count);
        for (int i = 0; i < count; ++i)
        {
            const V3 d = fibonacci (i, count); float best = -2; int bi = 0;
            for (int k = 0; k < 24; ++k)
            {
                const V3 c = fibonacci (k, 24); const float dd = d.x * c.x + d.y * c.y + d.z * c.z;
                if (dd > best) { best = dd; bi = k; }
            }
            shard[(size_t) i] = (unsigned char) bi;
        }
    }
}

void Sim3D::update (const SoundFrame& sf, const Scene& sc)
{
    const int mode = sc.choice (P::d3Mode), attr = sc.choice (P::d3Attr);
    const int count = mode == 3 ? 0 : (int) (3000.f + sc[P::d3Count] * 40000.f);
    if (mode != curMode || count != curCount || (mode == 2 && attr != curAttr)) rebuild (mode, attr, count);

    const float orbit = sc[P::d3Cam];
    camYaw = (float) sf.animTime * 0.22f * orbit + sf.beat * 0.03f * sc[P::d3React];
    camPitch = 0.30f + 0.12f * std::sin ((float) sf.animTime * 0.17f * orbit);
    camDist = 3.4f - 0.25f * sf.bass * sc[P::d3React];
    camLookY = 0.f;

    verts.clear();
    switch (mode)
    {
        case 0: updateMorph (sf, sc); break;
        case 1: updateCurl (sf, sc); break;
        case 2: updateAttractor (sf, sc); break;
        case 3: updateTerrain (sf, sc); break;
        default: updateShatter (sf, sc); break;
    }
}

void Sim3D::updateMorph (const SoundFrame& sf, const Scene& sc)
{
    const int n = curCount; verts.reserve ((size_t) n);
    const float react = sc[P::d3React], hue = sc[P::d3Hue], sat = sc[P::d3Sat];
    const float m = (float) sf.animTime * 0.16f * sc[P::d3Speed] + 0.01f;
    const int k = (int) std::floor (m); float f = m - (float) k; f = f * f * (3.f - 2.f * f);
    const float pump = 1.f + 0.22f * sf.bass * react + 0.12f * sf.beat * react;

    auto shape = [] (int id, int i, int n_, float u, float v) -> V3
    {
        switch (((id % 4) + 4) % 4)
        {
            case 0: { V3 d = fibonacci (i, n_); return { d.x * 1.0f, d.y * 1.0f, d.z * 1.0f }; }
            case 1: { const float a = (float) i * 2.39996323f, b = v * 2.f * PI; const float R = 0.78f, r = 0.3f;
                      return { (R + r * std::cos (b)) * std::cos (a), r * std::sin (b), (R + r * std::cos (b)) * std::sin (a) }; }
            case 2: { V3 d = fibonacci (i, n_); const float mx = std::max ({ std::fabs (d.x), std::fabs (d.y), std::fabs (d.z) });
                      const float s = 0.78f / mx; return { d.x * s, d.y * s, d.z * s }; }
            default: { const float t = u * 6.f * PI * 2.f, w = 0.35f + 0.55f * std::sin (u * PI);
                       return { std::cos (t) * w, (u - 0.5f) * 2.1f, std::sin (t) * w }; }
        }
    };

    for (int i = 0; i < n; ++i)
    {
        const float u = (float) i / (float) n, v = hash1 ((unsigned) i * 31u + 9), w = hash1 ((unsigned) i * 57u + 3);
        const V3 a = shape (k, i, n, u, v), b = shape (k + 1, i, n, u, v);
        V3 p { a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f };
        const float len = std::sqrt (p.x * p.x + p.y * p.y + p.z * p.z) + 1e-5f;
        const float band = sf.bands[(int) (w * 63.f)];
        const float disp = band * 0.42f * react * (0.4f + 0.6f * v);
        p.x = p.x * pump + p.x / len * disp; p.y = p.y * pump + p.y / len * disp; p.z = p.z * pump + p.z / len * disp;
        float r, g, bl; hsv2rgb (hue + (w - 0.5f) * 0.22f, sat, 0.45f + 0.9f * band + 0.2f * sf.beat, r, g, bl);
        verts.push_back ({ p.x, p.y, p.z, r, g, bl, 0.0035f * sc[P::d3Size] * (0.7f + 0.6f * v) });
    }
}

void Sim3D::updateCurl (const SoundFrame& sf, const Scene& sc)
{
    const float t = (float) sf.animTime, react = sc[P::d3React];
    const float speed = sc[P::d3Speed] * (0.45f + 0.9f * sf.level + 1.2f * sf.bass * react);
    const float dt = (float) std::min (sf.dt, 0.1) * speed;
    verts.reserve (ps.size());
    for (size_t i = 0; i < ps.size(); ++i)
    {
        auto& p = ps[i];
        const float k1 = 1.7f, k2 = 1.3f, k3 = 1.9f, k4 = 1.1f, k5 = 1.5f, k6 = 1.2f;
        // psi = (sin(k1 y + .6t) cos(k2 z), sin(k3 z + .5t) cos(k4 x), sin(k5 x + .4t) cos(k6 y)); v = curl(psi)
        const float vx = -k6 * std::sin (k5 * p.x + 0.4f * t) * std::sin (k6 * p.y) - k3 * std::cos (k3 * p.z + 0.5f * t) * std::cos (k4 * p.x);
        const float vy = -k2 * std::sin (k1 * p.y + 0.6f * t) * std::sin (k2 * p.z) - k5 * std::cos (k5 * p.x + 0.4f * t) * std::cos (k6 * p.y);
        const float vz = -k4 * std::sin (k3 * p.z + 0.5f * t) * std::sin (k4 * p.x) - k1 * std::cos (k1 * p.y + 0.6f * t) * std::cos (k2 * p.z);
        p.x += vx * dt; p.y += vy * dt; p.z += vz * dt;
        const float r2 = p.x * p.x + p.y * p.y + p.z * p.z;
        if (r2 > 2.6f * 2.6f || ! std::isfinite (r2))
        {
            ++p.respawn;
            const V3 d = fibonacci ((int) (hash1 ((unsigned) i * 977u + p.respawn * 131u) * 100000.f), 100000);
            const float r = 1.0f * std::cbrt (hash1 ((unsigned) i * 31u + p.respawn * 17u));
            p.x = d.x * r; p.y = d.y * r; p.z = d.z * r;
        }
        const float sp = std::sqrt (vx * vx + vy * vy + vz * vz);
        float r, g, b; hsv2rgb (sc[P::d3Hue] + 0.18f * clamp01 (sp * 0.35f) + 0.1f * (p.seed - 0.5f), sc[P::d3Sat], 0.35f + 0.7f * clamp01 (sp * 0.45f) + 0.25f * sf.beat, r, g, b);
        verts.push_back ({ p.x, p.y, p.z, r, g, b, 0.0028f * sc[P::d3Size] * (0.6f + 0.8f * p.seed) });
    }
}

void Sim3D::updateAttractor (const SoundFrame& sf, const Scene& sc)
{
    const float react = sc[P::d3React];
    const int attr = curAttr;
    const float speed = sc[P::d3Speed] * (0.6f + 0.9f * sf.level + 1.0f * sf.bass * react);
    float dtBase, scale, cy;
    if (attr == 0) { dtBase = 0.0035f; scale = 0.048f; cy = 24.f; }
    else if (attr == 1) { dtBase = 0.0075f; scale = 0.95f; cy = 0.5f; }
    else { dtBase = 0.075f; scale = 0.42f; cy = 0.f; }
    const int steps120 = std::min (24, (int) std::lround (sf.dt * 120.0));
    const int sub = std::max (1, steps120);            // always evaluate the field (it drives the colour)
    const float dt = steps120 > 0 ? dtBase * speed : 0.f;
    const float pump = 1.f + 0.10f * sf.beat * react;

    verts.reserve (ps.size());
    for (size_t i = 0; i < ps.size(); ++i)
    {
        auto& p = ps[i]; float dx = 0, dy = 0, dz = 0;
        for (int s = 0; s < sub; ++s)
        {
            if (attr == 0) { dx = 10.f * (p.y - p.x); dy = p.x * (28.f - p.z) - p.y; dz = p.x * p.y - 8.f / 3.f * p.z; }
            else if (attr == 1)
            {
                const float a = .95f, b = .7f, c = .6f, d = 3.5f, e = .25f, f = .1f;
                dx = (p.z - b) * p.x - d * p.y; dy = d * p.x + (p.z - b) * p.y;
                dz = c + a * p.z - p.z * p.z * p.z / 3.f - (p.x * p.x + p.y * p.y) * (1.f + e * p.z) + f * p.z * p.x * p.x * p.x;
            }
            else { const float b = .208186f; dx = std::sin (p.y) - b * p.x; dy = std::sin (p.z) - b * p.y; dz = std::sin (p.x) - b * p.z; }
            p.x += dx * dt; p.y += dy * dt; p.z += dz * dt;
        }
        if (! std::isfinite (p.x + p.y + p.z) || std::fabs (p.x) + std::fabs (p.y) + std::fabs (p.z) > 400.f)
        { ++p.respawn; p.x = 0.1f + hash1 ((unsigned) i + p.respawn * 7u) * 0.5f; p.y = 0.2f; p.z = 0.3f; }

        const float sp = std::sqrt (dx * dx + dy * dy + dz * dz);
        const float sn = attr == 0 ? clamp01 (sp / 90.f) : (attr == 1 ? clamp01 (sp / 6.f) : clamp01 (sp / 1.4f));
        float r, g, b; hsv2rgb (sc[P::d3Hue] + 0.16f * sn + 0.06f * (p.seed - 0.5f), sc[P::d3Sat], 0.4f + 0.75f * sn + 0.3f * sf.beat, r, g, b);
        V3 w;
        if (attr == 0) w = { p.x * scale, (p.z - cy) * scale, p.y * scale };
        else if (attr == 1) w = { p.x * scale, (p.z - cy) * scale * 1.1f, p.y * scale };
        else w = { p.x * scale, p.z * scale, p.y * scale };
        verts.push_back ({ w.x * pump, w.y * pump, w.z * pump, r, g, b, 0.0026f * sc[P::d3Size] * (0.6f + 0.8f * p.seed) });
    }
}

void Sim3D::updateTerrain (const SoundFrame& sf, const Scene& sc)
{
    constexpr int rows = 44, cols = 72;
    const float react = sc[P::d3React];
    const double step = 1.0 / (14.0 * (0.4 + sc[P::d3Speed]));
    histAcc += sf.dt;
    if (hist.empty()) hist.assign (rows, {});
    int guard = 0;
    while (histAcc >= step && guard++ < 4)
    {
        histAcc -= step;
        std::rotate (hist.rbegin(), hist.rbegin() + 1, hist.rend());     // newest at index 0
        for (int b = 0; b < 64; ++b) hist[0][(size_t) b] = sf.bands[b];
    }
    if (histAcc > step) histAcc = 0;

    camPitch = 0.52f + 0.05f * std::sin ((float) sf.animTime * 0.2f); camYaw = 0.35f * std::sin ((float) sf.animTime * 0.11f * sc[P::d3Cam]);
    camDist = 3.5f; camLookY = -0.15f;

    verts.reserve ((size_t) rows * cols * 2);
    const float frac = (float) (histAcc / step);
    for (int r = 0; r < rows; ++r)
    {
        const float z = -1.4f + ((float) r + frac) / (float) rows * 3.0f * -1.f + 1.4f * 0.f; // r=0 near (z>0), older rows go back
        const float zz = 1.5f - ((float) r + frac) * (3.0f / rows);
        (void) z;
        const float fade = 1.f - (float) r / (float) rows;
        for (int c = 0; c < cols; ++c)
        {
            const float x = ((float) c / (cols - 1)) * 2.f - 1.f;
            const float u = std::fabs (x);
            const float fb = u * 63.f; const int i0 = (int) fb; const int i1 = std::min (63, i0 + 1);
            const float bv = hist[(size_t) r][(size_t) i0] * (1.f - (fb - (float) i0)) + hist[(size_t) r][(size_t) i1] * (fb - (float) i0);
            const float h = std::pow (clamp01 (bv), 1.4f) * 1.15f * (0.4f + 0.6f * react);
            float rr, g, b; hsv2rgb (sc[P::d3Hue] - 0.12f * (1.f - clamp01 (bv * 1.5f)) + 0.1f * clamp01 (bv), sc[P::d3Sat], (0.25f + 0.95f * clamp01 (bv * 1.3f)) * (0.25f + 0.75f * fade), rr, g, b);
            verts.push_back ({ x * 1.9f, h - 0.55f, zz, rr, g, b, 0.0045f * sc[P::d3Size] * (0.6f + 0.6f * fade) });
            if (h > 0.05f) // little pillar cap for the cube look
                verts.push_back ({ x * 1.9f, h * 0.5f - 0.55f, zz, rr * 0.5f, g * 0.5f, b * 0.5f, 0.0035f * sc[P::d3Size] * (0.6f + 0.6f * fade) });
        }
    }
}

void Sim3D::updateShatter (const SoundFrame& sf, const Scene& sc)
{
    const int n = curCount; verts.reserve ((size_t) n);
    const float react = sc[P::d3React], hue = sc[P::d3Hue], sat = sc[P::d3Sat];
    const float blast = sf.beat * react * 1.1f + sf.bass * 0.22f * react;
    const float spin = (float) sf.animTime * 0.3f * sc[P::d3Speed];
    for (int i = 0; i < n; ++i)
    {
        const int k = shard[(size_t) i];
        const V3 axis = fibonacci (k, 24);
        const float hk = hash1 ((unsigned) k * 73u + 11), hi = hash1 ((unsigned) i * 41u + 5);
        V3 d = fibonacci (i, n);
        const float band = sf.bands[(int) (std::fabs (d.y) * 63.f)];
        V3 p { d.x * (1.f + 0.12f * band * react), d.y * (1.f + 0.12f * band * react), d.z * (1.f + 0.12f * band * react) };
        p = rotateAxis (p, axis, blast * (hk - 0.5f) * 1.4f);
        const float push = blast * (0.55f + 0.7f * hk);
        p.x += axis.x * push + d.x * blast * 0.12f * hi; p.y += axis.y * push + d.y * blast * 0.12f * hi; p.z += axis.z * push + d.z * blast * 0.12f * hi;
        p = rotateAxis (p, { 0, 1, 0 }, spin);
        float r, g, b; hsv2rgb (hue + 0.16f * (hk - 0.5f) + 0.05f * hi, sat, 0.5f + 0.5f * clamp01 (blast) + 0.4f * band, r, g, b);
        verts.push_back ({ p.x, p.y, p.z, r, g, b, 0.0032f * sc[P::d3Size] * (0.7f + 0.6f * hi) });
    }
}

// ============================================================================ TextSim
void TextSim::setTargets (std::vector<TextTarget> t, unsigned seed, float dotSize, int appearMode)
{
    tg = std::move (t);
    dot = dotSize;
    mode = appearMode;
    age = 0.f;
    pt.resize (tg.size());
    for (size_t i = 0; i < tg.size(); ++i)
    {
        const float a = hash1 ((unsigned) i * 3u + seed * 101u) * 2.f * PI, r = 0.35f + 1.1f * hash1 ((unsigned) i * 5u + seed * 37u);
        pt[i] = mode == 0 ? Pt { tg[i].x + std::cos (a) * r, tg[i].y + std::sin (a) * r * 0.6f, 0.f, 0.f }
                          : Pt { tg[i].x, tg[i].y, 0.f, 0.f };
    }
}

void TextSim::update (const SoundFrame& sf, const Scene& sc, int litWord)
{
    verts.clear();
    if (tg.empty()) return;
    const float react = 0.8f, bright = sc[P::txBright];
    const float shake = (0.10f * sf.treble + 0.35f * sf.beat) * react;
    age += (float) sf.dt;
    float dt = (float) std::min (sf.dt, 0.1);
    const int steps = mode != 0 || dt <= 0.f ? 0 : std::min (8, (int) std::ceil (dt * 60.f - 1e-4f) * 2);
    dt /= (float) std::max (1, steps);
    for (int s = 0; s < steps; ++s)
    {
        ++frame;
        for (size_t i = 0; i < pt.size(); ++i)
        {
            auto& p = pt[i];
            const float nx = hash1 ((unsigned) i * 7919u + frame * 31u) - 0.5f, ny = hash1 ((unsigned) i * 104729u + frame * 17u) - 0.5f;
            const float ax = (tg[i].x - p.x) * 46.f - p.vx * 9.5f + nx * shake * 3.f;
            const float ay = (tg[i].y - p.y) * 46.f - p.vy * 9.5f + ny * shake * 3.f;
            p.vx += ax * dt; p.vy += ay * dt; p.x += p.vx * dt; p.y += p.vy * dt;
        }
    }
    verts.reserve (pt.size());
    float sr, sg, sb, ur, ug, ub;
    hsv2rgb (sc[P::txSungHue], 0.9f, 1.f, sr, sg, sb);
    hsv2rgb (sc[P::txHue], sc[P::txSat], 1.f, ur, ug, ub);
    for (size_t i = 0; i < pt.size(); ++i)
    {
        const bool sung = tg[i].word <= litWord && litWord >= 0;
        float k = sung ? (mode == 2 ? 1.0f : 1.0f + 0.5f * sf.beat) * bright : 0.72f * bright;
        if (mode == 1) { const float f = std::min (1.f, age / 0.35f); k *= f * f * (3.f - 2.f * f); }
        verts.push_back ({ pt[i].x, pt[i].y, 0.f, (sung ? sr : ur) * k, (sung ? sg : ug) * k, (sung ? sb : ub) * k, sung ? dot * 1.2f : dot });
    }
}
}
