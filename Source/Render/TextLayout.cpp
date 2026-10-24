#include "TextLayout.h"
#include "../Lyrics/Lyrics.h"
#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"
#include <cmath>

namespace au
{
namespace
{
juce::Typeface::Ptr typefaceFor (Script s)
{
    static juce::Typeface::Ptr latin  = juce::Typeface::createSystemTypefaceFor (BinaryData::NotoSansBold_ttf, (size_t) BinaryData::NotoSansBold_ttfSize);
    static juce::Typeface::Ptr arm    = juce::Typeface::createSystemTypefaceFor (BinaryData::NotoSansArmenianBold_ttf, (size_t) BinaryData::NotoSansArmenianBold_ttfSize);
    static juce::Typeface::Ptr geo    = juce::Typeface::createSystemTypefaceFor (BinaryData::NotoSansGeorgianBold_ttf, (size_t) BinaryData::NotoSansGeorgianBold_ttfSize);
    static juce::Typeface::Ptr arabic = juce::Typeface::createSystemTypefaceFor (BinaryData::NotoSansArabicBold_ttf, (size_t) BinaryData::NotoSansArabicBold_ttfSize);
    switch (s)
    {
        case Script::Armenian: return arm;
        case Script::Georgian: return geo;
        case Script::Arabic:   return arabic;
        default:               return latin;
    }
}

juce::Font fontFor (const std::string& w, float h)
{
    return juce::Font (juce::FontOptions (typefaceFor (dominantScript (w))).withHeight (h));
}

float widthOf (const std::string& w, float h)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (fontFor (w, h), juce::String::fromUTF8 (w.c_str()), 0.f, 0.f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}
}

TextLayoutResult layoutWords (const std::vector<std::string>& words, float frameW, float frameH, float sizeUnits, float posY, int maxTargets)
{
    TextLayoutResult res;
    if (words.empty()) return res;

    const float ppu = 520.f;                      // raster pixels per unit
    const float maxW = frameW * 0.86f * ppu;
    std::string all; for (auto& w : words) { all += w; all += ' '; }
    const bool rtl = isRightToLeft (all);

    float fontH = sizeUnits * 0.16f * ppu;
    std::vector<std::vector<int>> rows;
    std::vector<float> widths;
    for (int attempt = 0; attempt < 12; ++attempt)
    {
        widths.clear();
        for (auto& w : words) widths.push_back (widthOf (w, fontH));
        const float space = fontH * 0.28f;
        rows.clear(); rows.emplace_back();
        float cur = 0; bool tooWide = false;
        for (int i = 0; i < (int) words.size(); ++i)
        {
            if (widths[(size_t) i] > maxW) tooWide = true;
            const float add = (rows.back().empty() ? 0.f : space) + widths[(size_t) i];
            if (! rows.back().empty() && cur + add > maxW) { rows.emplace_back(); cur = 0; }
            cur += (rows.back().size() > 0 ? space : 0.f) + widths[(size_t) i];
            rows.back().push_back (i);
        }
        const float totalH = (float) rows.size() * fontH * 1.25f;
        if (! tooWide && rows.size() <= 3 && totalH < frameH * 0.7f * ppu) break;
        fontH *= 0.86f;
    }

    const float space = fontH * 0.28f, lineH = fontH * 1.25f;
    const float blockH = (float) rows.size() * lineH;

    // ink area estimate -> sampling step
    struct Placed { int word; float x, y; };
    std::vector<Placed> placed;
    for (size_t r = 0; r < rows.size(); ++r)
    {
        float rowW = 0;
        for (size_t k = 0; k < rows[r].size(); ++k) rowW += widths[(size_t) rows[r][k]] + (k ? space : 0.f);
        float x = -rowW * 0.5f;
        const float y = -blockH * 0.5f + (float) r * lineH;          // top of row, y down
        std::vector<int> order = rows[r];
        if (rtl) std::reverse (order.begin(), order.end());
        for (int wi : order) { placed.push_back ({ wi, x, y }); x += widths[(size_t) wi] + space; }
    }

    struct Raster { int word; float x0, y0; juce::Image img; };
    std::vector<Raster> rasters; double ink = 0;
    for (auto& p : placed)
    {
        const int iw = (int) std::ceil (widths[(size_t) p.word] + fontH * 0.4f), ih = (int) std::ceil (lineH * 1.1f);
        juce::Image img (juce::Image::SingleChannel, std::max (1, iw), std::max (1, ih), true, juce::SoftwareImageType());
        {
            juce::Graphics g (img);
            g.setColour (juce::Colours::white);
            juce::GlyphArrangement ga;
            ga.addLineOfText (fontFor (words[(size_t) p.word], fontH), juce::String::fromUTF8 (words[(size_t) p.word].c_str()), fontH * 0.2f, fontH * 0.95f);
            ga.draw (g);
        }
        const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < bd.height; ++y)
        {
            const uint8_t* row = bd.getLinePointer (y);
            for (int x = 0; x < bd.width; ++x) ink += row[x] / 255.0;
        }
        rasters.push_back ({ p.word, p.x - fontH * 0.2f, p.y, std::move (img) });
    }
    if (ink < 1) return res;

    const float step = std::max (1.0f, (float) std::sqrt (ink / (double) maxTargets));
    res.dotSize = std::max (0.0022f, 0.62f * step / ppu);
    res.box[0] = res.box[1] = 1e9f; res.box[2] = res.box[3] = -1e9f;
    unsigned n = 0;
    for (auto& r : rasters)
    {
        const juce::Image::BitmapData bd (r.img, juce::Image::BitmapData::readOnly);
        for (float yy = step * 0.5f; yy < (float) bd.height; yy += step)
            for (float xx = step * 0.5f; xx < (float) bd.width; xx += step)
            {
                ++n;
                const float jx = (hash1 (n * 2u + 1u) - 0.5f) * step * 0.7f, jy = (hash1 (n * 2u + 2u) - 0.5f) * step * 0.7f;
                const int sx = juce::jlimit (0, bd.width - 1, (int) (xx + jx)), sy = juce::jlimit (0, bd.height - 1, (int) (yy + jy));
                if (bd.getLinePointer (sy)[sx] < 110) continue;
                const float ux = (r.x0 + (float) sx) / ppu, uy = -((r.y0 + (float) sy) / ppu) + posY;
                res.targets.push_back ({ ux, uy, r.word });
                res.box[0] = std::min (res.box[0], ux); res.box[2] = std::max (res.box[2], ux);
                res.box[1] = std::min (res.box[1], uy); res.box[3] = std::max (res.box[3], uy);
            }
    }
    return res;
}
}
