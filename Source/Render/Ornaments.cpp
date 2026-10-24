#include "Ornaments.h"
#include <juce_graphics/juce_graphics.h>
#include <cmath>

namespace au
{
namespace
{
constexpr float PI = juce::MathConstants<float>::pi;

// Curved blade swirling outward from the centre.
juce::Path spiralArm (float angle0, float twist, float rIn, float rOut, float width, bool bulb)
{
    juce::Path p;
    const int steps = 48;
    std::vector<juce::Point<float>> outer, inner;
    for (int i = 0; i <= steps; ++i)
    {
        const float u = (float) i / steps;
        const float r = rIn + (rOut - rIn) * u;
        const float a = angle0 + twist * u * u * 1.15f + twist * 0.25f * u;
        const float w = width * std::sin (PI * std::pow (u, 0.75f)) * (bulb ? (0.65f + 0.6f * u) : 1.f);
        const float da = w / std::max (r, 0.05f);
        outer.push_back ({ r * std::cos (a + da), r * std::sin (a + da) });
        inner.push_back ({ r * std::cos (a - da * 0.4f), r * std::sin (a - da * 0.4f) });
    }
    p.startNewSubPath (outer.front());
    for (auto& pt : outer) p.lineTo (pt);
    for (auto it = inner.rbegin(); it != inner.rend(); ++it) p.lineTo (*it);
    p.closeSubPath();
    if (bulb)
    {
        const auto end = outer.back();
        const float rr = width * 0.85f;
        p.addEllipse (end.x - rr, end.y - rr, rr * 2, rr * 2);
    }
    return p;
}

juce::Path wheel (int arms, float twist, float rOut, float width, bool bulb, float centre)
{
    juce::Path p;
    for (int k = 0; k < arms; ++k)
        p.addPath (spiralArm ((float) k * 2.f * PI / (float) arms, twist, 0.05f, rOut, width, bulb));
    p.addEllipse (-centre, -centre, centre * 2, centre * 2);
    return p;
}

void drawArevakhach (juce::Graphics& g)
{
    g.fillPath (wheel (8, 2.2f, 0.82f, 0.16f, false, 0.09f));
    g.strokePath ([] { juce::Path c; c.addEllipse (-0.93f, -0.93f, 1.86f, 1.86f); return c; }(), juce::PathStrokeType (0.035f));
}

void drawBorjgali (juce::Graphics& g)
{
    g.fillPath (wheel (7, -2.6f, 0.74f, 0.17f, true, 0.12f));
    g.strokePath ([] { juce::Path c; c.addEllipse (-0.95f, -0.95f, 1.9f, 1.9f); return c; }(), juce::PathStrokeType (0.025f));
    for (int k = 0; k < 7; ++k)
    {
        const float a = (float) k * 2.f * PI / 7.f + 0.45f;
        g.fillEllipse (0.86f * std::cos (a) - 0.035f, 0.86f * std::sin (a) - 0.035f, 0.07f, 0.07f);
    }
}

void flaredArm (juce::Path& p, juce::Point<float> dir, float len, float w)
{
    const juce::Point<float> n (-dir.y, dir.x);
    const auto base = dir * 0.08f;
    const auto tip = dir * len;
    const float flare = w * 2.4f;
    p.startNewSubPath (base + n * w);
    p.lineTo (tip - dir * (flare * 0.9f) + n * w);
    p.quadraticTo (tip - dir * (flare * 0.4f) + n * w, tip + n * flare);
    p.lineTo (tip + n * (flare * 0.35f) - dir * (w * 0.3f));
    p.lineTo (tip - n * (flare * 0.35f) - dir * (w * 0.3f));
    p.lineTo (tip - n * flare);
    p.quadraticTo (tip - dir * (flare * 0.4f) - n * w, tip - dir * (flare * 0.9f) - n * w);
    p.lineTo (base - n * w);
    p.closeSubPath();
    // split trefoil tips
    const float r = w * 0.75f;
    for (float s : { -1.f, 1.f })
    {
        const auto c = tip + n * (flare * 0.95f * s) + dir * (r * 0.4f);
        p.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
    }
}

void drawKhachkar (juce::Graphics& g)
{
    // stone frame with an interlaced border band
    const juce::Rectangle<float> stone (-0.62f, -0.95f, 1.24f, 1.9f);
    g.strokePath ([&] { juce::Path p; p.addRoundedRectangle (stone, 0.04f); return p; }(), juce::PathStrokeType (0.03f));
    g.strokePath ([&] { juce::Path p; p.addRectangle (stone.reduced (0.13f)); return p; }(), juce::PathStrokeType (0.02f));
    const float band = 0.065f;
    auto knotRow = [&] (juce::Point<float> a, juce::Point<float> b, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const auto c = a + (b - a) * (((float) i + 0.5f) / (float) n);
            juce::Path k; k.addEllipse (c.x - band * 0.75f, c.y - band * 0.75f, band * 1.5f, band * 1.5f);
            g.strokePath (k, juce::PathStrokeType (0.012f));
            juce::Path x; x.startNewSubPath (c.x - band * 0.55f, c.y - band * 0.55f); x.lineTo (c.x + band * 0.55f, c.y + band * 0.55f);
            x.startNewSubPath (c.x - band * 0.55f, c.y + band * 0.55f); x.lineTo (c.x + band * 0.55f, c.y - band * 0.55f);
            g.strokePath (x, juce::PathStrokeType (0.01f));
        }
    };
    const auto in = stone.reduced (0.065f);
    knotRow (in.getTopLeft(), in.getTopRight(), 10);
    knotRow (in.getBottomLeft(), in.getBottomRight(), 10);
    knotRow (in.getTopLeft(), in.getBottomLeft(), 15);
    knotRow (in.getTopRight(), in.getBottomRight(), 15);

    // cross (JUCE y axis points down: up = negative y)
    juce::Path cross;
    const juce::Point<float> c (0.f, -0.18f);
    juce::Path arms;
    flaredArm (arms, { 0.f, -1.f }, 0.48f, 0.05f);
    flaredArm (arms, { 1.f, 0.f }, 0.36f, 0.05f);
    flaredArm (arms, { -1.f, 0.f }, 0.36f, 0.05f);
    flaredArm (arms, { 0.f, 1.f }, 0.5f, 0.05f);
    arms.applyTransform (juce::AffineTransform::translation (c.x, c.y));
    g.fillPath (arms);
    g.fillEllipse (c.x - 0.07f, c.y - 0.07f, 0.14f, 0.14f);

    // rosette below
    juce::Path ros = wheel (8, 2.f, 0.16f, 0.04f, false, 0.03f);
    ros.applyTransform (juce::AffineTransform::translation (0.f, 0.52f));
    g.fillPath (ros);
    // leaves sprouting from the base of the cross
    for (float s : { -1.f, 1.f })
    {
        juce::Path leaf;
        leaf.startNewSubPath (0.f, 0.28f);
        leaf.cubicTo (0.12f * s, 0.25f, 0.28f * s, 0.12f, 0.3f * s, -0.02f);
        leaf.cubicTo (0.22f * s, 0.1f, 0.1f * s, 0.22f, 0.f, 0.3f);
        g.fillPath (leaf);
    }
}
}

void makeOrnamentMask (int motif, int size, uint8_t* dst)
{
    juce::Image img (juce::Image::ARGB, size, size, true, juce::SoftwareImageType());
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale ((float) size * 0.5f).translated ((float) size * 0.5f, (float) size * 0.5f));
        g.setColour (juce::Colours::white);
        if (motif == 0) drawArevakhach (g);
        else if (motif == 1) drawKhachkar (g);
        else drawBorjgali (g);
    }
    const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            dst[(size_t) (size - 1 - y) * (size_t) size + (size_t) x] = bd.getPixelColour (x, y).getAlpha();
}
}
